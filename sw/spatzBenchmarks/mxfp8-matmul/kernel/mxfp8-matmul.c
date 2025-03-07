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
// - M, N, K > 0
// - K % 32 == 0 (integer number of blocks)
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

        uint32_t as = (uint8_t)a_scale[m * K_BLOCK + k_block];
        uint32_t bs = (uint8_t)b_scale[n * K_BLOCK + k_block];
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
      // NOTE: This improves the performance of the reduction by first adding
      // the elements from both vector registers (in parallel) before reducing.
      asm volatile("vmv.v.i v24, 0");
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE / 2));
      asm volatile("vfadd.vv v0, v0, v1");
      asm volatile("vfredusum.vs v24, v0, v24");

      // store result in c[m][n]
      float acc;
      asm volatile("vfmv.f.s %0, v24" : "=f"(acc));
      c[m * N + n] = acc;
    }
  }
}

// MXFP8 dot product
// - block size = 32 fixed
// - a is an MxK matrix, stored in row-major format
// - b is an KxN matrix, stored in column-major format
// - c is an MxN matrix, stored in row-major format
// - in other words,
//   K = reduction dimension = inner dimension = block quantization direction
// - scale data format: E8M0 (bias = 127, NaN not handled)
//
// - M, N, K > 0
// - N %  4 == 0 (unroll factor)
// - K % 32 == 0 (integer number of blocks)
//
// NOTE: This assumes VLEN = 512, which leads to 16 elements per vector register
// for SEW = 32 (and thus, an entire MX block fits when LMUL = 2 and SEW = 32).
void mxfp8_matmul_fp32_dotp4(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  for (uint32_t m = 0; m < M; m++) {
    for (uint32_t n = 0; n < N; n += 4) {
      // post-scale accumulator: v0-v1, v2-v3, ..., v6-v7

      // NOTE: We accumulate the elements in vector form here, which helps
      //       amortize the expensive reduction operation at the end.
      //       This is not energy-efficient as the scaling is applied 32 times
      //       instead of once, but it is much more performant.

      for (uint32_t k = 0; k < K; k += MXFP8_BLOCK_SIZE) {
        uint32_t k_block = k / MXFP8_BLOCK_SIZE;
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

        const char *a_ = a + m * K + k;
        const char *b_ = b + n * K + k;

        // load operands and widen to FP16
        //  (a) a[m][k:k+MXFP8_BLOCK_SIZE]   -> v23 (FP8) -> v16 (FP16)
        asm volatile("vle8.v v23, (%0)" :: "r"(a_));
        asm volatile("vfwadd.vf v16, v23, %0" :: "f"(0.0f));
        // (b0) b[k:k+MXFP8_BLOCK_SIZE][n  ] -> v24 (FP8) -> v17 (FP16)
        asm volatile("vle8.v v24, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v17, v24, %0" :: "f"(0.0f));
        b_ += K;
        // (b1) b[k:k+MXFP8_BLOCK_SIZE][n+1] -> v25 (FP8) -> v18 (FP16)
        asm volatile("vle8.v v25, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v18, v25, %0" :: "f"(0.0f));
        b_ += K;
        // (b2) b[k:k+MXFP8_BLOCK_SIZE][n+2] -> v26 (FP8) -> v19 (FP16)
        asm volatile("vle8.v v26, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v19, v26, %0" :: "f"(0.0f));
        b_ += K;
        // (b3) b[k:k+MXFP8_BLOCK_SIZE][n+3] -> v27 (FP8) -> v20 (FP16)
        asm volatile("vle8.v v27, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v20, v27, %0" :: "f"(0.0f));
        b_ += K;

        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

        // widen and multiply operands to FP32
        asm volatile("vfwmul.vv v24, v16, v17");
        asm volatile("vfwmul.vv v26, v16, v18");
        asm volatile("vfwmul.vv v28, v16, v19");
        asm volatile("vfwmul.vv v30, v16, v20");

        asm volatile("vsetvli zero, %0, e32, m2, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

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
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE / 2));
      asm volatile("vmv.v.i v23, 0");

      asm volatile("vfadd.vv v0, v0, v1");
      asm volatile("vfadd.vv v2, v2, v3");
      asm volatile("vfadd.vv v4, v4, v5");
      asm volatile("vfadd.vv v6, v6, v7");
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
      *c_ = acc0;
      c_++;
      asm volatile("vfmv.f.s %0, v10" : "=f"(acc2));
      *c_ = acc0;
      c_++;
      asm volatile("vfmv.f.s %0, v11" : "=f"(acc3));
      *c_ = acc0;
      c_++;
    }
  }
}

