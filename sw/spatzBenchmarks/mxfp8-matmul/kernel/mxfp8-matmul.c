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
#define FP16ALT_BIAS 127

#define FCSR_MODE_DST (1 << 8)
#define FCSR_MODE_SRC (1 << 9)


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

        asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE / 2));

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
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(MXFP8_BLOCK_SIZE / 2));
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
      uint32_t n_remaining = N - n;

      // post-scale accumulator (FP32): v0-v3, v4-v7
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(n_vl) : "r"(n_remaining));
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v4, 0");

      float *c_ = c_m0 + n;    // c[m][n]
      const char *a_ = a_m0;   // a[m][0]
      const char *b_ = b + n;  // b[0][n]
      const uint8_t *a_scale_ = a_scale_m0;                    // a_scale[m][0]
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n;  // b_scale[0][n]

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator (FP32): v8-v11, v12-v15

        // loop head

        // load operands and widen to FP16: b[k][n:n+n_vl]
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(n_remaining));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v18, v16, %0" :: "f"(0.0f));
        b_ += N; // next row

        // load operands and widen to FP16: a[m:m+2][k]
        const char *a__ = a_;
        double a0, a1; // actually contain FP8 (and later FP16)
        asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
        asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
        a__ += K;
        asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
        asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
        a_ += 1; // next column

        uint32_t k_elem = 0;

        while (k_elem < MXFP8_BLOCK_SIZE) {
          k_elem += 1;

          // load operands: b[k][n:n+n_vl]
          asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(n_remaining));
          asm volatile("vle8.v v17, (%0)" :: "r"(b_));
          asm volatile("vfwadd.vf v20, v17, %0" :: "f"(0.0f));
          b_ += N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(n_remaining));
          a__ = a_;
          if (k_elem == 1) {
            asm volatile("vfwmul.vf  v8, v18, %0" :: "f"(a0));
            asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
            a__ += K;
            asm volatile("vfwmul.vf v12, v18, %0" :: "f"(a1));
            asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
          } else {
            asm volatile("vfwmacc.vf  v8, %0, v18" :: "f" (a0));
            asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
            a__ += K;
            asm volatile("vfwmacc.vf v12, %0, v18" :: "f" (a1));
            asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
          }
          a_ += 1; // next column

          // unrolled: repeat loop body
          k_elem += 1;

          if (k_elem == MXFP8_BLOCK_SIZE)
            break;

          // load operands: b[k][n:n+n_vl]
          asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(n_remaining));
          asm volatile("vle8.v v16, (%0)" :: "r"(b_));
          asm volatile("vfwadd.vf v18, v16, %0" :: "f"(0.0f));
          b_ += N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(n_remaining));
          a__ = a_;
          asm volatile("vfwmacc.vf  v8, %0, v20" :: "f" (a0));
          asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
          a__ += K;
          asm volatile("vfwmacc.vf v12, %0, v20" :: "f" (a1));
          asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
          a_ += 1; // next column
        }

        // loop tail
        asm volatile("vfwmacc.vf  v8, %0, v20" :: "f"(a0));
        asm volatile("vfwmacc.vf v12, %0, v20" :: "f"(a1));

        // scaling

        // load operands: b_scale[k_block][n:n+n_vl] -> v16
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(n_remaining));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_scale_));

        // load operand: a_scale[m:m+2][k_block]
        const uint8_t *a_scale__ = a_scale_;
        uint8_t as0 = *a_scale__;
        a_scale__ += K_BLOCK;
        uint8_t as1 = *a_scale__;

        // widen to 16-bit unsigned integer -> v18-v19
        asm volatile("vwcvtu.x.x.v v18, v16");

        // re-bias scales for FP32
        int16_t as0_rescaled = as0 - (2 * E8M0_BIAS - FP32_BIAS);
        int16_t as1_rescaled = as1 - (2 * E8M0_BIAS - FP32_BIAS);

        // add scales and widen to 32-bit -> v24-27, v28-v31
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(n_remaining));
        asm volatile("vwadd.vx v24, v18, %0" :: "r"(as0_rescaled));
        asm volatile("vwadd.vx v28, v18, %0" :: "r"(as1_rescaled));

        // convert to FP32 using bit operations
        asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(n_remaining));
        asm volatile("vsll.vi v24, v24, 23");
        asm volatile("vsll.vi v28, v28, 23");

        // post-scale acc += pre-scale acc * scale
        if (k_block < K_BLOCK - 1) {
          asm volatile("vfmacc.vv v0,  v8, v24");
          asm volatile("vfmacc.vv v4, v12, v28");
        } else {
          // last iteration: issue stores
          float *c__ = c_;
          asm volatile("vfmacc.vv v0,  v8, v24");
          asm volatile("vse32.v v0, (%0)" :: "r"(c__));
          c__ += N;
          asm volatile("vfmacc.vv v4, v12, v28");
          asm volatile("vse32.v v4, (%0)" :: "r"(c__));
        }

        a_scale_ += 1; // next column
        b_scale_ += N; // next row
      }
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

  float *c_m0 = c;      // c[m][0]
  const char *a_m0 = a; // a[m][0]
  const uint8_t *a_scale_m0 = (const uint8_t*)a_scale; // a_scale[m][0]

  for (uint32_t m = 0; m < M; m += 4) {
    for (uint32_t n = 0; n < N; n += n_vl) {
      uint32_t n_remaining = N - n;

      // post-scale accumulator (FP32): v0-v1, v2-v3, v4-v5, v6-v7
      asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(n_vl) : "r"(n_remaining));
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v2, 0");
      asm volatile("vmv.v.i v4, 0");
      asm volatile("vmv.v.i v6, 0");

      float *c_ = c_m0 + n;    // c[m][n]
      const char *a_ = a_m0;   // a[m][0]
      const char *b_ = b + n;  // b[0][n]
      const uint8_t *a_scale_ = a_scale_m0;                    // a_scale[m][0]
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n;  // b_scale[0][n]

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator (FP32): v8-v9, v10-v11, v12-v13, v14-v15

        // loop head

        // load operands and widen to FP16: b[k][n:n+n_vl]
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(n_remaining));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_));
        asm volatile("vfwadd.vf v18, v16, %0" :: "f"(0.0f));
        b_ += N; // next row

        // load operands and widen to FP16: a[m:m+4][k]
        const char *a__ = a_;
        double a0, a1, a2, a3; // actually contain FP8 (and later FP16)
        asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
        asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
        a__ += K;
        asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
        asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
        a__ += K;
        asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a__));
        asm volatile("fcvt.h.b %0, %0" : "+f"(a2));
        a__ += K;
        asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a__));
        asm volatile("fcvt.h.b %0, %0" : "+f"(a3));
        a_ += 1; // next column

        uint32_t k_elem = 0;

        while (k_elem < MXFP8_BLOCK_SIZE) {
          k_elem += 1;

          // load operands: b[k][n:n+n_vl]
          asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(n_remaining));
          asm volatile("vle8.v v17, (%0)" :: "r"(b_));
          asm volatile("vfwadd.vf v20, v17, %0" :: "f"(0.0f));
          b_ += N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(n_remaining));
          a__ = a_;
          if (k_elem == 1) {
            asm volatile("vfwmul.vf  v8, v18, %0" :: "f"(a0));
            asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
            a__ += K;
            asm volatile("vfwmul.vf v10, v18, %0" :: "f"(a1));
            asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
            a__ += K;
            asm volatile("vfwmul.vf v12, v18, %0" :: "f"(a2));
            asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a2));
            a__ += K;
            asm volatile("vfwmul.vf v14, v18, %0" :: "f"(a3));
            asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a3));
          } else {
            asm volatile("vfwmacc.vf  v8, %0, v18" :: "f" (a0));
            asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
            a__ += K;
            asm volatile("vfwmacc.vf v10, %0, v18" :: "f" (a1));
            asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
            a__ += K;
            asm volatile("vfwmacc.vf v12, %0, v18" :: "f" (a2));
            asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a2));
            a__ += K;
            asm volatile("vfwmacc.vf v14, %0, v18" :: "f" (a3));
            asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a__));
            asm volatile("fcvt.h.b %0, %0" : "+f"(a3));
          }
          a_ += 1; // next column

          // unrolled: repeat loop body
          k_elem += 1;

          if (k_elem == MXFP8_BLOCK_SIZE)
            break;

          // load operands: b[k][n:n+n_vl]
          asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(n_remaining));
          asm volatile("vle8.v v16, (%0)" :: "r"(b_));
          asm volatile("vfwadd.vf v18, v16, %0" :: "f"(0.0f));
          b_ += N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(n_remaining));
          a__ = a_;
          asm volatile("vfwmacc.vf  v8, %0, v20" :: "f" (a0));
          asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a0));
          a__ += K;
          asm volatile("vfwmacc.vf v10, %0, v20" :: "f" (a1));
          asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a1));
          a__ += K;
          asm volatile("vfwmacc.vf v12, %0, v20" :: "f" (a2));
          asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a__));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a2));
          a__ += K;
          asm volatile("vfwmacc.vf v14, %0, v20" :: "f" (a3));
          asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a__));
          asm volatile("fcvt.h.b %0, %0" : "+f"(a3));
          a_ += 1; // next column
        }

        // loop tail
        asm volatile("vfwmacc.vf  v8, %0, v20" :: "f"(a0));
        asm volatile("vfwmacc.vf v10, %0, v20" :: "f"(a1));
        asm volatile("vfwmacc.vf v12, %0, v20" :: "f"(a2));
        asm volatile("vfwmacc.vf v14, %0, v20" :: "f"(a3));

        // scaling

        // load operands: b_scale[k_block][n:n+n_vl] -> v16
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(n_remaining));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_scale_));

        // load operand: a_scale[m:m+4][k_block]
        const uint8_t *a_scale__ = a_scale_;
        uint8_t as0 = *a_scale__;
        a_scale__ += K_BLOCK;
        uint8_t as1 = *a_scale__;
        a_scale__ += K_BLOCK;
        uint8_t as2 = *a_scale__;
        a_scale__ += K_BLOCK;
        uint8_t as3 = *a_scale__;

        // widen to 16-bit unsigned integer -> v18
        asm volatile("vwcvtu.x.x.v v18, v16");

        // re-bias scales for FP32
        int16_t as0_rescaled = as0 - (2 * E8M0_BIAS - FP32_BIAS);
        int16_t as1_rescaled = as1 - (2 * E8M0_BIAS - FP32_BIAS);
        int16_t as2_rescaled = as2 - (2 * E8M0_BIAS - FP32_BIAS);
        int16_t as3_rescaled = as3 - (2 * E8M0_BIAS - FP32_BIAS);

        // add scales and widen to 32-bit -> v24-25, v26-v27, v28-v29, v30-v31
        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(n_remaining));
        asm volatile("vwadd.vx v24, v18, %0" :: "r"(as0_rescaled));
        asm volatile("vwadd.vx v26, v18, %0" :: "r"(as1_rescaled));
        asm volatile("vwadd.vx v28, v18, %0" :: "r"(as2_rescaled));
        asm volatile("vwadd.vx v30, v18, %0" :: "r"(as3_rescaled));

        // convert to FP32 using bit operations
        asm volatile("vsetvli zero, %0, e32, m2, ta, ma" :: "r"(n_remaining));
        asm volatile("vsll.vi v24, v24, 23");
        asm volatile("vsll.vi v26, v26, 23");
        asm volatile("vsll.vi v28, v28, 23");
        asm volatile("vsll.vi v30, v30, 23");

        // post-scale acc += pre-scale acc * scale
        if (k_block < K_BLOCK - 1) {
          asm volatile("vfmacc.vv v0,  v8, v24");
          asm volatile("vfmacc.vv v2, v10, v26");
          asm volatile("vfmacc.vv v4, v12, v28");
          asm volatile("vfmacc.vv v6, v14, v30");
        } else {
          // last iteration: issue stores
          float *c__ = c_;
          asm volatile("vfmacc.vv v0,  v8, v24");
          asm volatile("vse32.v v0, (%0)" :: "r"(c__));
          c__ += N;
          asm volatile("vfmacc.vv v2, v10, v26");
          asm volatile("vse32.v v2, (%0)" :: "r"(c__));
          c__ += N;
          asm volatile("vfmacc.vv v4, v12, v28");
          asm volatile("vse32.v v4, (%0)" :: "r"(c__));
          c__ += N;
          asm volatile("vfmacc.vv v6, v14, v30");
          asm volatile("vse32.v v6, (%0)" :: "r"(c__));
        }

        a_scale_ += 1; // next column
        b_scale_ += N; // next row
      }
    }

    // increment row
    c_m0 += 4 * N;
    a_m0 += 4 * K;
    a_scale_m0 += 4 * K_BLOCK;
  }
}

