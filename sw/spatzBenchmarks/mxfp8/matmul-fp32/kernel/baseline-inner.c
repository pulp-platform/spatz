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

#include "baseline-inner.h"
#include "../../mx_layer.h"

// vector register allocation:
// - post-scale accumulator: 2x EMUL=4: v0-v3, v4-v7
// - ...
//
// - inner product: vectorizing reduction dimension (K dimension)
// - 4x data reuse
// - statically assumes VLEN=512, making 32 elements with SEW=32 (entire MX
//   block) fit into a vector register group with LMUL=2
void mxfp8_matmul_fp32_inner_4x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  // makes compiler avoid special cases on loop entries
  if (M == 0 || N == 0 || K == 0 || K % MX_BLOCK_SIZE != 0)
    return;

  uint32_t K_BLOCK = K / MX_BLOCK_SIZE;

  for (uint32_t m = 0; m < M; m++) {
    for (uint32_t n = 0; n < N; n += 4) {
      // NOTE: We accumulate the elements in vector form here, which helps
      //       amortize the expensive reduction operation at the end.
      //       This is not energy-efficient as the scaling is applied 32 times
      //       instead of once, but it is much more performant.

      for (uint32_t k = 0; k < K; k += MX_BLOCK_SIZE) {
        uint32_t k_block = k / MX_BLOCK_SIZE;
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(MX_BLOCK_SIZE));

        const char *a_ = a + m * K + k;
        const char *b_ = b + n * K + k;

        // load operands and widen to FP16
        //  (a) a[m][k:k+MX_BLOCK_SIZE]   -> v23 () -> v16 (FP16)
        asm volatile("vle8.v v23, (%0)" :: "r"(a_));
        asm volatile("vfwadd.vf v16, v23, %0" :: "f"(0.0f));
        // (b0) b[k:k+MX_BLOCK_SIZE][n  ] -> v24 () -> v17 (FP16)
        asm volatile("vle8.v v24, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v17, v24, %0" :: "f"(0.0f));
        b_ += K;
        // (b1) b[k:k+MX_BLOCK_SIZE][n+1] -> v25 () -> v18 (FP16)
        asm volatile("vle8.v v25, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v18, v25, %0" :: "f"(0.0f));
        b_ += K;
        // (b2) b[k:k+MX_BLOCK_SIZE][n+2] -> v26 () -> v19 (FP16)
        asm volatile("vle8.v v26, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v19, v26, %0" :: "f"(0.0f));
        b_ += K;
        // (b3) b[k:k+MX_BLOCK_SIZE][n+3] -> v27 () -> v20 (FP16)
        asm volatile("vle8.v v27, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v20, v27, %0" :: "f"(0.0f));
        b_ += K;

        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(MX_BLOCK_SIZE));

        // widen and multiply operands to FP32
        asm volatile("vfwmul.vv v24, v16, v17");
        asm volatile("vfwmul.vv v26, v16, v18");
        asm volatile("vfwmul.vv v28, v16, v19");
        asm volatile("vfwmul.vv v30, v16, v20");

        asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MX_BLOCK_SIZE / 2));

        // load scales, add and re-bias to FP32
        const uint8_t *a_scale_ = (const uint8_t *)a_scale + m * K_BLOCK + k_block;
        const uint8_t *b_scale_ = (const uint8_t *)b_scale + n * K_BLOCK + k_block;

        uint32_t as  = *a_scale_;
        uint32_t bs0 = *b_scale_;
        uint32_t ss0 = as + bs0 - (2 * E8M0_BIAS - FP32_BIAS);
        b_scale_ += K_BLOCK;
        uint32_t bs1 = *b_scale_;
        uint32_t ss1 = as + bs1 - (2 * E8M0_BIAS - FP32_BIAS);
        b_scale_ += K_BLOCK;
        uint32_t bs2 = *b_scale_;
        uint32_t ss2 = as + bs2 - (2 * E8M0_BIAS - FP32_BIAS);
        b_scale_ += K_BLOCK;
        uint32_t bs3 = *b_scale_;
        uint32_t ss3 = as + bs3 - (2 * E8M0_BIAS - FP32_BIAS);
        b_scale_ += K_BLOCK;

        asm volatile("vfadd.vv v24, v24, v25");
        asm volatile("vfadd.vv v26, v26, v27");
        asm volatile("vfadd.vv v28, v28, v29");
        asm volatile("vfadd.vv v30, v30, v31");

        // convert to FP32 using bit operations
        float scale0, scale1, scale2, scale3;
        ss0 <<= 23;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale0) : "r"(ss0));
        ss1 <<= 23;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale1) : "r"(ss1));
        ss2 <<= 23;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale2) : "r"(ss2));
        ss3 <<= 23;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale3) : "r"(ss3));

        // scale and accumulate
        if (k == 0) {
          asm volatile("vfmul.vf v0, v24, %0" :: "f"(scale0));
          asm volatile("vfmul.vf v2, v26, %0" :: "f"(scale1));
          asm volatile("vfmul.vf v4, v28, %0" :: "f"(scale2));
          asm volatile("vfmul.vf v6, v30, %0" :: "f"(scale3));
        } else {
          asm volatile("vfmacc.vf v0, %0, v24" :: "f"(scale0));
          asm volatile("vfmacc.vf v2, %0, v26" :: "f"(scale1));
          asm volatile("vfmacc.vf v4, %0, v28" :: "f"(scale2));
          asm volatile("vfmacc.vf v6, %0, v30" :: "f"(scale3));
        }
      }

      // reduce
      // NOTE: This improves the performance of the reduction by first adding
      // the elements from both vector registers (in parallel) before reducing.
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MX_BLOCK_SIZE / 2));
      asm volatile("vmv.v.i v23, 0");

      asm volatile("vfredusum.vs  v8, v0, v23");
      asm volatile("vfredusum.vs  v9, v2, v23");
      asm volatile("vfredusum.vs v10, v4, v23");
      asm volatile("vfredusum.vs v11, v6, v23");

      // store result in c[m][n:n+4]
      float acc0, acc1, acc2, acc3;
      float *c_ = c + m * N + n;
      asm volatile("vfmv.f.s %0, v8" : "=f"(acc0));
      *c_ = acc0;
      c_++;
      asm volatile("vfmv.f.s %0, v9" : "=f"(acc1));
      *c_ = acc1;
      c_++;
      asm volatile("vfmv.f.s %0, v10" : "=f"(acc2));
      *c_ = acc2;
      c_++;
      asm volatile("vfmv.f.s %0, v11" : "=f"(acc3));
      *c_ = acc3;
      c_++;
    }
  }
}