// MXFP8 dot product
// - block size = 32 fixed
// - a is an MxK matrix, stored in row-major format
// - b is an KxN matrix, stored in row-major (!) format
// - c is an MxN matrix, stored in row-major format
// - a_scale is an Mx(K/32), stored in row-major format
// - b_scale is an (K/32)xN, stored in row-major format
// - K = reduction dimension = block quantization direction
// - scale data format: E8M0 (bias = 127, NaN not handled)
//
// - M, N, K > 0
// - K % 32 == 0 (integer number of blocks)
//
// NOTE: For maximum throughput, N should be a multiple of (4 * VLEN / 32).
void mxfp8_matmul_fp32_rowmaj_m4(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  uint32_t n_vl;
  for (uint32_t m = 0; m < M; m++) {
    for (uint32_t n = 0; n < N; n += n_vl) {
      // post-scale accumulator: v0-v3
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(n_vl) : "r"(N - n));
      asm volatile("vmv.v.i v0, 0");

      float *c_ = c + m * N + n;  // c[m][n]
      const char *a_ = a + m * K; // a[m][0]
      const char *b_ = b + n;     // b[0][n]
      const uint8_t *a_scale_ = (const uint8_t *)a_scale + m * K_BLOCK;
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n;

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator: v4-v7

        for (uint32_t k_elem = 0; k_elem < MXFP8_BLOCK_SIZE; k_elem++) {
          asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(N - n));

          // load operand: a[m][k]
          float a0;
          asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a_));

         // load operands: b[k][n:n+n_vl]
          asm volatile("vle8.v v8, (%0)" :: "r"(b_));

          // widen to FP16
          asm volatile("vfwadd.vf v10, v8, %0" :: "f"(0.0f));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
          // BUG: Moving the fcvt.h.b before the vfwadd.vf breaks, because the
          //      result is then written to the VRF instead of FP RF. This is
          //      (I think) because spatz_vfu moves on to the next instruction
          //      too quickly, which then clears the `op_arith.is_scalar` flag
          //      in `spatz_req`, which leads the VFU to (incorrectly) send 0
          //      instead of the result back to the controller. It also writes
          //      the value to the VRF, even though it shouldn't.

          asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(N - n));


          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          if (k_elem == 0) {
            asm volatile("vfwmul.vf v4, v10, %0" :: "f"(a0));
          } else {
            asm volatile("vfwmacc.vf v4, %0, v10" :: "f" (a0));
          }

          a_ += 1; // next column
          b_ += N; // next row
        }

        // scaling

        // load operand: a_scale[m][k_block]
        uint32_t as = *a_scale_;
        // load operands: b_scale[k_block][n:n+n_vl] -> v12
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(N - n));
        asm volatile("vle8.v v12, (%0)" :: "r"(b_scale_));

        // widen b_scale to 32-bit -> v8-v11
        asm volatile("vwmulu.vx v14, v12, %0" :: "r"(1));
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(N - n));
        asm volatile("vwmulu.vx v8, v14, %0" :: "r"(1));

        // add and re-bias for FP32 -> v8-v11
        asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(N - n));
        uint32_t as_rebiased = as - (2 * E8M0_BIAS - FP32_BIAS);
        asm volatile("vadd.vx v8, v8, %0" :: "r"(as_rebiased));

        // convert to FP32 using bit operations
        asm volatile("vsll.vi v8, v8, 23");

        // post-scale acc += pre-scale acc * scale
        asm volatile("vfmacc.vv v0, v4, v8");

        a_scale_ += 1; // next column
        b_scale_ += N; // next row
      }

      asm volatile("vse32.v v0, (%0)" :: "r"(c_));
    }
  }
}
