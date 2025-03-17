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

#include "mxfp8-matvec.h"

#define MXFP8_BLOCK_SIZE 32
#define E8M0_BIAS 127
#define FP32_BIAS 127


// MXFP8 matrix-vector multiplication
// ----------------------------------
// - block size = 32 fixed
// - a is an MxK matrix
// - b is an Nx1 vector
// - a_scale is an Mx(N/32)
// - b_scale is an (N/32)x1
// - N = reduction dimension = block quantization direction
// - element data format: FP8
// - scale data format: E8M0 (NaN not handled)
// - accumulation in FP32

// - inner product: vectorizing reduction dimension (K dimension)
// - natural data layout: c, a, a_scale in row-major order
//                        b, b_scale in column-major order
// - M, N, K > 0
// - statically assumes VLEN=512, making 32 elements with SEW=32 (entire MX
//   block) fit into a vector register group with LMUL=2
// - 4x data reuse
void mxfp8_matvec_fp32_inner_4x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N)
{
  uint32_t N_BLOCK = N / MXFP8_BLOCK_SIZE;

  for (uint32_t m = 0; m < M; m += 4) {
    // post-scale accumulator: v0, v2, v4, v6

    // NOTE: We accumulate the elements in vector form here, which helps
    //       amortize the expensive reduction operation at the end.
    //       This is not energy-efficient as the scaling is applied 32 times
    //       instead of once, but it is much more performant.

    for (uint32_t n = 0; n < N; n += MXFP8_BLOCK_SIZE) {
      uint32_t n_block = n / MXFP8_BLOCK_SIZE;
      asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

      const char *a_ = a + m * N + n;
      const char *b_ = b + n;

      // load operands and widen to FP16
      // (a0) a[m  ][n:n+MXFP8_BLOCK_SIZE]   -> v20 (FP8) -> v16 (FP16)
      asm volatile("vle8.v v20, (%0)" :: "r"(a_));
      asm volatile("vfwadd.vf v16, v20, %0" :: "f"(0.0f));
      a_ += N;
      // (a1) a[m+1][n:n+MXFP8_BLOCK_SIZE]   -> v21 (FP8) -> v17 (FP16)
      asm volatile("vle8.v v21, (%0)" :: "r"(a_));
      asm volatile("vfwadd.vf v17, v21, %0" :: "f"(0.0f));
      a_ += N;
      // (a2) a[m+2][n:n+MXFP8_BLOCK_SIZE]   -> v22 (FP8) -> v18 (FP16)
      asm volatile("vle8.v v22, (%0)" :: "r"(a_));
      asm volatile("vfwadd.vf v18, v22, %0" :: "f"(0.0f));
      a_ += N;
      // (a3) a[m+3][n:n+MXFP8_BLOCK_SIZE]   -> v23 (FP8) -> v19 (FP16)
      asm volatile("vle8.v v23, (%0)" :: "r"(a_));
      asm volatile("vfwadd.vf v19, v23, %0" :: "f"(0.0f));
      // (b)  b[n:n+MXFP8_BLOCK_SIZE]        -> v24 (FP8) -> v20 (FP16)
      asm volatile("vle8.v v24, (%0)" :: "r"(b_));
      asm volatile("vfwadd.vf v20, v24, %0" :: "f"(0.0f));

      // widen and multiply operands to FP32
      asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));
      asm volatile("vfwmul.vv  v8, v16, v20");
      asm volatile("vfwmul.vv v10, v17, v20");
      asm volatile("vfwmul.vv v12, v18, v20");
      asm volatile("vfwmul.vv v14, v19, v20");

      asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE / 2));

      // load scales, add and re-bias to FP32
      const uint8_t *a_scale_ = (const uint8_t *)a_scale + m * N_BLOCK + n_block;
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n_block;

      uint32_t bs  = *b_scale_;
      uint32_t as0 = *a_scale_;
      a_scale_ += N_BLOCK;
      uint32_t ss0 = as0 + bs - (2 * E8M0_BIAS - FP32_BIAS);
      uint32_t as1 = *a_scale_;
      a_scale_ += N_BLOCK;
      uint32_t ss1 = as1 + bs - (2 * E8M0_BIAS - FP32_BIAS);
      uint32_t as2 = *a_scale_;
      a_scale_ += N_BLOCK;
      uint32_t ss2 = as2 + bs - (2 * E8M0_BIAS - FP32_BIAS);
      uint32_t as3 = *a_scale_;
      uint32_t ss3 = as3 + bs - (2 * E8M0_BIAS - FP32_BIAS);

      // reduce
      asm volatile("vfadd.vv  v8,  v8,  v9");
      asm volatile("vfadd.vv v10, v10, v11");
      asm volatile("vfadd.vv v12, v12, v13");
      asm volatile("vfadd.vv v14, v14, v15");

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
      if (n == 0) {
        asm volatile("vfmul.vf v0,  v8, %0" :: "f"(scale0));
        asm volatile("vfmul.vf v2, v10, %0" :: "f"(scale1));
        asm volatile("vfmul.vf v4, v12, %0" :: "f"(scale2));
        asm volatile("vfmul.vf v6, v14, %0" :: "f"(scale3));
      } else {
        asm volatile("vfmacc.vf v0, %0,  v8" :: "f"(scale0));
        asm volatile("vfmacc.vf v2, %0, v10" :: "f"(scale1));
        asm volatile("vfmacc.vf v4, %0, v12" :: "f"(scale2));
        asm volatile("vfmacc.vf v6, %0, v14" :: "f"(scale3));
      }
    }

    // reduce
    // NOTE: This improves the performance of the reduction by first adding
    // the elements from both vector registers (in parallel) before reducing.
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE / 2));
    asm volatile("vmv.v.i v16, 0");

    asm volatile("vfredusum.vs  v8, v0, v16");
    asm volatile("vfredusum.vs  v9, v2, v16");
    asm volatile("vfredusum.vs v10, v4, v16");
    asm volatile("vfredusum.vs v11, v6, v16");

    // store result in c[m][n:n+4]
    float acc0, acc1, acc2, acc3;
    float *c_ = c + m;
    asm volatile("vfmv.f.s %0,  v8" : "=f"(acc0));
    *c_ = acc0;
    c_++;
    asm volatile("vfmv.f.s %0,  v9" : "=f"(acc1));
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
