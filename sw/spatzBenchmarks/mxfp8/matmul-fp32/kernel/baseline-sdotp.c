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

#include "baseline-sdotp.h"
#include "../../mx_layer.h"

// vector register allocation:
// - post-scale accumulator: 2x EMUL=4: v0-v3, v4-v7
// - pre-scale accumulator:  2x EMUL=4: v7-v11, v12-v15
// - ...
//
// - 2x data reuse
// - For maximum throughput, N should be a multiple of (4 * VLEN / 32).
__attribute__((noinline))
void mxfp8_matmul_fp32_sdotp_lmul4_2x(float *c,
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

        for (uint32_t k_elem = 0; k_elem < MX_BLOCK_SIZE; k_elem += 2) {

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

