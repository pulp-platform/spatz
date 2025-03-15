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


// MXFP8 matrix multiplication
// ---------------------------
// - block size = 32 fixed
// - a is an MxK matrix
// - b is an KxN matrix
// - c is an MxN matrix
// - a_scale is an Mx(K/32)
// - b_scale is an (K/32)xN
// - K = reduction dimension = block quantization direction
// - element data format: FP8
// - scale data format: E8M0 (NaN not handled)
// - accumulation in FP32


// - inner product: vectorizing reduction dimension (K dimension)
// - natural data layout: c, a, a_scale in row-major order
//                        b, b_scale in column-major order
// - M, N, K > 0
// - statically assumes VLEN=512, making 32 elements with SEW=32 (entire MX
//   block) fit into a vector register group with LMUL=2
// - no data reuse
void mxfp8_matmul_fp32_inner_1x(float *c,
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

      // BUG: This is required to ensure correct results.
      for (int i = 0; i < 4; i++) asm volatile("nop");

      // store result in c[m][n]
      float acc;
      asm volatile("vfmv.f.s %0, v24" : "=f"(acc));
      c[m * N + n] = acc;
    }
  }
}

// - inner product: vectorizing reduction dimension (K dimension)
// - reduce first: uses more (slow) reductions, potentially more energy
//                 efficient
// - natural data layout: c, a, a_scale in row-major order
//                        b, b_scale in column-major order
// - M, N, K > 0
// - statically assumes VLEN=512, making 32 elements with SEW=32 (entire MX
//   block) fit into a vector register group with LMUL=2
// - no data reuse
void mxfp8_matmul_fp32_inner_reducefirst_1x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  for (uint32_t m = 0; m < M; m++) {
    for (uint32_t n = 0; n < N; n++) {
      float acc = 0;

      for (uint32_t k = 0; k < K; k += MXFP8_BLOCK_SIZE) {
        uint32_t k_block = k / MXFP8_BLOCK_SIZE;
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

        asm volatile("vmv.v.i v0, 0");

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

        asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

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

// - inner product: vectorizing reduction dimension (K dimension)
// - natural data layout: c, a, a_scale in row-major order
//                        b, b_scale in column-major order
// - M, N, K > 0
// - statically assumes VLEN=512, making 32 elements with SEW=32 (entire MX
//   block) fit into a vector register group with LMUL=2
// - 4x data reuse
void mxfp8_matmul_fp32_inner_4x(float *c,
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

// - inner product: vectorizing reduction dimension (K dimension)
// - sdotp: uses custom ExSdotp instructions (vfwdotp.vv)
// - natural data layout: c, a, a_scale in row-major order
//                        b, b_scale in column-major order
// - M, N, K > 0
// - statically assumes VLEN=512, making 32 elements with SEW=32 (entire MX
//   block) fit into a vector register group with LMUL=2
// - 4x data reuse
void mxfp8_matmul_fp32_inner_sdotp_4x(float *c,
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
        // clear 4 registers for use as vfwdotp accumulators
        asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(2 * MXFP8_BLOCK_SIZE));
        asm volatile("vmv.v.i v8, 0");

        uint32_t k_block = k / MXFP8_BLOCK_SIZE;
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

        const char *a_ = a + m * K + k;
        const char *b_ = b + n * K + k;
        const uint8_t *a_scale_ = (const uint8_t *)a_scale + m * K_BLOCK + k_block;
        const uint8_t *b_scale_ = (const uint8_t *)b_scale + n * K_BLOCK + k_block;

        // (1) load operands and widen to FP16
        // (2) load scales, add and re-bias to FP32

        //  (a) a[m][k:k+MXFP8_BLOCK_SIZE]   -> v23 (FP8) -> v16 (FP16)
        asm volatile("vle8.v v23, (%0)" :: "r"(a_));
        uint32_t as  = *a_scale_;
        asm volatile("vfwadd.vf v16, v23, %0" :: "f"(0.0f));

        // (b0) b[k:k+MXFP8_BLOCK_SIZE][n  ] -> v24 (FP8) -> v17 (FP16)
        asm volatile("vle8.v v24, (%0)" :: "r"(b_));
        b_ += K;
        uint32_t bs0 = *b_scale_;
        b_scale_ += K_BLOCK;
        asm volatile("vfwadd.vf v17, v24, %0" :: "f"(0.0f));
        uint32_t ss0 = as + bs0 - (2 * E8M0_BIAS - FP32_BIAS);

        // (b1) b[k:k+MXFP8_BLOCK_SIZE][n+1] -> v25 (FP8) -> v18 (FP16)
        asm volatile("vle8.v v25, (%0)" :: "r"(b_));
        b_ += K;
        uint32_t bs1 = *b_scale_;
        b_scale_ += K_BLOCK;
        asm volatile("vfwadd.vf v18, v25, %0" :: "f"(0.0f));
        uint32_t ss1 = as + bs1 - (2 * E8M0_BIAS - FP32_BIAS);

        // (b2) b[k:k+MXFP8_BLOCK_SIZE][n+2] -> v26 (FP8) -> v19 (FP16)
        asm volatile("vle8.v v26, (%0)" :: "r"(b_));
        b_ += K;
        uint32_t bs2 = *b_scale_;
        b_scale_ += K_BLOCK;
        asm volatile("vfwadd.vf v19, v26, %0" :: "f"(0.0f));
        uint32_t ss2 = as + bs2 - (2 * E8M0_BIAS - FP32_BIAS);

        // (b3) b[k:k+MXFP8_BLOCK_SIZE][n+3] -> v27 (FP8) -> v20 (FP16)
        asm volatile("vle8.v v27, (%0)" :: "r"(b_));
        b_ += K;
        uint32_t bs3 = *b_scale_;
        b_scale_ += K_BLOCK;
        asm volatile("vfwadd.vf v20, v27, %0" :: "f"(0.0f));
        uint32_t ss3 = as + bs3 - (2 * E8M0_BIAS - FP32_BIAS);

        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE));

        // (1) widening dot product to FP32 -> v8, v9, v10, v11 (FP32) as LMUL=1
        // (2) convert scales to FP32 using bit operations
        float scale0, scale1, scale2, scale3;

        asm volatile("vfwdotp.vv  v8, v16, v17");
        ss0 <<= 23;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale0) : "r"(ss0));

        asm volatile("vfwdotp.vv  v9, v16, v18");
        ss1 <<= 23;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale1) : "r"(ss1));

        asm volatile("vfwdotp.vv v10, v16, v19");
        ss2 <<= 23;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale2) : "r"(ss2));

        asm volatile("vfwdotp.vv v11, v16, v20");
        ss3 <<= 23;
        asm volatile("fmv.w.x %0, %1" : "=f"(scale3) : "r"(ss3));

        asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE / 2));

        // scale and accumulate
        if (k == 0) {
          asm volatile("vfmul.vf v0, v8, %0" :: "f"(scale0));
          asm volatile("vfmul.vf v1, v9, %0" :: "f"(scale1));
          asm volatile("vfmul.vf v2, v10, %0" :: "f"(scale2));
          asm volatile("vfmul.vf v3, v11, %0" :: "f"(scale3));
        } else {
          asm volatile("vfmacc.vf v0, %0,  v8" :: "f"(scale0));
          asm volatile("vfmacc.vf v1, %0,  v9" :: "f"(scale1));
          asm volatile("vfmacc.vf v2, %0, v10" :: "f"(scale2));
          asm volatile("vfmacc.vf v3, %0, v11" :: "f"(scale3));
        }
      }

      // reduce
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE / 2));
      asm volatile("vmv.v.i v23, 0");

      asm volatile("vfredusum.vs  v8, v0, v23");
      asm volatile("vfredusum.vs  v9, v1, v23");
      asm volatile("vfredusum.vs v10, v2, v23");
      asm volatile("vfredusum.vs v11, v3, v23");

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

