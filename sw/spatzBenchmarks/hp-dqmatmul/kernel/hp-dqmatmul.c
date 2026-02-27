// Copyright 2026 ETH Zurich and University of Bologna.
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

// Author: Bowen Wang, ETH Zurich

#include "hp-dqmatmul.h"
#include <stddef.h>
#include <stdint.h>

#define CB_D   8u // 8 elements per codebook entry

// ---------------
// Solid 2xVL
// ---------------

void dq_matmul_2xVL(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0, const __fp16 *b_cb1,
                    const uint8_t *b_idx0, const uint8_t *b_idx1,
                    const unsigned int m_start, const unsigned int m_end,
                    const unsigned int N, const unsigned int P,
                    const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e16, m4, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    // pointer to codebook indices
    const uint8_t *b_idx0_ = b_idx0 + p / CB_D;
    const uint8_t *b_idx1_ = b_idx1 + p / CB_D;

    // store codebook base addr to t2 and t3
    register const __fp16 *cb0_reg asm("t2") = b_cb0;
    register const __fp16 *cb1_reg asm("t3") = b_cb1;
    asm volatile("" :: "r"(cb0_reg), "r"(cb1_reg));

    __fp16 *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 2) {
      const __fp16 *a_  = a + m * N;
      const __fp16 *a__ = a_;

      // preload codebook and dequantize
      asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(gvl / CB_D));
      asm volatile("vle8.v v16, (%0)" :: "r"(b_idx0_));
      asm volatile("vle8.v v18, (%0)" :: "r"(b_idx1_));

      asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(gvl));
      asm volatile("vlxblk8ei8.v v8,  (%0), v16" :: "r"(cb0_reg) : "memory");
      asm volatile("vlxblk8ei8.v v12, (%0), v18" :: "r"(cb1_reg) : "memory");
      asm volatile("vfadd.vv v8, v8, v12");

      // proceed pointers
      const uint8_t *b_idx0__ = b_idx0_ + P / CB_D;
      const uint8_t *b_idx1__ = b_idx1_ + P / CB_D;

      __fp16 *c__ = c_ + m * P;

      float t0, t1;

      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
      a__ -= (N-1);

      // pre-load and dequant b --> v20
      asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(gvl / CB_D));
      asm volatile("vle8.v v16, (%0)" :: "r"(b_idx0__));
      asm volatile("vle8.v v18, (%0)" :: "r"(b_idx1__));
      asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(gvl));
      asm volatile("vlxblk8ei8.v v20, (%0), v16" :: "r"(cb0_reg) : "memory");
      asm volatile("vlxblk8ei8.v v24, (%0), v18" :: "r"(cb1_reg) : "memory");
      asm volatile("vfadd.vv v20, v20, v24");
      b_idx0__ += P / CB_D;
      b_idx1__ += P / CB_D;
      // compute with v8
      asm volatile("vfmul.vf v0, v8, %0" :: "f"(t0));
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
      a__ += N;
      asm volatile("vfmul.vf v4, v8, %0" :: "f"(t1));
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
      a__ -= (N-1);

      unsigned int n = 1;

      while (n < N-1) {

        // pre-load and dequant b --> v8
        asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(gvl / CB_D));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_idx0__));
        b_idx0__ += P / CB_D;
        asm volatile("vle8.v v18, (%0)" :: "r"(b_idx1__));
        b_idx1__ += P / CB_D;
        asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(gvl));
        asm volatile("vlxblk8ei8.v v8, (%0), v16" :: "r"(cb0_reg) : "memory");

        // compute
        asm volatile("vfmacc.vf v0, %0, v20" :: "f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        asm volatile("vlxblk8ei8.v v12, (%0), v18" :: "r"(cb1_reg) : "memory");
        a__ += N;
        asm volatile("vfmacc.vf v4, %0, v20" :: "f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        asm volatile("vfadd.vv v8, v8, v12");
        a__ -= (N-1);


        asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(gvl / CB_D));
        asm volatile("vle8.v v16, (%0)" :: "r"(b_idx0__));
        b_idx0__ += P / CB_D;
        asm volatile("vle8.v v18, (%0)" :: "r"(b_idx1__));
        b_idx1__ += P / CB_D;
        asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(gvl));
        asm volatile("vlxblk8ei8.v v20,  (%0), v16" :: "r"(cb0_reg) : "memory");

        asm volatile("vfmacc.vf v0, %0, v8" :: "f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        asm volatile("vlxblk8ei8.v v24, (%0), v18" :: "r"(cb1_reg) : "memory");
        a__ += N;
        asm volatile("vfmacc.vf v4, %0, v8" :: "f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        asm volatile("vfadd.vv v20, v20, v24");
        a__ -= (N-1);

        n += 2;
      }

      asm volatile("vfmacc.vf v0, %0, v20" :: "f"(t0));
      asm volatile("vse16.v v0, (%0)" :: "r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v4, %0, v20" :: "f"(t1));
      asm volatile("vse16.v v4, (%0)" :: "r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 4xVL
// ---------------

void dq_matmul_4xVL(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0, const __fp16 *b_cb1,
                    const uint8_t *b_idx0, const uint8_t *b_idx1,
                    const unsigned int m_start, const unsigned int m_end,
                    const unsigned int N, const unsigned int P,
                    const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e16, m4, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    // pointer to codebook indices
    const uint8_t *b_idx0_ = b_idx0 + p / CB_D;
    const uint8_t *b_idx1_ = b_idx1 + p / CB_D;

    // store codebook base addr to t2 and t3
    register const __fp16 *cb0_reg asm("t2") = b_cb0;
    register const __fp16 *cb1_reg asm("t3") = b_cb1;
    asm volatile("" :: "r"(cb0_reg), "r"(cb1_reg));

    __fp16 *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 4) {
      const __fp16 *a_  = a + m * N;

      // preload codebook and dequantize
      asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(gvl / CB_D));
      asm volatile("vle8.v v28, (%0)" :: "r"(b_idx0_));
      asm volatile("vle8.v v30, (%0)" :: "r"(b_idx1_));

      asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(gvl));
      asm volatile("vlxblk8ei8.v v16,  (%0), v28" :: "r"(cb0_reg) : "memory");
      asm volatile("vlxblk8ei8.v v24, (%0), v30" :: "r"(cb1_reg) : "memory");
      asm volatile("vfadd.vv v16, v16, v24");

      // proceed pointers
      const uint8_t *b_idx0__ = b_idx0_ + P / CB_D;
      const uint8_t *b_idx1__ = b_idx1_ + P / CB_D;

      __fp16 *c__ = c_ + m * P;

      float t0, t1, t2, t3;

      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a_));
      a_ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a_));
      a_ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a_));
      a_ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a_));
      a_ -= (3*N-1);

      // pre-load and dequant b --> v20
      asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(gvl / CB_D));
      asm volatile("vle8.v v28, (%0)" :: "r"(b_idx0__));
      asm volatile("vle8.v v30, (%0)" :: "r"(b_idx1__));
      asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(gvl));
      asm volatile("vlxblk8ei8.v v20, (%0), v28" :: "r"(cb0_reg) : "memory");
      asm volatile("vlxblk8ei8.v v24, (%0), v30" :: "r"(cb1_reg) : "memory");
      asm volatile("vfadd.vv v20, v20, v24");
      b_idx0__ += P / CB_D;
      b_idx1__ += P / CB_D;
      // compute with v8
      asm volatile("vfmul.vf v0, v16, %0" :: "f"(t0));
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a_));
      a_ += N;
      asm volatile("vfmul.vf v4, v16, %0" :: "f"(t1));
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a_));
      a_ += N;
      asm volatile("vfmul.vf v8, v16, %0" :: "f"(t2));
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a_));
      a_ += N;
      asm volatile("vfmul.vf v12, v16, %0" :: "f"(t3));
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a_));
      a_ -= (3*N-1);

      unsigned int n = 1;

      while (n < N-1) {

        // pre-load and dequant b --> v8
        asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(gvl / CB_D));
        asm volatile("vle8.v v28, (%0)" :: "r"(b_idx0__));
        b_idx0__ += P / CB_D;
        asm volatile("vle8.v v30, (%0)" :: "r"(b_idx1__));
        b_idx1__ += P / CB_D;
        asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(gvl));
        asm volatile("vlxblk8ei8.v v16, (%0), v28" :: "r"(cb0_reg) : "memory");

        // compute
        asm volatile("vfmacc.vf v0, %0, v20" :: "f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a_));
        asm volatile("vlxblk8ei8.v v24, (%0), v30" :: "r"(cb1_reg) : "memory");
        a_ += N;
        asm volatile("vfmacc.vf v4, %0, v20" :: "f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a_));
        a_ += N;
        asm volatile("vfmacc.vf v8, %0, v20" :: "f"(t2));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a_));
        a_ += N;
        asm volatile("vfmacc.vf v12, %0, v20" :: "f"(t3));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a_));
        asm volatile("vfadd.vv v16, v16, v24");
        a_ -= (3*N-1);


        asm volatile("vsetvli zero, %0, e8, m2, ta, ma" :: "r"(gvl / CB_D));
        asm volatile("vle8.v v28, (%0)" :: "r"(b_idx0__));
        b_idx0__ += P / CB_D;
        asm volatile("vle8.v v30, (%0)" :: "r"(b_idx1__));
        b_idx1__ += P / CB_D;
        asm volatile("vsetvli zero, %0, e16, m4, ta, ma" :: "r"(gvl));
        asm volatile("vlxblk8ei8.v v20,  (%0), v28" :: "r"(cb0_reg) : "memory");

        asm volatile("vfmacc.vf v0, %0, v16" :: "f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a_));
        asm volatile("vlxblk8ei8.v v24, (%0), v30" :: "r"(cb1_reg) : "memory");
        a_ += N;
        asm volatile("vfmacc.vf v4, %0, v16" :: "f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a_));
        a_ += N;
        asm volatile("vfmacc.vf v8, %0, v16" :: "f"(t2));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a_));
        a_ += N;
        asm volatile("vfmacc.vf v12, %0, v16" :: "f"(t3));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a_));
        asm volatile("vfadd.vv v20, v20, v24");
        a_ -= (3*N-1);

        n += 2;
      }

      asm volatile("vfmacc.vf v0, %0, v20" :: "f"(t0));
      asm volatile("vse16.v v0, (%0)" :: "r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v4, %0, v20" :: "f"(t1));
      asm volatile("vse16.v v4, (%0)" :: "r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v8, %0, v20" :: "f"(t2));
      asm volatile("vse16.v v8, (%0)" :: "r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v12, %0, v20" :: "f"(t3));
      asm volatile("vse16.v v12, (%0)" :: "r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 8xVL
// ---------------