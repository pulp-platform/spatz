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

#include "baseline_sdotp.h"
#include "../../mx_layer.h"

// vector register allocation:
// - post-scale accumulator: 4x EMUL=2: v0-v1, v2-v3, v4-v5, v6-v7
// - pre-scale accumulator:  4x EMUL=2: v8-v9, v10-v11, v12-v13, v14-v15
// - ...
//
// - 4x data reuse
// - For maximum throughput, N should be a multiple of (2 * VLEN / 16).
void mxfp8_matmul_bf16_outer_sdotp_lmul2_4x(_Float16 *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  // makes compiler avoid special cases on loop entries
  if (M == 0 || N == 0 || K == 0 || K % MX_BLOCK_SIZE != 0)
    return;

  uint32_t K_BLOCK = K / MX_BLOCK_SIZE;

  uint32_t n_vl;

  _Float16 *c_m0 = c;   // c[m][0]
  const char *a_m0 = a; // a[m][0]
  const uint8_t *a_scale_m0 = (const uint8_t*)a_scale; // a_scale[m][0]

  for (uint32_t m = 0; m < M; m += 4) {
    for (uint32_t n = 0; n < N; n += n_vl) {
      uint32_t n_remaining = N - n;

      // post-scale accumulator (FP16): v0-v1, v2-v3, v4-v5, v6-v7
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

        // enable alternate FP destination format (BF16)
        asm volatile("csrw fcsr, %0" :: "r"(FCSR_MODE_DST));

        // loop head

        // load operands: b[k:k+2][n:n+n_vl]
        asm volatile("vle8.v v16, (%0)" :: "r"(b_));
        b_ += 2 * N; // next row

        // load operands: a[m:m+4][k:k+2]
        double a0, a1, a2, a3; // actually contain 2x FP8 packed
        asm volatile("flh %0, (%1)" : "=f"(a0) : "r"(a_));
        asm volatile("vmv.v.i  v8, 0");
        const char *a__ = a_ + K;
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

        while (k_elem < MX_BLOCK_SIZE) {
          k_elem += 2;

          // load operands: b[k:k+2][n:n+n_vl]
          asm volatile("vle8.v v20, (%0)" :: "r"(b_));
          b_ += 2 * N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP16
          asm volatile("vfwdotp.vf  v8, %0, v16" :: "f"(a0));
          asm volatile("flh %0, (%1)" : "=f"(a0) : "r"(a_));
          a__ = a_ + K;
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

          if (k_elem == MX_BLOCK_SIZE)
            break;

          // load operands: b[k:k+2][n:n+n_vl]
          asm volatile("vle8.v v16, (%0)" :: "r"(b_));
          b_ += 2 * N; // next row

          // widen, multiply, and accumulate operands (pre-scaling) to FP16
          asm volatile("vfwdotp.vf  v8, %0, v20" :: "f"(a0));
          asm volatile("flh %0, (%1)" : "=f"(a0) : "r"(a_));
          a__ = a_ + K;
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

        // enable alternate FP source and destination formats (BF16)
        asm volatile("csrw fcsr, %0" :: "r"(FCSR_MODE_DST | FCSR_MODE_SRC));

        // widen to 16-bit unsigned integer -> v18-v19
        asm volatile("vwcvtu.x.x.v v18, v16");

        // re-bias scales for FP16ALT
        int16_t as0_rescaled = as0 - (2 * E8M0_BIAS - BF16_BIAS);
        int16_t as1_rescaled = as1 - (2 * E8M0_BIAS - BF16_BIAS);
        int16_t as2_rescaled = as2 - (2 * E8M0_BIAS - BF16_BIAS);
        int16_t as3_rescaled = as3 - (2 * E8M0_BIAS - BF16_BIAS);

        // add scales -> v24-v25, ..., v30-v31
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(n_remaining));
        asm volatile("vadd.vx v24, v18, %0" :: "r"(as0_rescaled));
        asm volatile("vadd.vx v26, v18, %0" :: "r"(as1_rescaled));
        asm volatile("vadd.vx v28, v18, %0" :: "r"(as2_rescaled));
        asm volatile("vadd.vx v30, v18, %0" :: "r"(as3_rescaled));

        // convert to BF16 using bit operations
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