// - outer product: vectorizing along output rows (N dimension)
// - optimal data layout: all matrices in row-major order
// - M, N, K > 0
// - no data reuse
// - for maximum throughput, N should be a multiple of (4 * VLEN / 32)
void mxfp8_matmul_fp32_outer_lmul4_1x(float *c,
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
        uint8_t as = *a_scale_;
        // load operands: b_scale[k_block][n:n+n_vl] -> v12
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(N - n));
        asm volatile("vle8.v v12, (%0)" :: "r"(b_scale_));

        // add scales and widen to 16-bit -> v14-v15
        asm volatile("vwaddu.vx v14, v12, %0" :: "r"(as));

        // re-bias for FP32 and widen to 32-bit -> v8-v11
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(N - n));
        asm volatile("vwsubu.vx v8, v14, %0" :: "r"(2 * E8M0_BIAS - FP32_BIAS));

        // convert to FP32 using bit operations
        asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(N - n));
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

// - outer product: vectorizing along output rows (N dimension)
// - optimal data layout: all matrices in row-major order
// - M, N, K > 0
// - 2x data reuse
// - for maximum throughput, N should be a multiple of (4 * VLEN / 32)
void mxfp8_matmul_fp32_outer_lmul4_2x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  uint32_t n_vl;

  float *c_m0 = c;      // c[m][0]
  const char *a_m0 = a; // a[m][0]
  const uint8_t *a_scale_m0 = (const uint8_t*)a_scale; // a_scale[m][0]

  for (uint32_t m = 0; m < M; m += 2) {
    for (uint32_t n = 0; n < N; n += n_vl) {
      // post-scale accumulator: v0-v3, v16-v19
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(n_vl) : "r"(N - n));
      asm volatile("vmv.v.i  v0, 0");
      asm volatile("vmv.v.i v16, 0");

      float *c_ = c_m0 + n;   // c[m][n]
      const char *a_ = a_m0;  // a[m][0]
      const char *b_ = b + n; // b[0][n]
      const uint8_t *a_scale_ = a_scale_m0;                   // a_scale[m][0]
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n; // b_scale[0][n]

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator: v4-v7, v20-v23

        for (uint32_t k_elem = 0; k_elem < MXFP8_BLOCK_SIZE; k_elem++) {
          asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(N - n));

          // load operands: a[m:m+2][k] and b[k][n:n+n_vl]
          float a0, a1;
          asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a_));
          asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a_ + K));
          asm volatile("vle8.v v8, (%0)" :: "r"(b_));

          // widen to FP16
          asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
          asm volatile("vfwadd.vf v10, v8, %0" :: "f"(0.0f));

          asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(N - n));

          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          if (k_elem == 0) {
            asm volatile("vfwmul.vf  v4, v10, %0" :: "f"(a0));
            asm volatile("vfwmul.vf v20, v10, %0" :: "f"(a1));
          } else {
            asm volatile("vfwmacc.vf  v4, %0, v10" :: "f" (a0));
            asm volatile("vfwmacc.vf v20, %0, v10" :: "f" (a1));
          }

          a_ += 1; // next column
          b_ += N; // next row
        }

        // scaling

        // load operand: a_scale[m:m+2][k_block]
        uint8_t as0 = *(a_scale_);
        uint8_t as1 = *(a_scale_ + K_BLOCK);
        // load operands: b_scale[k_block][n:n+n_vl] -> v12
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(N - n));
        asm volatile("vle8.v v12, (%0)" :: "r"(b_scale_));

        // add scales and widen to 16-bit -> v14-v15, v30-v31
        asm volatile("vwaddu.vx v14, v12, %0" :: "r"(as0));
        asm volatile("vwaddu.vx v30, v12, %0" :: "r"(as1));

        // re-bias for FP32 and widen to 32-bit -> v8-v11, v24-v27
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(N - n));
        asm volatile("vwsubu.vx  v8, v14, %0" :: "r"(2 * E8M0_BIAS - FP32_BIAS));
        asm volatile("vwsubu.vx v24, v30, %0" :: "r"(2 * E8M0_BIAS - FP32_BIAS));

        // convert to FP32 using bit operations
        asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(N - n));
        asm volatile("vsll.vi  v8,  v8, 23");
        asm volatile("vsll.vi v24, v24, 23");

        // post-scale acc += pre-scale acc * scale
        asm volatile("vfmacc.vv  v0,  v4,  v8");
        asm volatile("vfmacc.vv v16, v20, v24");

        a_scale_ += 1; // next column
        b_scale_ += N; // next row
      }

      asm volatile("vse32.v  v0, (%0)" :: "r"(c_));
      asm volatile("vse32.v v16, (%0)" :: "r"(c_ + N));
    }

    // increment row
    c_m0 += 2 * N;
    a_m0 += 2 * K;
    a_scale_m0 += 2 * K_BLOCK;
  }
}