// - outer product: vectorizing along output rows (N dimension)
// - sdotp: uses custom ExSdotp instructions (vfwdotp.vv)
// - optimal data layout: c, a, a_scale, b_scale in row-major layout
//                        b in custom layout (TODO: explain this)
// - M, N, K > 0
// - 2x data reuse
// - for maximum throughput, N should be a multiple of (2 * VLEN / 32)
void mxfp8_matmul_fp32_outer_sdotp_lmul2_4x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  uint32_t n_vl;

  float *c_m0 = c;      // c[m][0]
  const char *a_m0 = a; // a[m][0]
  const uint8_t *a_scale_m0 = (const uint8_t*)a_scale; // a_scale[m][0]

  for (uint32_t m = 0; m < M; m += 4) {
    for (uint32_t n = 0; n < N; n += n_vl) {
      uint32_t n_remaining = N - n;

      // post-scale accumulator: v0-v1, v2-v3, v4-v5, v6-v7 (4 rows)
      asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(n_vl) : "r"(n_remaining));
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v2, 0");
      asm volatile("vmv.v.i v4, 0");
      asm volatile("vmv.v.i v6, 0");

      float *c_ = c_m0 + n;       // c[m][n]
      const char *a_ = a_m0;      // a[m][0]
      const char *b_ = b + 2 * n; // b[0][n]
      const uint8_t *a_scale_ = a_scale_m0;                   // a_scale[m][0]
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n; // b_scale[0][n]

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator: v8-v9, v10-v11, v12-v13, v14-v15
        asm volatile("vsetvli zero, %0, e32, m8, ta, ma" :: "r"(-1));
        asm volatile("vmv.v.i v8, 0");

        for (uint32_t k_elem = 0; k_elem < MXFP8_BLOCK_SIZE; k_elem += 2) {

          // load operands: a[m:m+4][k:k+2] and b[k:k+2][n:n+n_vl]
          // BUG: There is an issue with a RAW hazard between the VLSU and the
          //      VFU when using 2-element 8-bit loads. Due to this, use a
          //      single 16-bit load, which completes in a single cycle (and is
          //      also more efficient). This bug only occurs if the address is
          //      not 64-bit aligned.
          asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(1));
          const char *a__ = a_;
          asm volatile("vle16.v v20, (%0)" :: "r"(a__));
          a__ += K;
          asm volatile("vle16.v v21, (%0)" :: "r"(a__));
          a__ += K;
          asm volatile("vle16.v v22, (%0)" :: "r"(a__));
          a__ += K;
          asm volatile("vle16.v v23, (%0)" :: "r"(a__));

          // widen to FP16
          asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(2));
          asm volatile("vfwadd.vf v24, v20, %0" :: "f"(0.0f));
          asm volatile("vfwadd.vf v26, v21, %0" :: "f"(0.0f));
          asm volatile("vfwadd.vf v28, v22, %0" :: "f"(0.0f));
          asm volatile("vfwadd.vf v30, v23, %0" :: "f"(0.0f));

          // load operands: b[k:k+2][n:n+n_vl]
         asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(2 * n_remaining));
          asm volatile("vle8.v v16, (%0)" :: "r"(b_));

          // widen to FP16
          asm volatile("vfwadd.vf v18, v16, %0" :: "f"(0.0f));

          // move a0, a1, a2, a3 to scalar registers (2x FP16 packed into FP32
          // register)
          float a0, a1, a2, a3;
          // NOTE:This should be required as per the RVV specification, but
          //      isn't. This is because vfmv.f.s always movs a 64-bit element
          //      (instead of considering SEW).
          // BUG: Adding this vsetvli instruction triggers a bug where the VFU
          //      doesn't handle back-pressure on the response properly, leading
          //      to one of the vfmv.f.s results being dropped and a subsequent
          //      deadlock (SVA: VfuRspDataStable).
          // asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(1));
          asm volatile("vfmv.f.s %0, v24" : "=f"(a0));
          asm volatile("vfmv.f.s %0, v26" : "=f"(a1));
          asm volatile("vfmv.f.s %0, v28" : "=f"(a2));
          asm volatile("vfmv.f.s %0, v30" : "=f"(a3));

          // BUG: Wait here to ensure the vfmv.f.s instructions all commit
          //      before the next vsetvli instruction is issued. Otherwise, the
          //      controller will apply back-pressure to the VFU and one of the
          //      vfmv.f.s responses will be dropped (SVA: VfuRspDataStable).
          asm volatile("nop"); // 1 NOP is enough (determined experimentally)

          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(2 * n_remaining));
          asm volatile("vfwdotp.vf  v8, %0, v18" :: "f"(a0));
          asm volatile("vfwdotp.vf v10, %0, v18" :: "f"(a1));
          asm volatile("vfwdotp.vf v12, %0, v18" :: "f"(a2));
          asm volatile("vfwdotp.vf v14, %0, v18" :: "f"(a3));

          a_ += 2; // next column
          b_ += 2 * N; // next row
        }

        // scaling

        // load operand: a_scale[m:m+4][k_block]
        const uint8_t *a_scale__ = a_scale_;
        uint8_t as0 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as1 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as2 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as3 = *(a_scale__);
        // load operands: b_scale[k_block][n:n+n_vl] -> v16
        asm volatile("vsetvli zero, %0, e8, mf2, ta, ma" :: "r"(n_remaining));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_scale_));

        int16_t as0_rescaled = as0 - (2 * E8M0_BIAS - FP32_BIAS);
        int16_t as1_rescaled = as1 - (2 * E8M0_BIAS - FP32_BIAS);
        int16_t as2_rescaled = as2 - (2 * E8M0_BIAS - FP32_BIAS);
        int16_t as3_rescaled = as3 - (2 * E8M0_BIAS - FP32_BIAS);

        // widen to 16-bit unsigned integer -> v18-v19
        asm volatile("vwcvtu.x.x.v v18, v16");

        // add, re-bias for FP32 and widen to 32-bit -> v24-v25, ..., v30-v31
        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(n_remaining));
        asm volatile("vwadd.vx v24, v18, %0" :: "r"(as0_rescaled));
        asm volatile("vwadd.vx v26, v18, %0" :: "r"(as1_rescaled));
        asm volatile("vwadd.vx v28, v18, %0" :: "r"(as2_rescaled));
        asm volatile("vwadd.vx v30, v18, %0" :: "r"(as3_rescaled));

        // convert to FP32 using bit operations
        asm volatile("vsetvli zero, %0, e32, m2, ta, ma" :: "r"(n_remaining));
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

      float *c__ = c_;
      asm volatile("vse32.v v0, (%0)" :: "r"(c__));
      c__ += N;
      asm volatile("vse32.v v2, (%0)" :: "r"(c__));
      c__ += N;
      asm volatile("vse32.v v4, (%0)" :: "r"(c__));
      c__ += N;
      asm volatile("vse32.v v6, (%0)" :: "r"(c__));
    }

    // increment row
    c_m0 += 4 * N;
    a_m0 += 4 * K;
    a_scale_m0 += 4 * K_BLOCK;
  }
}

