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

#include "baseline.h"
#include "../../mx_layer.h"

// vector register allocation:
// - post-scale accumulator: 2x EMUL=4: v0-v3, v4-v7
// - pre-scale accumulator:  2x EMUL=4: v7-v11, v12-v15
// - ...
//
// - 2x data reuse
// - For maximum throughput, N should be a multiple of (4 * VLEN / 32).
__attribute__((noinline))
void mxfp8_matmul_fp32_outer_lmul4_2x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  // makes compiler avoid special cases on loop entries
  if (M == 0 || N == 0 || K == 0 || K % MX_BLOCK_SIZE != 0)
    return;

  uint32_t K_BLOCK = K / MX_BLOCK_SIZE;

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

        while (k_elem < MX_BLOCK_SIZE) {
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

          if (k_elem == MX_BLOCK_SIZE)
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