// - outer product: vectorizing along output rows (N dimension)
// - optimal data layout: all matrices in row-major order
// - M, N, K > 0
// - 4x data reuse
// - for maximum throughput, N should be a multiple of (2 * VLEN / 32)
void mxfp8_matmul_fp32_outer_lmul2_4x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  uint32_t n_vl;
  for (uint32_t m = 0; m < M; m += 4) {
    for (uint32_t n = 0; n < N; n += n_vl) {
      // post-scale accumulator: v0-v1, v2-v3, v4-v5, v6-v7
      asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(n_vl) : "r"(N - n));
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v2, 0");
      asm volatile("vmv.v.i v4, 0");
      asm volatile("vmv.v.i v6, 0");

      float *c_ = c + m * N + n;  // c[m][n]
      const char *a_ = a + m * K; // a[m][0]
      const char *b_ = b + n;     // b[0][n]
      const uint8_t *a_scale_ = (const uint8_t *)a_scale + m * K_BLOCK;
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n;

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator: v8-v9, v10-v11, v12-v13, v14-v15

        for (uint32_t k_elem = 0; k_elem < MXFP8_BLOCK_SIZE; k_elem++) {
          asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(N - n));

          // load operand: a[m:m+4][k]
          float a0, a1, a2, a3;
          asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a_        ));
          asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a_ +     K));
          asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a_ + 2 * K));
          asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a_ + 3 * K));

          // load operands: b[k][n:n+n_vl]
          asm volatile("vle8.v v16, (%0)" :: "r"(b_));

          // widen to FP16
          asm volatile("vfwadd.vf v18, v16, %0" :: "f"(0.0f));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a2));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a3));
          // BUG: Moving the fcvt.h.b before the vfwadd.vf breaks, because the
          //      result is then written to the VRF instead of FP RF. This is
          //      (I think) because spatz_vfu moves on to the next instruction
          //      too quickly, which then clears the `op_arith.is_scalar` flag
          //      in `spatz_req`, which leads the VFU to (incorrectly) send 0
          //      instead of the result back to the controller. It also writes
          //      the value to the VRF, even though it shouldn't.

          asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(N - n));

          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          if (k_elem == 0) {
            asm volatile("vfwmul.vf  v8, v18, %0" :: "f"(a0));
            asm volatile("vfwmul.vf v10, v18, %0" :: "f"(a1));
            asm volatile("vfwmul.vf v12, v18, %0" :: "f"(a2));
            asm volatile("vfwmul.vf v14, v18, %0" :: "f"(a3));
          } else {
            asm volatile("vfwmacc.vf  v8, %0, v18" :: "f" (a0));
            asm volatile("vfwmacc.vf v10, %0, v18" :: "f" (a1));
            asm volatile("vfwmacc.vf v12, %0, v18" :: "f" (a2));
            asm volatile("vfwmacc.vf v14, %0, v18" :: "f" (a3));
          }

          a_ += 1; // next column
          b_ += N; // next row
        }

        // scaling

        // load operand: a_scale[m:m+2][k_block]
        uint8_t as0 = a_scale_[0];
        uint8_t as1 = a_scale_[1 * K_BLOCK];
        uint8_t as2 = a_scale_[2 * K_BLOCK];
        uint8_t as3 = a_scale_[3 * K_BLOCK];
        // load operands: b_scale[k_block][n:n+n_vl] -> v16
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(N - n));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_scale_));

        // add scales and widen to 16-bit -> v18, v19, v20, v21
        asm volatile("vwaddu.vx v18, v16, %0" :: "r"(as0));
        asm volatile("vwaddu.vx v19, v16, %0" :: "r"(as1));
        asm volatile("vwaddu.vx v20, v16, %0" :: "r"(as2));
        asm volatile("vwaddu.vx v21, v16, %0" :: "r"(as3));

        // re-bias for FP32 and widen to 32-bit -> v24-v25, v26-v27, v28-v29, v30-v31
        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(N - n));
        asm volatile("vwsubu.vx v24, v18, %0" :: "r"(2 * E8M0_BIAS - FP32_BIAS));
        asm volatile("vwsubu.vx v26, v19, %0" :: "r"(2 * E8M0_BIAS - FP32_BIAS));
        asm volatile("vwsubu.vx v28, v20, %0" :: "r"(2 * E8M0_BIAS - FP32_BIAS));
        asm volatile("vwsubu.vx v30, v21, %0" :: "r"(2 * E8M0_BIAS - FP32_BIAS));

        // convert to FP32 using bit operations
        asm volatile("vsetvli zero, %0, e32, m2, ta, ma" :: "r"(N - n));
        asm volatile("vsll.vi v24, v24, 23");
        asm volatile("vsll.vi v26, v26, 23");
        asm volatile("vsll.vi v28, v28, 23");
        asm volatile("vsll.vi v30, v30, 23");

        // post-scale acc += pre-scale acc * scale
        asm volatile("vfmacc.vv  v0,  v8, v24");
        asm volatile("vfmacc.vv  v2, v10, v26");
        asm volatile("vfmacc.vv  v4, v12, v28");
        asm volatile("vfmacc.vv  v6, v14, v30");

        a_scale_ += 1; // next column
        b_scale_ += N; // next row
      }

      asm volatile("vse32.v v0, (%0)" :: "r"(c_        ));
      asm volatile("vse32.v v2, (%0)" :: "r"(c_ +     N));
      asm volatile("vse32.v v4, (%0)" :: "r"(c_ + 2 * N));
      asm volatile("vse32.v v6, (%0)" :: "r"(c_ + 3 * N));
    }
  }
}