// - outer product: vectorizing along output rows (N dimension)
// - sdotp: uses custom ExSdotp instructions (vfwdotp.vv)
// - optimal data layout: c, a, a_scale, b_scale in row-major layout
//                        b in custom layout (TODO: explain this)
// - M, N, K > 0
// - 2x data reuse
// - for maximum throughput, N should be a multiple of (4 * VLEN / 32)
void mxfp8_matmul_fp32_outer_sdotp_lmul4_2x(float *c,
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
      uint32_t n_remaining = N - n;

      // post-scale accumulator: v0-v3, v4-v7 (2 rows)
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(n_vl) : "r"(n_remaining));
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v4, 0");

      float *c_ = c_m0 + n;       // c[m][n]
      const char *a_ = a_m0;      // a[m][0]
      const char *b_ = b + 2 * n; // b[0][n]
      const uint8_t *a_scale_ = a_scale_m0;                   // a_scale[m][0]
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n; // b_scale[0][n]

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator: v8-v11, v12-v15
        asm volatile("vsetvli zero, %0, e32, m8, ta, ma" :: "r"(-1));
        asm volatile("vmv.v.i v8, 0");

        for (uint32_t k_elem = 0; k_elem < MXFP8_BLOCK_SIZE; k_elem += 2) {

          // load operands: a[m:m+2][k:k+2] and b[k:k+2][n:n+n_vl]
          // BUG: There is an issue with a RAW hazard between the VLSU and the
          //      VFU when using 2-element 8-bit loads. Due to this, use a
          //      single 16-bit load, which completes in a single cycle (and is
          //      also more efficient). This bug only occurs if the address is
          //      not 64-bit aligned.
          asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(1));
          const char *a__ = a_;
          asm volatile("vle16.v v18, (%0)" :: "r"(a__));
          a__ += K;
          asm volatile("vle16.v v19, (%0)" :: "r"(a__));

          // widen to FP16
          asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(2));
          asm volatile("vfwadd.vf v24, v18, %0" :: "f"(0.0f));
          asm volatile("vfwadd.vf v26, v19, %0" :: "f"(0.0f));

          // load operands: b[k:k+2][n:n+n_vl]
          asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(2 * n_remaining));
          asm volatile("vle8.v v16, (%0)" :: "r"(b_));

          // widen to FP16
          asm volatile("vfwadd.vf v20, v16, %0" :: "f"(0.0f));

          // move a0, a1 to scalar registers (2x FP16 packed into FP32 register)
          float a0, a1;
          // NOTE:This should be required as per the RVV specification, but
          //      isn't. This is because vfmv.f.s always movs a 64-bit element
          //      (instead of considering SEW).
          // BUG: Adding this vsetvli instruction triggers a bug where the VFU
          //      doesn't handle back-pressure on the response properly, leading
          //      to one of the vfmv.f.s results being dropped and a subsequent
          //      deadlock (SVA: VfuRspDataStable).
          // asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(1));
          asm volatile("vfmv.f.s %0, v24" : "=f"(a0));
          asm volatile("vfmv.f.s %0, v26" : "=f"(a1));

          // widen, multiply, and accumulate operands (pre-scaling) to FP32
          asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(2 * n_remaining));
          asm volatile("vfwdotp.vf  v8, %0, v20" :: "f"(a0));
          asm volatile("vfwdotp.vf v12, %0, v20" :: "f"(a1));

          a_ += 2; // next column
          b_ += 2 * N; // next row
        }

        // scaling

        // load operand: a_scale[m:m+2][k_block]
        const uint8_t *a_scale__ = a_scale_;
        uint8_t as0 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as1 = *(a_scale__);
        // load operands: b_scale[k_block][n:n+n_vl] -> v16
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(n_remaining));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_scale_));

        int16_t as0_rescaled = as0 - (2 * E8M0_BIAS - FP32_BIAS);
        int16_t as1_rescaled = as1 - (2 * E8M0_BIAS - FP32_BIAS);

        // widen to 16-bit unsigned integer -> v18-v19
        asm volatile("vwcvtu.x.x.v v18, v16");

        // add, re-bias for FP32 and widen to 32-bit -> v24-v27, v28-v31
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(n_remaining));
        asm volatile("vwadd.vx v24, v18, %0" :: "r"(as0_rescaled));
        asm volatile("vwadd.vx v28, v18, %0" :: "r"(as1_rescaled));

        // convert to FP32 using bit operations
        asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(n_remaining));
        asm volatile("vsll.vi v24, v24, 23");
        asm volatile("vsll.vi v28, v28, 23");

        // post-scale acc += pre-scale acc * scale
        asm volatile("vfmacc.vv  v0,  v8, v24");
        asm volatile("vfmacc.vv  v4, v12, v28");

        a_scale_ += 1; // next column
        b_scale_ += N; // next row
      }

      float *c__ = c_;
      asm volatile("vse32.v v0, (%0)" :: "r"(c__));
      c__ += N;
      asm volatile("vse32.v v4, (%0)" :: "r"(c__));
    }

    // increment row
    c_m0 += 2 * N;
    a_m0 += 2 * K;
    a_scale_m0 += 2 * K_BLOCK;
  }
}

