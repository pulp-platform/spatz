// Copyright 2023 ETH Zurich and University of Bologna.
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

// Author: Matheus Cavalcante, ETH Zurich

#include "sdotp-fmatmul.h"
#include <stddef.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void matmul(char *c, const char *a, const char *b, const unsigned int M,
            const unsigned int N, const unsigned int P) {
  if (M <= 4) {
    matmul_2xVL(c, a, b, 0, M, N, P, 0, P);
  } else if (M <= 8) {
    matmul_4xVL(c, a, b, 0, M, N, P, 0, P);
  } else {
    matmul_8xVL(c, a, b, 0, M, N, P, 0, P);
  }
}

// ---------------
// 2xVL
// ---------------

void matmul_2xVL(char *c, const char *a, const char *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e8, m8, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(2 * (p_end - p)));

    const char *b_ = b + 2 * p;
    char *c_ = c + p;

    // Account for the used operands
    p += gvl / 2;

    for (unsigned int m = m_start; m < m_end; m += 2) {
      asm volatile("vsetvli zero, %0, e8, m8, ta, ma" ::"r"(gvl));

      const char *a_ = a + m * N;
      const char *a__ = a_;

      asm volatile("vle8.v v16, (%0);" ::"r"(b_));
      const char *b__ = b_ + 2 * P;

      char *c__ = c_ + m * P;

      double t0, t1;

      asm volatile("vmv.v.x v0, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v8, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));

      unsigned int n = 0;

      while (n < N) {
        n += 2;
        a__ = a_ + n;

        asm volatile("vle8.v v24, (%0);" ::"r"(b__));
        b__ += 2 * P;

        asm volatile("vfwdotp.vf v0, %0, v16" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v8, %0, v16" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));

        n += 2;
        a__ = a_ + n;

        if (n == N)
          break;

        asm volatile("vle8.v v16, (%0);" ::"r"(b__));
        b__ += 2 * P;

        asm volatile("vfwdotp.vf v0, %0, v24" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v8, %0, v24" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
      }

      asm volatile("vfwdotp.vf v0, %0, v24" ::"f"(t0));
      asm volatile("vfwdotp.vf v8, %0, v24" ::"f"(t1));

      asm volatile("vsetvli zero, %0, e8, m8, ta, ma" ::"r"(gvl / 2));

      asm volatile("vfncvt.f.f.w v0, v0");
      asm volatile("vse8.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v8, v8");
      asm volatile("vse8.v v8, (%0);" ::"r"(c__));
    }
  }
}

// ---------------
// 4xVL
// ---------------

void matmul_4xVL(char *c, const char *a, const char *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e8, m4, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(2 * (p_end - p)));

    const char *b_ = b + 2 * p;
    char *c_ = c + p;

    // Account for the used operands
    p += gvl / 2;

    for (unsigned int m = m_start; m < m_end; m += 4) {
      asm volatile("vsetvli zero, %0, e8, m4, ta, ma" ::"r"(gvl));

      const char *a_ = a + m * N;
      const char *a__ = a_;

      asm volatile("vle8.v v16, (%0);" ::"r"(b_));
      const char *b__ = b_ + 2 * P;

      char *c__ = c_ + m * P;

      double t0, t1, t2, t3;

      asm volatile("vmv.v.x v0, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v4, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v8, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v12, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));

      unsigned int n = 0;

      while (n < N) {
        n += 2;
        a__ = a_ + n;

        asm volatile("vle8.v v20, (%0);" ::"r"(b__));
        b__ += 2 * P;

        asm volatile("vfwdotp.vf v0, %0, v16" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v4, %0, v16" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v8, %0, v16" ::"f"(t2));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v12, %0, v16" ::"f"(t3));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));

        n += 2;
        a__ = a_ + n;

        if (n == N)
          break;

        asm volatile("vle8.v v16, (%0);" ::"r"(b__));
        b__ += 2 * P;

        asm volatile("vfwdotp.vf v0, %0, v20" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v4, %0, v20" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v8, %0, v20" ::"f"(t2));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v12, %0, v20" ::"f"(t3));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
      }

      asm volatile("vfwdotp.vf v0, %0, v20" ::"f"(t0));
      asm volatile("vfwdotp.vf v4, %0, v20" ::"f"(t1));
      asm volatile("vfwdotp.vf v8, %0, v20" ::"f"(t2));
      asm volatile("vfwdotp.vf v12, %0, v20" ::"f"(t3));

      asm volatile("vsetvli zero, %0, e8, m4, ta, ma" ::"r"(gvl / 2));

      asm volatile("vfncvt.f.f.w v0, v0");
      asm volatile("vse8.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v4, v4");
      asm volatile("vse8.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v8, v8");
      asm volatile("vse8.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v12, v12");
      asm volatile("vse8.v v12, (%0);" ::"r"(c__));
    }
  }
}

