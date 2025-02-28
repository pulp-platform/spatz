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

#include "mxdotp.h"
#include "../../mx_layer.h"

// vector register allocation:
// - accumulator: 8x EMUL=  1: v0, v1, .., v7
// - elements:    2x EMUL=  4: v16-v19, v20-v23
// - scales:      2x EMUL=1/2: v28, v29
// - constant zero:            v31
__attribute__((noinline))
void mxfp4_matmul_bf16_mxdotp_lmul1_8x(_Float16 *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K)
{
  // makes compiler avoid special cases on loop entries
  if (M == 0 || N == 0 || K == 0 || K % MX_BLOCK_SIZE != 0)
    return;

  // enable FP4 source format and BF16 destination format
  asm volatile("csrs fcsr, %0" :: "r"(FCSR_MODE_SRC_FP4 | FCSR_MODE_DST));

  uint32_t K_BYTES = K / 2;  // number of bytes (FP4 -> 1/2 byte per element)
  uint32_t K_BLOCK = K / MX_BLOCK_SIZE;

  // keep v31 as constant zero register
  asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(-1));
  asm volatile("vmv.v.i v31, 0");

  _Float16 *c_m_0 = c;      // c[m][0]
  const char *a_m_0 = a; // a[m][0]
  const uint8_t *a_scale_m_0 = (const uint8_t *)a_scale;

  for (uint32_t m = 0; m < M; m += 8) {
    const char *b_n_0 = b; // b[col][row] = b[n][0]

    uint32_t n_vl;

    for (uint32_t n = 0; n < N; n += n_vl) {
      _Float16 *c_ = c_m_0 + n;                               // c[row=m][col=n]
      const char *a_ = a_m_0;                                 // b[row=m][col=0]
      const char *b_ = b_n_0;                                 // b[col=n][row=0]
      const uint8_t* a_scale_ = a_scale_m_0;                  // a_scale[row=m][col=0]
      const uint8_t* b_scale_ = (const uint8_t *)b_scale + n; // b_scale[row=0][col=n]

      asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(n_vl) : "r"(N - n));

      // FP4 16x packed operands
      double a0, a1, a2, a3, a4, a5, a6, a7;
      // E8M0 1x packed operands
      double as0, as1, as2, as3, as4, as5, as6, as7;

      uint32_t k = 0;

      // iter 0: load b (vector)
      asm volatile("vlse64.v v16, (%0), %1" :: "r"(b_), "r"(K_BYTES)); // EMUL=4
      b_ += 16 / 2; // next 16 rows

      // iter 0: load a (scalar)
      const char *a__;
      asm volatile("vfadd.vv v0, v31, v31");
      asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a_));
      asm volatile("add %0, %1, %2" : "=r"(a__) : "r"(a_), "r"(K_BYTES));
      asm volatile("vfadd.vv v1, v31, v31");
      asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vfadd.vv v2, v31, v31");
      asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vfadd.vv v3, v31, v31");
      asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vfadd.vv v4, v31, v31");
      asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vfadd.vv v5, v31, v31");
      asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vfadd.vv v6, v31, v31");
      asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vfadd.vv v7, v31, v31");
      asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
      a_ += 16 / 2; // next 16 columns

      // NOTE: We load the scale twice, once for use with v16 and once with v20.
      // This avoids stalls when "switching" to the next scale within the k-loop
      // below. Otherwise, two consecutive FPU instructions need to use the same
      // vector register but with different values (due to a load in-between),
      // which forces a pipeline stall until the load can complete.
      // This is not very energy-efficient, but easier than a vmv instruction
      // (as this would require vsetvli to change LMUL).

      // iter 0: load b_scale (vector)
      asm volatile("vle8.v v28, (%0)" :: "r"(b_scale_)); // EMUL=1/2
      asm volatile("vle8.v v29, (%0)" :: "r"(b_scale_)); // EMUL=1/2
      b_scale_ += N; // next row

      const uint8_t *a_scale__;

      // iter 0: load b (vector)
      asm volatile("vlse64.v v20, (%0), %1" :: "r"(b_), "r"(K_BYTES)); // EMUL=4
      b_ += 16 / 2; // next 16 rows

      // iter 0: load a_scale (scalar) + mxdotp + load next a (scalar)
      // load 2 scales initally to avoid RAW stalls
      asm volatile("flb %0, (%1)" : "=f"(as0) : "r"(a_scale_));
      asm volatile("add %0, %1, %2" : "=r"(a_scale__) : "r"(a_scale_), "r"(K_BLOCK));
      asm volatile("flb %0, (%1)" : "=f"(as1) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("vmxdotp.qf v0, %0, v16, %1, v28" :: "f"(a0), "f"(as0));
      asm volatile("flb %0, (%1)" : "=f"(as2) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a_));
      asm volatile("add %0, %1, %2" : "=r"(a__) : "r"(a_), "r"(K_BYTES));
      asm volatile("vmxdotp.qf v1, %0, v16, %1, v28" :: "f"(a1), "f"(as1));
      asm volatile("flb %0, (%1)" : "=f"(as3) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vmxdotp.qf v2, %0, v16, %1, v28" :: "f"(a2), "f"(as2));
      asm volatile("flb %0, (%1)" : "=f"(as4) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vmxdotp.qf v3, %0, v16, %1, v28" :: "f"(a3), "f"(as3));
      asm volatile("flb %0, (%1)" : "=f"(as5) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vmxdotp.qf v4, %0, v16, %1, v28" :: "f"(a4), "f"(as4));
      asm volatile("flb %0, (%1)" : "=f"(as6) : "r"(a_scale__));
      a_scale__ += K_BLOCK;
      asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vmxdotp.qf v5, %0, v16, %1, v28" :: "f"(a5), "f"(as5));
      asm volatile("flb %0, (%1)" : "=f"(as7) : "r"(a_scale__));
      a_scale_++; // next column
      asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vmxdotp.qf v6, %0, v16, %1, v28" :: "f"(a6), "f"(as6));
      asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
      a__ += K_BYTES;
      asm volatile("vmxdotp.qf v7, %0, v16, %1, v28" :: "f"(a7), "f"(as7));
      asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
      a_ += 16 / 2; // next 16 columns

      goto loop_in;

      while (true) {
        // load b (vector)
        asm volatile("vlse64.v v20, (%0), %1" :: "r"(b_), "r"(K_BYTES)); // EMUL=4
        b_ += 16 / 2; // next 16 rows

        // scales and elements already loaded into a0-a7 and as0-as7 in
        // previous loop iteration

        // mxdotp + load a (scalar)
        asm volatile("vmxdotp.qf v0, %0, v16, %1, v28" :: "f"(a0), "f"(as0));
        asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a_));
        asm volatile("add %0, %1, %2" : "=r"(a__) : "r"(a_), "r"(K_BYTES));
        asm volatile("vmxdotp.qf v1, %0, v16, %1, v28" :: "f"(a1), "f"(as1));
        asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
        a__ += K_BYTES;
        asm volatile("vmxdotp.qf v2, %0, v16, %1, v28" :: "f"(a2), "f"(as2));
        asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
        a__ += K_BYTES;
        asm volatile("vmxdotp.qf v3, %0, v16, %1, v28" :: "f"(a3), "f"(as3));
        asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
        a__ += K_BYTES;
        asm volatile("vmxdotp.qf v4, %0, v16, %1, v28" :: "f"(a4), "f"(as4));
        asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
        a__ += K_BYTES;
        asm volatile("vmxdotp.qf v5, %0, v16, %1, v28" :: "f"(a5), "f"(as5));
        asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
        a__ += K_BYTES;
        asm volatile("vmxdotp.qf v6, %0, v16, %1, v28" :: "f"(a6), "f"(as6));
        asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
        a__ += K_BYTES;
        asm volatile("vmxdotp.qf v7, %0, v16, %1, v28" :: "f"(a7), "f"(as7));
        asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
        a_ += 16 / 2; // next 16 columns

      loop_in:
        k += 32;
        b_n_0 += (32 / 2) * n_vl;

        if (k == K)
          break;

        // load b (vector)
        asm volatile("vlse64.v v16, (%0), %1" :: "r"(b_), "r"(K_BYTES)); // EMUL=4
        b_ += 16 / 2; // next 16 rows

        // mxdotp + load a (scalar)
        if (k % MX_BLOCK_SIZE == 0) {
          // load b_scale (for later)
          asm volatile("vle8.v v28, (%0)" :: "r"(b_scale_)); // EMUL=1/2

          asm volatile("vmxdotp.qf v0, %0, v20, %1, v29" :: "f"(a0), "f"(as0));
          asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a_));
          asm volatile("add %0, %1, %2" : "=r"(a__) : "r"(a_), "r"(K_BYTES));
          asm volatile("flb %0, (%1)" : "=f"(as0) : "r"(a_scale_));
          asm volatile("add %0, %1, %2" : "=r"(a_scale__) : "r"(a_scale_), "r"(K_BLOCK));
          asm volatile("vmxdotp.qf v1, %0, v20, %1, v29" :: "f"(a1), "f"(as1));
          asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("flb %0, (%1)" : "=f"(as1) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.qf v2, %0, v20, %1, v29" :: "f"(a2), "f"(as2));
          asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("flb %0, (%1)" : "=f"(as2) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.qf v3, %0, v20, %1, v29" :: "f"(a3), "f"(as3));
          asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("flb %0, (%1)" : "=f"(as3) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.qf v4, %0, v20, %1, v29" :: "f"(a4), "f"(as4));
          asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("flb %0, (%1)" : "=f"(as4) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.qf v5, %0, v20, %1, v29" :: "f"(a5), "f"(as5));
          asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("flb %0, (%1)" : "=f"(as5) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.qf v6, %0, v20, %1, v29" :: "f"(a6), "f"(as6));
          asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("flb %0, (%1)" : "=f"(as6) : "r"(a_scale__));
          a_scale__ += K_BLOCK;
          asm volatile("vmxdotp.qf v7, %0, v20, %1, v29" :: "f"(a7), "f"(as7));
          asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
          a_ += 16 / 2; // next 16 columns
          asm volatile("flb %0, (%1)" : "=f"(as7) : "r"(a_scale__));
          a_scale_++; // next column

          // load b_scale
          asm volatile("vle8.v v29, (%0)" :: "r"(b_scale_)); // EMUL=1/2
          b_scale_ += N; // next row

        } else {
          asm volatile("vmxdotp.qf v0, %0, v20, %1, v29" :: "f"(a0), "f"(as0));
          asm volatile("fld %0, (%1)" : "=f"(a0) : "r"(a_));
          asm volatile("add %0, %1, %2" : "=r"(a__) : "r"(a_), "r"(K_BYTES));
          asm volatile("vmxdotp.qf v1, %0, v20, %1, v29" :: "f"(a1), "f"(as1));
          asm volatile("fld %0, (%1)" : "=f"(a1) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("vmxdotp.qf v2, %0, v20, %1, v29" :: "f"(a2), "f"(as2));
          asm volatile("fld %0, (%1)" : "=f"(a2) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("vmxdotp.qf v3, %0, v20, %1, v29" :: "f"(a3), "f"(as3));
          asm volatile("fld %0, (%1)" : "=f"(a3) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("vmxdotp.qf v4, %0, v20, %1, v29" :: "f"(a4), "f"(as4));
          asm volatile("fld %0, (%1)" : "=f"(a4) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("vmxdotp.qf v5, %0, v20, %1, v29" :: "f"(a5), "f"(as5));
          asm volatile("fld %0, (%1)" : "=f"(a5) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("vmxdotp.qf v6, %0, v20, %1, v29" :: "f"(a6), "f"(as6));
          asm volatile("fld %0, (%1)" : "=f"(a6) : "r"(a__));
          a__ += K_BYTES;
          asm volatile("vmxdotp.qf v7, %0, v20, %1, v29" :: "f"(a7), "f"(as7));
          asm volatile("fld %0, (%1)" : "=f"(a7) : "r"(a__));
          a_ += 16 / 2; // next 16 columns
        }
      }

      asm volatile("vmxdotp.qf v0, %0, v20, %1, v29" :: "f"(a0), "f"(as0));
      asm volatile("vse16.v v0, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.qf v1, %0, v20, %1, v29" :: "f"(a1), "f"(as1));
      asm volatile("vse16.v v1, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.qf v2, %0, v20, %1, v29" :: "f"(a2), "f"(as2));
      asm volatile("vse16.v v2, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.qf v3, %0, v20, %1, v29" :: "f"(a3), "f"(as3));
      asm volatile("vse16.v v3, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.qf v4, %0, v20, %1, v29" :: "f"(a4), "f"(as4));
      asm volatile("vse16.v v4, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.qf v5, %0, v20, %1, v29" :: "f"(a5), "f"(as5));
      asm volatile("vse16.v v5, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.qf v6, %0, v20, %1, v29" :: "f"(a6), "f"(as6));
      asm volatile("vse16.v v6, (%0)" :: "r"(c_));
      c_ += N;
      asm volatile("vmxdotp.qf v7, %0, v20, %1, v29" :: "f"(a7), "f"(as7));
      asm volatile("vse16.v v7, (%0)" :: "r"(c_));
    }

    // next row in C, A, and A_scale
    c_m_0 += 8 * N;
    a_m_0 += 8 * K_BYTES;
    a_scale_m_0 += 8 * K_BLOCK;
  }
}