// - outer product: vectorizing along output rows (N dimension)
// - FP16: uses FP16ALT (BF16) accumulation
// - optimal data layout: c, a, a_scale, b_scale in row-major layout
//                        b in custom layout (TODO: explain this)
// - M, N, K > 0
// - 2x data reuse
// - for maximum throughput, N should be a multiple of (2 * VLEN / 16)
void mxfp8_matmul_fp16_outer_lmul2_4x(_Float16 *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  uint32_t n_vl;

  _Float16 *c_m0 = c;   // c[m][0]
  const char *a_m0 = a; // a[m][0]
  const uint8_t *a_scale_m0 = (const uint8_t*)a_scale; // a_scale[m][0]

  for (uint32_t m = 0; m < M; m += 4) {
    for (uint32_t n = 0; n < N; n += n_vl) {
      uint32_t n_remaining = N - n;

      // post-scale accumulator (FP16): v0-v1, v2-v3, v4-v5, v6-v7 (4 rows)
      asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(n_vl) : "r"(n_remaining));
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v2, 0");
      asm volatile("vmv.v.i v4, 0");
      asm volatile("vmv.v.i v6, 0");

      _Float16 *c_ = c_m0 + n;    // c[m][n]
      const char *a_ = a_m0;      // a[m][0]
      const char *b_ = b + n;     // b[0][n]
      const uint8_t *a_scale_ = a_scale_m0;                   // a_scale[m][0]
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n; // b_scale[0][n]

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator (FP16): v8-v9, v10-v11, v12-v13, v14-v15
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(n_remaining));

        // enable alternate FP16 (destination) format
        asm volatile("csrw fcsr, %0" :: "r"(FCSR_MODE_DST));

        // loop head

        // load operands: b[k:k+2][n:n+n_vl]
        asm volatile("vle8.v v16, (%0)" :: "r"(b_));
        b_ += N; // next row

        // load operands: a[m:m+4][k]
        const char *a__ = a_;
        double a0, a1, a2, a3; // actually contain FP8
        asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
        a__ += K;
        asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
        a__ += K;
        asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a__));
        a__ += K;
        asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a__));
        a_ += 1; // next column

        uint32_t k_elem = 0;

        while (k_elem < MXFP8_BLOCK_SIZE) {
          k_elem += 1;

          // load operands: b[k:k+2][n:n+n_vl]
          asm volatile("vle8.v v20, (%0)" :: "r"(b_));
          b_ += N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP16
          a__ = a_;
          if (k_elem == 1) {
            asm volatile("vfwmul.vf  v8, v16, %0" :: "f"(a0));
            asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
            a__ += K;
            asm volatile("vfwmul.vf v10, v16, %0" :: "f"(a1));
            asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
            a__ += K;
            asm volatile("vfwmul.vf v12, v16, %0" :: "f"(a2));
            asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a__));
            a__ += K;
            asm volatile("vfwmul.vf v14, v16, %0" :: "f"(a3));
            asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a__));
          } else {
            asm volatile("vfwmacc.vf  v8, %0, v16" :: "f"(a0));
            asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
            a__ += K;
            asm volatile("vfwmacc.vf v10, %0, v16" :: "f"(a1));
            asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
            a__ += K;
            asm volatile("vfwmacc.vf v12, %0, v16" :: "f"(a2));
            asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a__));
            a__ += K;
            asm volatile("vfwmacc.vf v14, %0, v16" :: "f"(a3));
            asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a__));
          }
          a_ += 1; // next column

          // unrolled: repeat loop body
          k_elem += 1;

          if (k_elem == MXFP8_BLOCK_SIZE)
            break;

          // load operands: b[k:k+2][n:n+n_vl]
          asm volatile("vle8.v v16, (%0)" :: "r"(b_));
          b_ += N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP16
          a__ = a_;
          asm volatile("vfwmacc.vf  v8, %0, v20" :: "f"(a0));
          asm volatile("flb %0, (%1)" : "=f"(a0) : "r"(a__));
          a__ += K;
          asm volatile("vfwmacc.vf v10, %0, v20" :: "f"(a1));
          asm volatile("flb %0, (%1)" : "=f"(a1) : "r"(a__));
          a__ += K;
          asm volatile("vfwmacc.vf v12, %0, v20" :: "f"(a2));
          asm volatile("flb %0, (%1)" : "=f"(a2) : "r"(a__));
          a__ += K;
          asm volatile("vfwmacc.vf v14, %0, v20" :: "f"(a3));
          asm volatile("flb %0, (%1)" : "=f"(a3) : "r"(a__));
          a_ += 1; // next column
        }

        // loop tail
        asm volatile("vfwmacc.vf  v8, %0, v20" :: "f"(a0));
        asm volatile("vfwmacc.vf v10, %0, v20" :: "f"(a1));
        asm volatile("vfwmacc.vf v12, %0, v20" :: "f"(a2));
        asm volatile("vfwmacc.vf v14, %0, v20" :: "f"(a3));

        // scaling

        // load operands: b_scale[k_block][n:n+n_vl] -> v16
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(n_remaining));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_scale_));

        // load operand: a_scale[m:m+4][k_block]
        const uint8_t *a_scale__ = a_scale_;
        uint8_t as0 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as1 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as2 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as3 = *(a_scale__);

        // enable alternate FP16 (source and destination) format
        asm volatile("csrw fcsr, %0" :: "r"(FCSR_MODE_DST | FCSR_MODE_SRC));

        // widen to 16-bit unsigned integer -> v18-v19
        asm volatile("vwcvtu.x.x.v v18, v16");

        // re-bias scales for FP16ALT
        int16_t as0_rescaled = as0 - (2 * E8M0_BIAS - FP16ALT_BIAS);
        int16_t as1_rescaled = as1 - (2 * E8M0_BIAS - FP16ALT_BIAS);
        int16_t as2_rescaled = as2 - (2 * E8M0_BIAS - FP16ALT_BIAS);
        int16_t as3_rescaled = as3 - (2 * E8M0_BIAS - FP16ALT_BIAS);

        // add scales -> v24-v25, ..., v30-v31
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(n_remaining));
        asm volatile("vadd.vx v24, v18, %0" :: "r"(as0_rescaled));
        asm volatile("vadd.vx v26, v18, %0" :: "r"(as1_rescaled));
        asm volatile("vadd.vx v28, v18, %0" :: "r"(as2_rescaled));
        asm volatile("vadd.vx v30, v18, %0" :: "r"(as3_rescaled));

        // convert to FP16ALT using bit operations
        asm volatile("vsll.vi v24, v24, 7");
        asm volatile("vsll.vi v26, v26, 7");
        asm volatile("vsll.vi v28, v28, 7");
        asm volatile("vsll.vi v30, v30, 7");

        // post-scale acc += pre-scale acc * scale
        if (k_block < K_BLOCK - 1) {
          asm volatile("vfmacc.vv  v0,  v8, v24");
          asm volatile("vfmacc.vv  v2, v10, v26");
          asm volatile("vfmacc.vv  v4, v12, v28");
          asm volatile("vfmacc.vv  v6, v14, v30");
        } else {
          // last iteration: issue stores
          _Float16 *c__ = c_;
          asm volatile("vfmacc.vv  v0,  v8, v24");
          asm volatile("vse16.v v0, (%0)" :: "r"(c__));
          c__ += N;

          asm volatile("vfmacc.vv  v2, v10, v26");
          asm volatile("vse16.v v2, (%0)" :: "r"(c__));
          c__ += N;

          asm volatile("vfmacc.vv  v4, v12, v28");
          asm volatile("vse16.v v4, (%0)" :: "r"(c__));
          c__ += N;

          asm volatile("vfmacc.vv  v6, v14, v30");
          asm volatile("vse16.v v6, (%0)" :: "r"(c__));
        }

        a_scale_ += 1; // next column
        b_scale_ += N; // next row
      }
    }

    // increment row
    c_m0 += 4 * N;
    a_m0 += 4 * K;
    a_scale_m0 += 4 * K_BLOCK;
  }
}

