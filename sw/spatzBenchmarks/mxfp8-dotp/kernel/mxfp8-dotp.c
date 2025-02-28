// Copyright 2022 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Max Wipfli <mwipfli@ethz.ch>

#include "mxfp8-dotp.h"

#define MXFP8_BLOCK_SIZE 32
#define E8M0_BIAS 127
#define FP32_BIAS 127

#define DEBUG

#ifdef DEBUG
void snrt_putchar(char c);

#define nibble_to_char(nibble) \
   ((nibble) < 10 ? ((nibble) + '0') : ((nibble) - 10 + 'a'))

static void dump_vreg(const char *regname, const char *reg, int len) {
  while (*regname) {
    snrt_putchar(*regname);
    regname++;
  }
  snrt_putchar(':');
  snrt_putchar(' ');

  for (int i = len - 1; i >= 0; i--) {
    unsigned char byte = reg[i];
    snrt_putchar(nibble_to_char(byte >> 4));
    snrt_putchar(nibble_to_char(byte & 0xf));
    if ((i & 0x3) == 0)
      snrt_putchar(' ');
  }
  snrt_putchar('\n');
}
#endif
#include <stdio.h>

// MXFP8 dot product
// - block size = 32 fixed
// - avl == LENGTH(a) == LENGTH(b) == 32 * LENGTH(a_scale) == 32 * LENGTH(b_scale)
// - element data format: FP8
// - scale data format: E8M0 (bias = 127, NaN not handled)
//
// REGISTER ALLOCATION:
// - v0 (LMUL=4): global accumulator
// - v4 (LMUL=4): block-local (unscaled accumulator)
float mxfp8_dotp_fp32(const char *a, const char *b, const char *a_scale, const char *b_scale, unsigned int avl) {
  unsigned int block_avl = avl / MXFP8_BLOCK_SIZE;
  unsigned int block_vl; // # of blocks handled in current iteration

#ifdef DEBUG
  char tmpreg[512 / 8 * 8];
  memset(&tmpreg, 0x55, 64);
#endif

  // Clear global accumulators
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(-1));
  asm volatile("vmv.v.i v0, 0");

  do {
    for (unsigned int i = 0; i < MXFP8_BLOCK_SIZE; i++) {
      asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(block_vl) : "r"(block_avl));
      asm volatile("vlse8.v v8, (%0), %1" :: "r"(a + i), "r"(MXFP8_BLOCK_SIZE * sizeof(char)));
      asm volatile("vlse8.v v9, (%0), %1" :: "r"(b + i), "r"(MXFP8_BLOCK_SIZE * sizeof(char)));
      // v8, v9 (LMUL=1): a and b as FP8

      printf("loaded\n");

      // upcast from FP8 to FP16
      asm volatile("vmv.v.i v10, 0");
      asm volatile("vfwadd.vv v12, v8, v10");
      asm volatile("vfwadd.vv v14, v9, v10");
      // v12, v14 (LMUL=2): a and b as FP16

      // printf("upcast 1\n");

      // upcast from FP16 to FP32
      asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(block_avl));
      asm volatile("vmv.v.i v8, 0");
      asm volatile("vfwadd.vv v16, v12, v8");
      asm volatile("vfwadd.vv v20, v14, v8");
      // v16, v20 (LMUL=4): a and b as FP32

      // printf("upcast 2\n");

      asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(block_avl));
      if (i == 0) {
        asm volatile("vfmul.vv  v4, v16, v20");
      } else {
        asm volatile("vfmacc.vv v4, v16, v20");
      }

      // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v  v4, (%0)" :: "r"(&tmpreg)); dump_vreg(" v4", tmpreg, 32);
      // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v v16, (%0)" :: "r"(&tmpreg)); dump_vreg("v16", tmpreg, 32);
      // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v v20, (%0)" :: "r"(&tmpreg)); dump_vreg("v20", tmpreg, 32);
    }

    // load `block_vl` E8M0 scales from `a_scale` and `b_scale`
    asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(block_avl));
    asm volatile("vle8.v v8, (%0)" :: "r"(a_scale));
    asm volatile("vle8.v v9, (%0)" :: "r"(b_scale));
    // v8, v9 (LMUL=1): a_scale and b_scale as UINT8 (E8M0)

    // upcast from UINT8 to UINT32, add them together
    // asm volatile("vwaddu.vv v10, v8, v9");
    asm volatile("vwmulu.vx v12, v8, %0" :: "r"(1));
    asm volatile("vwmulu.vx v14, v9, %0" :: "r"(1));
    // v12, v14 (LMUL=2): a_scale and b_scale as UINT8 (E8M0)

    asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(block_avl));
    asm volatile("vadd.vv v8, v12, v14");
    // v8 (LMUL=2): a_scale+b_scale as UINT8

    asm volatile("vwmulu.vx v16, v8, %0" :: "r"(1));
    // v16 (LMUL=4): a_scale+b_scale as UINT8

    // remove double bias, shift to manually create an FP32 scale
    asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(block_avl));
    asm volatile("vsub.vx v16, v16, %0" :: "r"(2 * E8M0_BIAS - FP32_BIAS));
    // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v v16, (%0)" :: "r"(&tmpreg)); dump_vreg("scales_m", tmpreg, 64);
    asm volatile("vsll.vi v16, v16, 23");
    // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v v16, (%0)" :: "r"(&tmpreg)); dump_vreg("scales_f", tmpreg, 64);
    // v16 (LMUL=4): product of scales as FP32

    // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v  v0, (%0)" :: "r"(&tmpreg)); dump_vreg("global", tmpreg, 64);
    // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v  v4, (%0)" :: "r"(&tmpreg)); dump_vreg(" local", tmpreg, 64);
    // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v v16, (%0)" :: "r"(&tmpreg)); dump_vreg("scales", tmpreg, 64);

    // global accumulator[i] += local accumulator[i] * scale[i]
    // NOTE: Need tail-undisturbed setting here to avoid overwriting values in
    //       global accumulator for last blocks.
    asm volatile("vsetvli zero, %0, e32, m4, tu, ma" :: "r"(block_avl));
    asm volatile("vfmacc.vv v0, v4, v16");

    // memset(&tmpreg, 0x55, 64); asm volatile("vse32.v  v0, (%0)" :: "r"(&tmpreg)); dump_vreg("global", tmpreg, 64);

    a += block_vl * MXFP8_BLOCK_SIZE;
    b += block_vl * MXFP8_BLOCK_SIZE;
    a_scale += block_vl;
    b_scale += block_vl;
    block_avl -= block_vl;
  } while (block_avl > 0);

#ifdef DEBUG
  printf("before reduce and return\n");
#endif

  // Reduce and return
  // NOTE: avl=-1 to sum all elements in v0.
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(-1));
  memset(&tmpreg, 0x55, 64); asm volatile("vse32.v  v0, (%0)" :: "r"(&tmpreg)); dump_vreg("global", tmpreg, 64);
  asm volatile("vmv.v.i v8, 0"); // FIXME: could be vmv.s.x
  memset(&tmpreg, 0x55, 64); asm volatile("vse32.v  v8, (%0)" :: "r"(&tmpreg)); dump_vreg("   out", tmpreg, 64);
  asm volatile("vfredusum.vs v8, v0, v8");
  memset(&tmpreg, 0x55, 64); asm volatile("vse32.v  v8, (%0)" :: "r"(&tmpreg)); dump_vreg("   out", tmpreg, 64);

  float result;
  asm volatile("vfmv.f.s %0, v8" : "=f"(result));
  return result;
}