// ---------------
// 8xVL
// ---------------

void matmul_8xVL(char *c, const char *a, const char *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e8, m2, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(2 * (p_end - p)));

    const char *b_ = b + 2 * p;
    char *c_ = c + p;

    // Account for the used operands
    p += gvl / 2;

    for (unsigned int m = m_start; m < m_end; m += 8) {
      asm volatile("vsetvli zero, %0, e8, m2, ta, ma" ::"r"(gvl));

      const char *a_ = a + m * N;
      const char *a__ = a_;

      asm volatile("vle8.v v18, (%0);" ::"r"(b_));
      const char *b__ = b_ + 2 * P;

      char *c__ = c_ + m * P;

      double t0, t1, t2, t3, t4, t5, t6, t7;

      asm volatile("vmv.v.x v0, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v2, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v4, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v6, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v8, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v10, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v12, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
      a__ += N;
      asm volatile("vmv.v.x v14, zero");
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));

      unsigned int n = 0;

      while (n < N) {
        n += 2;
        a__ = a_ + n;

        asm volatile("vle8.v v20, (%0);" ::"r"(b__));
        b__ += 2 * P;

        asm volatile("vfwdotp.vf v0, %0, v18" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v2, %0, v18" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v4, %0, v18" ::"f"(t2));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v6, %0, v18" ::"f"(t3));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v8, %0, v18" ::"f"(t4));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v10, %0, v18" ::"f"(t5));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v12, %0, v18" ::"f"(t6));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v14, %0, v18" ::"f"(t7));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));

        n += 2;
        a__ = a_ + n;

        if (n == N)
          break;

        asm volatile("vle8.v v18, (%0);" ::"r"(b__));
        b__ += 2 * P;

        asm volatile("vfwdotp.vf v0, %0, v20" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v2, %0, v20" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v4, %0, v20" ::"f"(t2));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v6, %0, v20" ::"f"(t3));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v8, %0, v20" ::"f"(t4));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v10, %0, v20" ::"f"(t5));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v12, %0, v20" ::"f"(t6));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfwdotp.vf v14, %0, v20" ::"f"(t7));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
      }

      asm volatile("vfwdotp.vf v0, %0, v20" ::"f"(t0));
      asm volatile("vfwdotp.vf v2, %0, v20" ::"f"(t1));
      asm volatile("vfwdotp.vf v4, %0, v20" ::"f"(t2));
      asm volatile("vfwdotp.vf v6, %0, v20" ::"f"(t3));
      asm volatile("vfwdotp.vf v8, %0, v20" ::"f"(t4));
      asm volatile("vfwdotp.vf v10, %0, v20" ::"f"(t5));
      asm volatile("vfwdotp.vf v12, %0, v20" ::"f"(t6));
      asm volatile("vfwdotp.vf v14, %0, v20" ::"f"(t7));

      asm volatile("vsetvli zero, %0, e8, m2, ta, ma" ::"r"(gvl / 2));

      asm volatile("vfncvt.f.f.w v0, v0");
      asm volatile("vse8.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v2, v2");
      asm volatile("vse8.v v2, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v4, v4");
      asm volatile("vse8.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v6, v6");
      asm volatile("vse8.v v6, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v8, v8");
      asm volatile("vse8.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v10, v10");
      asm volatile("vse8.v v10, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v12, v12");
      asm volatile("vse8.v v12, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v14, v14");
      asm volatile("vse8.v v14, (%0);" ::"r"(c__));
    }
  }
}