// - outer product: vectorizing along output rows (N dimension)
// - FP16: uses FP16ALT (BF16) accumulation
// - sdotp: uses custom ExSdotp instructions (vfwdotp.vv)
// - optimal data layout: c, a, a_scale, b_scale in row-major layout
//                        b in custom layout (TODO: explain this)
// - M, N, K > 0
// - 2x data reuse
// - for maximum throughput, N should be a multiple of (2 * VLEN / 16)
void mxfp8_matmul_fp16_outer_sdotp_lmul2_4x(_Float16 *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  uint32_t n_vl;

  _Float16 *c_m0 = c;   // c[m][0]
  const char *a_m0 = a; // a[m][0]
  const uint8_t *a_scale_m0 = (const uint8_t*)a_scale; // a_scale[m][0]

  for (uint32_t m = 0; m < M; m += 4) {
    for (uint32_t n = 0; n < N; n += n_vl) {
      uint32_t n_remaining = N - n;

      // post-scale accumulator (FP16): v0-v1, v2-v3, v4-v5, v6-v7 (4 rows)
      asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(n_vl) : "r"(n_remaining));
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v2, 0");
      asm volatile("vmv.v.i v4, 0");
      asm volatile("vmv.v.i v6, 0");

      _Float16 *c_ = c_m0 + n;    // c[m][n]
      const char *a_ = a_m0;      // a[m][0]
      const char *b_ = b + 2 * n; // b[0][n]
      const uint8_t *a_scale_ = a_scale_m0;                   // a_scale[m][0]
      const uint8_t *b_scale_ = (const uint8_t *)b_scale + n; // b_scale[0][n]

      for (uint32_t k_block = 0; k_block < K_BLOCK; k_block++) {
        // pre-scale accumulator (FP16): v8-v9, v10-v11, v12-v13, v14-v15
        asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(2 * n_remaining));

        // enable alternate FP16 (destination) format
        asm volatile("csrw fcsr, %0" :: "r"(FCSR_MODE_DST));

        // loop head

        // load operands: b[k:k+2][n:n+n_vl]
        asm volatile("vle8.v v16, (%0)" :: "r"(b_));
        b_ += 2 * N; // next row

        // load operands: a[m:m+4][k:k+2]
        const char *a__ = a_;
        double a0, a1, a2, a3; // actually contain 2x FP8 packed
        asm volatile("flh %0, (%1)" : "=f"(a0) : "r"(a__));
        asm volatile("vmv.v.i  v8, 0");
        a__ += K;
        asm volatile("flh %0, (%1)" : "=f"(a1) : "r"(a__));
        asm volatile("vmv.v.i v10, 0");
        a__ += K;
        asm volatile("flh %0, (%1)" : "=f"(a2) : "r"(a__));
        asm volatile("vmv.v.i v12, 0");
        a__ += K;
        asm volatile("flh %0, (%1)" : "=f"(a3) : "r"(a__));
        asm volatile("vmv.v.i v14, 0");
        a_ += 2; // next column

        uint32_t k_elem = 0;

        while (k_elem < MXFP8_BLOCK_SIZE) {
          k_elem += 2;

          // load operands: b[k:k+2][n:n+n_vl]
          asm volatile("vle8.v v20, (%0)" :: "r"(b_));
          b_ += 2 * N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP16
          a__ = a_;
          asm volatile("vfwdotp.vf  v8, %0, v16" :: "f"(a0));
          asm volatile("flh %0, (%1)" : "=f"(a0) : "r"(a__));
          a__ += K;
          asm volatile("vfwdotp.vf v10, %0, v16" :: "f"(a1));
          asm volatile("flh %0, (%1)" : "=f"(a1) : "r"(a__));
          a__ += K;
          asm volatile("vfwdotp.vf v12, %0, v16" :: "f"(a2));
          asm volatile("flh %0, (%1)" : "=f"(a2) : "r"(a__));
          a__ += K;
          asm volatile("vfwdotp.vf v14, %0, v16" :: "f"(a3));
          asm volatile("flh %0, (%1)" : "=f"(a3) : "r"(a__));
          a_ += 2; // next column

          // unrolled: repeat loop body
          k_elem += 2;

          if (k_elem == MXFP8_BLOCK_SIZE)
            break;

          // load operands: b[k:k+2][n:n+n_vl]
          asm volatile("vle8.v v16, (%0)" :: "r"(b_));
          b_ += 2 * N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP16
          a__ = a_;
          asm volatile("vfwdotp.vf  v8, %0, v20" :: "f"(a0));
          asm volatile("flh %0, (%1)" : "=f"(a0) : "r"(a__));
          a__ += K;
          asm volatile("vfwdotp.vf v10, %0, v20" :: "f"(a1));
          asm volatile("flh %0, (%1)" : "=f"(a1) : "r"(a__));
          a__ += K;
          asm volatile("vfwdotp.vf v12, %0, v20" :: "f"(a2));
          asm volatile("flh %0, (%1)" : "=f"(a2) : "r"(a__));
          a__ += K;
          asm volatile("vfwdotp.vf v14, %0, v20" :: "f"(a3));
          asm volatile("flh %0, (%1)" : "=f"(a3) : "r"(a__));
          a_ += 2; // next column
        }

        // loop tail
        asm volatile("vfwdotp.vf  v8, %0, v20" :: "f"(a0));
        asm volatile("vfwdotp.vf v10, %0, v20" :: "f"(a1));
        asm volatile("vfwdotp.vf v12, %0, v20" :: "f"(a2));
        asm volatile("vfwdotp.vf v14, %0, v20" :: "f"(a3));

        // scaling

        // load operands: b_scale[k_block][n:n+n_vl] -> v16
        asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"(n_remaining));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_scale_));

        // load operand: a_scale[m:m+4][k_block]
        const uint8_t *a_scale__ = a_scale_;
        uint8_t as0 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as1 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as2 = *(a_scale__);
        a_scale__  += K_BLOCK;
        uint8_t as3 = *(a_scale__);

        // enable alternate FP16 (source and destination) format
        asm volatile("csrw fcsr, %0" :: "r"(FCSR_MODE_DST | FCSR_MODE_SRC));

        // widen to 16-bit unsigned integer -> v18-v19
        asm volatile("vwcvtu.x.x.v v18, v16");

        // re-bias scales for FP16ALT
        int16_t as0_rescaled = as0 - (2 * E8M0_BIAS - FP16ALT_BIAS);
        int16_t as1_rescaled = as1 - (2 * E8M0_BIAS - FP16ALT_BIAS);
        int16_t as2_rescaled = as2 - (2 * E8M0_BIAS - FP16ALT_BIAS);
        int16_t as3_rescaled = as3 - (2 * E8M0_BIAS - FP16ALT_BIAS);

        // add scales -> v24-v25, ..., v30-v31
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(n_remaining));
        asm volatile("vadd.vx v24, v18, %0" :: "r"(as0_rescaled));
        asm volatile("vadd.vx v26, v18, %0" :: "r"(as1_rescaled));
        asm volatile("vadd.vx v28, v18, %0" :: "r"(as2_rescaled));
        asm volatile("vadd.vx v30, v18, %0" :: "r"(as3_rescaled));

        // convert to FP16ALT using bit operations
        asm volatile("vsll.vi v24, v24, 7");
        asm volatile("vsll.vi v26, v26, 7");
        asm volatile("vsll.vi v28, v28, 7");
        asm volatile("vsll.vi v30, v30, 7");

        // post-scale acc += pre-scale acc * scale
        if (k_block < K_BLOCK - 1) {
          asm volatile("vfmacc.vv  v0,  v8, v24");
          asm volatile("vfmacc.vv  v2, v10, v26");
          asm volatile("vfmacc.vv  v4, v12, v28");
          asm volatile("vfmacc.vv  v6, v14, v30");
        } else {
          // last iteration: issue stores
          _Float16 *c__ = c_;
          asm volatile("vfmacc.vv  v0,  v8, v24");
          asm volatile("vse16.v v0, (%0)" :: "r"(c__));
          c__ += N;

          asm volatile("vfmacc.vv  v2, v10, v26");
          asm volatile("vse16.v v2, (%0)" :: "r"(c__));
          c__ += N;

          asm volatile("vfmacc.vv  v4, v12, v28");
          asm volatile("vse16.v v4, (%0)" :: "r"(c__));
          c__ += N;

          asm volatile("vfmacc.vv  v6, v14, v30");
          asm volatile("vse16.v v6, (%0)" :: "r"(c__));
        }

        a_scale_ += 1; // next column
        b_scale_ += N; // next row
      }
    }

    // increment row
    c_m0 += 4 * N;
    a_m0 += 4 * K;
    a_scale_m0 += 4 * K_BLOCK;
  }
}

