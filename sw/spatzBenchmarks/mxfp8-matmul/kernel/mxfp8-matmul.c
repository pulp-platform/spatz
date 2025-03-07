// Copyright 2025 ETH Zurich and University of Bologna.
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

#include "mxfp8-matmul.h"

#define MXFP8_BLOCK_SIZE 32
#define E8M0_BIAS 127
#define FP32_BIAS 127

// MXFP8 dot product
// - block size = 32 fixed
// - a is an MxK matrix, stored in row-major format
// - b is an KxN matrix, stored in column-major format
// - c is an MxN matrix, stored in row-major format
// - in other words,
//   K = reduction dimension = inner dimension = block quantization direction
// - scale data format: E8M0 (bias = 127, NaN not handled)
//
// - M needs to be TODO
// - N needs to be TODO
// - K needs to be a multiple of 32 (block size)
//
// NOTE: This assumes VLEN = 512, which leads to 16 elements per vector register
// for SEW = 32 (and thus, an entire MX block fits when LMUL = 2 and SEW = 32).
//
// NOTE: This kernel simply loops through the output matrix and computes a dotp
//       for each result entry, without re-using any memory.
void mxfp8_matmul_fp32_dotp(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  for (uint32_t m = 0; m < M; m++) {
    for (uint32_t n = 0; n < N; n++) {
      // post-scale accumulator: v0-v1

      // NOTE: We accumulate the elements in vector form here, which helps
      //       amortize the expensive reduction operation at the end.
      //       This is not energy-efficient as the scaling is applied 32 times
      //       instead of once, but it is much more performant.

      for (uint32_t k = 0; k < K; k += MXFP8_BLOCK_SIZE) {
        uint32_t k_block = k / MXFP8_BLOCK_SIZE;
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

        // load operands: a[m][k:k+MXFP8_BLOCK_SIZE]
        asm volatile("vle8.v v4, (%0)" :: "r"(a + m * K + k));
        // load operands: b[k:k+MXFP8_BLOCK_SIZE][n]
        asm volatile("vle8.v v5, (%0)" :: "r"(b + n * K + k));

        // widen operands to FP16
        asm volatile("vfwadd.vf v6, v4, %0" :: "f"(0.0f));
        asm volatile("vfwadd.vf v7, v5, %0" :: "f"(0.0f));

        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

        // widen and multiply operands to FP32
        asm volatile("vfwmul.vv v4, v6, v7");

        asm volatile("vsetvli zero, %0, e32, m2, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

        uint32_t as = (uint32_t)a_scale[m * K_BLOCK + k_block];
        uint32_t bs = (uint32_t)b_scale[n * K_BLOCK + k_block];
        // add and re-bias for FP32
        uint32_t ss = as + bs - (2 * E8M0_BIAS - FP32_BIAS);
        // convert to FP32 using bit operations
        ss <<= 23;
        float scale;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale) : "r"(ss));

        // scale and accumulate
        if (k == 0) {
          asm volatile("vfmul.vf v0, v4, %0" :: "f"(scale));
        } else {
          asm volatile("vfmacc.vf v0, %0, v4" :: "f"(scale));
        }
      }

      // reduce
      asm volatile("vmv.v.i v24, 0");
      asm volatile("vfredusum.vs v24, v0, v24");

      // store result in c[m][n]
      float acc;
      asm volatile("vfmv.f.s %0, v24" : "=f"(acc));
      c[m * N + n] = acc;
    }
  }
}
