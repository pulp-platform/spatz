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

#include "baseline-inner-reducefirst.h"
#include "../../mx_layer.h"

void mxfp8_matmul_fp32_inner_reducefirst_1x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MX_BLOCK_SIZE;

  for (uint32_t m = 0; m < M; m++) {
    for (uint32_t n = 0; n < N; n++) {
      float acc = 0;

      for (uint32_t k = 0; k < K; k += MX_BLOCK_SIZE) {
        uint32_t k_block = k / MX_BLOCK_SIZE;
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(MX_BLOCK_SIZE));

        asm volatile("vmv.v.i v0, 0");

        // load operands: a[m][k:k+MX_BLOCK_SIZE]
        asm volatile("vle8.v v4, (%0)" :: "r"(a + m * K + k));
        // load operands: b[k:k+MX_BLOCK_SIZE][n]
        asm volatile("vle8.v v5, (%0)" :: "r"(b + n * K + k));

        // widen operands to FP16
        asm volatile("vfwadd.vf v6, v4, %0" :: "f"(0.0f));
        asm volatile("vfwadd.vf v7, v5, %0" :: "f"(0.0f));

        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(MX_BLOCK_SIZE));

        // widen and multiply operands to FP32
        asm volatile("vfwmul.vv v4, v6, v7");

        asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MX_BLOCK_SIZE));

        float reduced;
        asm volatile("vfadd.vv v4, v4, v5");
        asm volatile("vfredusum.vs v0, v4, v0");

        uint32_t as = (uint8_t)a_scale[m * K_BLOCK + k_block];
        uint32_t bs = (uint8_t)b_scale[n * K_BLOCK + k_block];
        // add and re-bias for FP32
        uint32_t ss = as + bs - (2 * E8M0_BIAS - FP32_BIAS);
        // convert to FP32 using bit operations
        ss <<= 23;
        float scale;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale) : "r"(ss));

        asm volatile("vfmv.f.s %0, v0" : "=f"(reduced));

        acc += reduced * scale;
      }

      // store result in c[m][n]
      c[m * N + n] = acc;
    }
  }
}