#define VMXDOTP_WF_V0_F0_V16_F8_V29   asm volatile (".word 0xeb00005f")
#define VMXDOTP_WF_V2_F1_V16_F9_V29   asm volatile (".word 0xeb00915f")
#define VMXDOTP_WF_V4_F2_V16_F10_V29  asm volatile (".word 0xeb01225f")
#define VMXDOTP_WF_V6_F3_V16_F11_V29  asm volatile (".word 0xeb01b35f")
#define VMXDOTP_WF_V8_F4_V16_F12_V29  asm volatile (".word 0xeb02445f")
#define VMXDOTP_WF_V10_F5_V16_F13_V29 asm volatile (".word 0xeb02d55f")
#define VMXDOTP_WF_V12_F6_V16_F14_V29 asm volatile (".word 0xeb03665f")
#define VMXDOTP_WF_V14_F7_V16_F15_V29 asm volatile (".word 0xeb03f75f")

#define VMXDOTP_WF_V0_F0_V20_F8_V29   asm volatile (".word 0xeb40005f")
#define VMXDOTP_WF_V2_F1_V20_F9_V29   asm volatile (".word 0xeb40915f")
#define VMXDOTP_WF_V4_F2_V20_F10_V29  asm volatile (".word 0xeb41225f")
#define VMXDOTP_WF_V6_F3_V20_F11_V29  asm volatile (".word 0xeb41b35f")
#define VMXDOTP_WF_V8_F4_V20_F12_V29  asm volatile (".word 0xeb42445f")
#define VMXDOTP_WF_V10_F5_V20_F13_V29 asm volatile (".word 0xeb42d55f")
#define VMXDOTP_WF_V12_F6_V20_F14_V29 asm volatile (".word 0xeb43665f")
#define VMXDOTP_WF_V14_F7_V20_F15_V29 asm volatile (".word 0xeb43f75f")

// - outer product: vectorizing along output rows (N dimension)
// - natural data layout: c, a, a_scale, b_scale in row-major order
//                        b in column-major order
// - accumulator EMUL = 2, element EMUL = 4, scale EMUL = 1/2
// - TODO: data reuse
// - TODO: maximum throughput
void mxfp8_matmul_fp32_outer_mxdotp_lmul2_8x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  // makes compiler avoid special cases on loop entries
  if (M == 0 || N == 0 || K == 0)
    return;

  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  // keep v30-v31 as constant zero register
  asm volatile("vsetvli zero, %0, e32, m2, ta, ma" :: "r"(-1));
  asm volatile("vmv.v.i v30, 0");

  float *c_m_0 = c;      // c[m][0]
  const char *a_m_0 = a; // a[m][0]
  const uint8_t *a_scale_m_0 = (const uint8_t *)a_scale;

  for (uint32_t m = 0; m < M; m += 8) {
    const char *b_n_0 = b; // b[col][row] = b[n][0]

    uint32_t n_vl;

    for (uint32_t n = 0; n < N; n += n_vl) {
      float *c_ = c_m_0 + n;                                  // c[row=m][col=n]
      const char *a_ = a_m_0;                                 // b[row=m][col=0]
      const char *b_ = b_n_0;                                 // b[col=n][row=0]
      const uint8_t* a_scale_ = a_scale_m_0;                  // a_scale[row=m][col=0]
      const uint8_t* b_scale_ = (const uint8_t *)b_scale + n; // b_scale[row=0][col=n]

      // 8x FP32 accumulators: v0-v1, v2-v3, ..., v14-v15 (EMUL=2)

      asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(n_vl) : "r"(N - n));

      // 2x FP8 operands:   v16-v19, v20-v23 (EMUL=4)
      // 1x scale operands: v29              (EMUL=1/2)

      // FP8 operands (scalar)
      register double a0  asm ("f0");
      register double a1  asm ("f1");
      register double a2  asm ("f2");
      register double a3  asm ("f3");
      register double a4  asm ("f4");
      register double a5  asm ("f5");
      register double a6  asm ("f6");
      register double a7  asm ("f7");
      register double as0 asm ("f8");
      register double as1 asm ("f9");
      register double as2 asm ("f10");
      register double as3 asm ("f11");
      register double as4 asm ("f12");
      register double as5 asm ("f13");
      register double as6 asm ("f14");
      register double as7 asm ("f15");

      uint32_t k = 0;

      // iter 0: load b (vector)
      asm volatile("vlse64.v v16, (%0), %1" :: "r"(b_), "r"(K)); // EMUL=4
      b_ += 8; // next 8 rows

      // iter 0: load a (scalar)
      const char *a__ = a_;
      asm volatile("vfadd.vv  v0, v30, v30");
      asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a__));
      asm volatile("add %0, %0, %1" : "+r"(a__) : "r"(K));
      asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
      a__ += K;
      asm volatile("vfadd.vv  v2, v30, v30");
      asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
      a__ += K;
      asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
      a__ += K;
      asm volatile("vfadd.vv  v4, v30, v30");
      asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
      a__ += K;
      asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
      a__ += K;
      asm volatile("vfadd.vv  v6, v30, v30");
      asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
      a__ += K;
      asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
      a_ += 8; // next 8 columns

      // iter 0: load b_scale (vector)
      asm volatile("vle8.v  v29, (%0)" :: "r"(b_scale_)); // EMUL=1/2
      b_scale_ += N; // next row

      // iter 0: load a_scale (scalar)
      const uint8_t *a_scale__ = a_scale_;
      asm volatile("vfadd.vv  v8, v30, v30");
      asm volatile("flb %0, (%1)" : "=f"(as0) : "r"(a_scale__));
      asm volatile("add %0, %0, %1" : "+r"(a_scale__) : "r"(K_BLOCK));
      asm volatile("flb %0, (%1)" : "=f"(as1) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("vfadd.vv v10, v30, v30");
      asm volatile("flb %0, (%1)" : "=f"(as2) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("flb %0, (%1)" : "=f"(as3) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("vfadd.vv v12, v30, v30");
      asm volatile("flb %0, (%1)" : "=f"(as4) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("flb %0, (%1)" : "=f"(as5) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("vfadd.vv v14, v30, v30");
      asm volatile("flb %0, (%1)" : "=f"(as6) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("flb %0, (%1)" : "=f"(as7) : "r"(a_scale__));
      a_scale_++; // next column

      while (k < K) {
        k += 8;

        // load b (vector)
        asm volatile("vlse64.v v20, (%0), %1" :: "r"(b_), "r"(K)); // EMUL=4
        b_ += 8; // next 8 rows

        // mxdotp + load a (scalar)
        a__ = a_;
        asm volatile("vmxdotp.wf v0, %0, v16, %1, v29" :: "f"(a0), "f"(as0));
        asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a__));
        asm volatile("add %0, %0, %1" : "+r"(a__) : "r"(K));
        asm volatile("vmxdotp.wf  v2, %0, v16, %1, v29" :: "f"(a1), "f"(as1));
        asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
        a__ += K;
        asm volatile("vmxdotp.wf  v4, %0, v16, %1, v29" :: "f"(a2), "f"(as2));
        asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
        a__ += K;
        asm volatile("vmxdotp.wf  v6, %0, v16, %1, v29" :: "f"(a3), "f"(as3));
        asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
        a__ += K;
        asm volatile("vmxdotp.wf  v8, %0, v16, %1, v29" :: "f"(a4), "f"(as4));
        asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
        a__ += K;
        asm volatile("vmxdotp.wf v10, %0, v16, %1, v29" :: "f"(a5), "f"(as5));
        asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
        a__ += K;
        asm volatile("vmxdotp.wf v12, %0, v16, %1, v29" :: "f"(a6), "f"(as6));
        asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
        a__ += K;
        asm volatile("vmxdotp.wf v14, %0, v16, %1, v29" :: "f"(a7), "f"(as7));
        asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
        a_ += 8; // next 8 columns

        k += 8;

        if (k == K)
          break;

        // load b (vector)
        asm volatile("vlse64.v v16, (%0), %1" :: "r"(b_), "r"(K)); // EMUL=4
        b_ += 8; // next 8 rows

        if (k % 32 == 0) {
          // mxdotp + load a (scalar) + load a_scale (scalar)
          a__ = a_;
          a_scale__ = a_scale_;
          asm volatile("vmxdotp.wf  v0, %0, v20, %1, v29" :: "f"(a0), "f"(as0));
          asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a__));
          asm volatile("add %0, %0, %1" : "+r"(a__) : "r"(K));
          asm volatile("flb %0, (%1)" : "=f"(as0) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.wf  v2, %0, v20, %1, v29" :: "f"(a1), "f"(as1));
          asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
          a__ += K;
          asm volatile("flb %0, (%1)" : "=f"(as1) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.wf  v4, %0, v20, %1, v29" :: "f"(a2), "f"(as2));
          asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
          a__ += K;
          asm volatile("flb %0, (%1)" : "=f"(as2) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.wf  v6, %0, v20, %1, v29" :: "f"(a3), "f"(as3));
          asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
          a__ += K;
          asm volatile("flb %0, (%1)" : "=f"(as3) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.wf  v8, %0, v20, %1, v29" :: "f"(a4), "f"(as4));
          asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
          a__ += K;
          asm volatile("flb %0, (%1)" : "=f"(as4) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.wf v10, %0, v20, %1, v29" :: "f"(a5), "f"(as5));
          asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
          a__ += K;
          asm volatile("flb %0, (%1)" : "=f"(as5) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.wf v12, %0, v20, %1, v29" :: "f"(a6), "f"(as6));
          asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
          a__ += K;
          asm volatile("flb %0, (%1)" : "=f"(as6) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.wf v14, %0, v20, %1, v29" :: "f"(a7), "f"(as7));
          asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
          a_ += 8; // next 8 columns
          asm volatile("flb %0, (%1)" : "=f"(as7) : "r"(a_scale__));
          a_scale_++; // next column

          // load b_scale
          asm volatile("vle8.v  v29, (%0)" :: "r"(b_scale_)); // EMUL=1/2
          b_scale_ += N; // next row
        } else {
          // mxdotp + load a (scalar)
          a__ = a_;
          asm volatile("vmxdotp.wf  v0, %0, v20, %1, v29" :: "f"(a0), "f"(as0));
          asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a__));
          asm volatile("add %0, %0, %1" : "+r"(a__) : "r"(K));
          asm volatile("vmxdotp.wf  v2, %0, v20, %1, v29" :: "f"(a1), "f"(as1));
          asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
          a__ += K;
          asm volatile("vmxdotp.wf  v4, %0, v20, %1, v29" :: "f"(a2), "f"(as2));
          asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
          a__ += K;
          asm volatile("vmxdotp.wf  v6, %0, v20, %1, v29" :: "f"(a3), "f"(as3));
          asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
          a__ += K;
          asm volatile("vmxdotp.wf  v8, %0, v20, %1, v29" :: "f"(a4), "f"(as4));
          asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
          a__ += K;
          asm volatile("vmxdotp.wf v10, %0, v20, %1, v29" :: "f"(a5), "f"(as5));
          asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
          a__ += K;
          asm volatile("vmxdotp.wf v12, %0, v20, %1, v29" :: "f"(a6), "f"(as6));
          asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
          a__ += K;
          asm volatile("vmxdotp.wf v14, %0, v20, %1, v29" :: "f"(a7), "f"(as7));
          asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
          a_ += 8; // next 8 columns
        }
      }

      asm volatile("vmxdotp.wf  v0, %0, v20, %1, v29" :: "f"(a0), "f"(as0));
      asm volatile("vse32.v  v0, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.wf  v2, %0, v20, %1, v29" :: "f"(a1), "f"(as1));
      asm volatile("vse32.v  v2, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.wf  v4, %0, v20, %1, v29" :: "f"(a2), "f"(as2));
      asm volatile("vse32.v  v4, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.wf  v6, %0, v20, %1, v29" :: "f"(a3), "f"(as3));
      asm volatile("vse32.v  v6, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.wf  v8, %0, v20, %1, v29" :: "f"(a4), "f"(as4));
      asm volatile("vse32.v  v8, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.wf v10, %0, v20, %1, v29" :: "f"(a5), "f"(as5));
      asm volatile("vse32.v v10, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.wf v12, %0, v20, %1, v29" :: "f"(a6), "f"(as6));
      asm volatile("vse32.v v12, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.wf v14, %0, v20, %1, v29" :: "f"(a7), "f"(as7));
      asm volatile("vse32.v v14, (%0)" :: "r"(c_));

      // next col C, B, and B_scale
      // NOTE: These are expensive multiplications, but maybe we can simply
      //       continue adding (as we do in the k_block loop).
      b_n_0 += n_vl * K;
    }

    // next row in C, A, and A_scale
    c_m_0 += 8 * N;
    a_m_0 += 8 * K;
    a_scale_m_0 += 8 * K_BLOCK;
  }
}

// WARN: This is just for performance tests, and will produce wrong results.
//
// - outer product: vectorizing along output rows (N dimension)
// - natural data layout: c, a, a_scale, b_scale in row-major order
//                        b in column-major order
// - accumulator EMUL = 2, element EMUL = 4, scale EMUL = 1/2
// - TODO: constraints like M, N, K > 0
void mxfp8_matmul_dummyperf_lmul2_4x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  uint32_t K_BLOCK = K / MXFP8_BLOCK_SIZE;

  // keep v30-v31 as constant zero register
  asm volatile("vsetvli zero, %0, e32, m2, ta, ma" :: "r"(-1));
  asm volatile("vmv.v.i v30, 0");

  float *c_m_0 = c;      // c[m][0]
  const char *a_m_0 = a; // a[m][0]
  const uint8_t *a_scale_m_0 = (const uint8_t *)a_scale;

  for (uint32_t m = 0; m < M; m += 4) {
    float *c_m_n = c_m_0;
    const char *b_n_0 = b; // b[col][row] = b[n][0]
    const uint8_t *b_scale_0_n = (const uint8_t *)b_scale;

    uint32_t n_vl;

    for (uint32_t n = 0; n < N; n += n_vl) {
      float *c_ = c_m_0 + n;
      const char *a_ = a_m_0;
      const char *b_ = b + 8 * n;
      const uint8_t* a_scale_ = a_scale_m_0;
      const uint8_t* b_scale_ = b_scale_0_n;

      asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(n_vl) : "r"(N - n));

      // 4x FP32 accumulators: v0-v1, v2-v3, v4-v5, v6-v7 (EMUL=2)
      asm volatile("vfadd.vv  v0, v30, v30");
      asm volatile("vfadd.vv  v2, v30, v30");
      asm volatile("vfadd.vv  v4, v30, v30");
      asm volatile("vfadd.vv  v6, v30, v30");

      // NOTE: This will not be required for the real vmxdotp instruction.
      asm volatile("vsetvli zero, %0, e64, m4, ta, ma" :: "r"(N - n));

      // 2x FP8 operands:   v16-v19, v20-v23 (EMUL=4)
      // 1x scale operands: v29              (EMUL=1/2)

      // FP8 operands (scalar)
      double a0, a1, a2, a3;
      double as0, as1, as2, as3;

      uint32_t k = 0;

      // iter 0: load b (vector)
      asm volatile("vlse64.v v16, (%0), %1" :: "r"(b_), "r"(K / 8)); // EMUL=4
      b_ += 8; // next 8 rows

      // iter 0: load a (scalar)
      const char *a__ = a_;
      asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a__));
      a__ += K;
      asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
      a__ += K;
      asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
      a__ += K;
      asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
      a_ += 8; // next 8 columns

      // iter 0: load b_scale (vector)
      asm volatile("vle8.v  v29, (%0)" :: "r"(b_scale_)); // EMUL=1/2
      b_scale_ += N; // next row

      // iter 0: load a_scale (scalar)
      const uint8_t *a_scale__ = a_scale_;
      asm volatile("flb %0, (%1)" : "=f"(as0) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("flb %0, (%1)" : "=f"(as1) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("flb %0, (%1)" : "=f"(as2) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("flb %0, (%1)" : "=f"(as3) : "r"(a_scale__));
      a_scale_ += 1; // next column

      while (k < K) {
        k += 8;

        // load b (vector)
        asm volatile("vlse64.v v20, (%0), %1" :: "r"(b_), "r"(K / 8)); // EMUL=4
        b_ += 8; // next 8 rows

        // mxdotp + load a (scalar)
        a__ = a_;
        asm volatile("vfmacc.vf  v0, %0, v16" :: "f"(a0));
        asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a__));
        a__ += K;
        asm volatile("vfmacc.vf  v4, %0, v16" :: "f"(a1));
        asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
        a__ += K;
        asm volatile("vfmacc.vf  v0, %0, v16" :: "f"(a2));
        asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
        a__ += K;
        asm volatile("vfmacc.vf  v4, %0, v16" :: "f"(a3));
        asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
        a_ += 8; // next 8 columns

        k += 8;

        if (k == K)
          break;

        // load b (vector)
        asm volatile("vlse64.v v16, (%0), %1" :: "r"(b_), "r"(K / 8)); // EMUL=4
        b_ += 8; // next 8 rows

        // mxdotp + load a (scalar)
        a__ = a_;
        asm volatile("vfmacc.vf  v0, %0, v20" :: "f"(a0));
        asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a__));
        a__ += K;
        asm volatile("vfmacc.vf  v4, %0, v20" :: "f"(a1));
        asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
        a__ += K;
        asm volatile("vfmacc.vf  v8, %0, v20" :: "f"(a2));
        asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
        a__ += K;
        asm volatile("vfmacc.vf v12, %0, v20" :: "f"(a3));
        asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
        a_ += 8; // next 8 columns

        if (k % 32 == 0) {
          // load next scales

          asm volatile("vle8.v  v29, (%0)" :: "r"(b_scale_)); // EMUL=1/2
          b_scale_ += N; // next row

          // iter 0: load a_scale (scalar)
          a_scale__ = a_scale_;
          asm volatile("flb %0, (%1)" : "=f"(as0) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("flb %0, (%1)" : "=f"(as1) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("flb %0, (%1)" : "=f"(as2) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("flb %0, (%1)" : "=f"(as3) : "r"(a_scale__));
          a_scale_ += 1; // next column
        }
      }

      asm volatile("vfmacc.vf  v0, %0, v20" :: "f"(a0));
      asm volatile("vse32.v  v0, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vfmacc.vf  v4, %0, v20" :: "f"(a1));
      asm volatile("vse32.v  v4, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vfmacc.vf  v0, %0, v20" :: "f"(a2));
      asm volatile("vse32.v  v0, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vfmacc.vf  v4, %0, v20" :: "f"(a3));
      asm volatile("vse32.v  v4, (%0)" :: "r"(c_));

      // next col C, B, and B_scale
      c_m_n += n_vl;
      // NOTE: These are expensive multiplications, but maybe we can simply
      //       continue adding (as we do in the k_block loop).
      b_n_0 += n_vl * K;
      b_scale_0_n += n_vl;
    }

    // next row in C, A, and A_scale
    c_m_0 += 4 * N;
    a_m_0 += 4 * K;
    a_scale_m_0 += 4 * K_BLOCK;
  }
}
