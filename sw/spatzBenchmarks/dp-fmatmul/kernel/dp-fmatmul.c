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

#include "dp-fmatmul.h"
#include <stddef.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void matmul(double *c, const double *a, const double *b, const unsigned int M,
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

void matmul_2xVL(double *c, const double *a, const double *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e64, m8, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const double *b_ = b + p;
    double *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 2) {
      const double *a_ = a + m * N;
      const double *a__ = a_;

      asm volatile("vle64.v v16, (%0);" ::"r"(b_));
      const double *b__ = b_ + P;

      double *c__ = c_ + m * P;

      double t0, t1;

      t0 = *a__;
      a__ += N;
      t1 = *a__;

      unsigned int n = 0;

      while (n < N) {
        a__ = a_ + ++n;

        asm volatile("vle64.v v24, (%0);" ::"r"(b__));
        b__ += P;

        if (n == 1) {
          asm volatile("vfmul.vf v0, v16, %0" ::"f"(t0));
          t0 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v8, v16, %0" ::"f"(t1));
          t1 = *a__;
        } else {
          asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
          t0 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t1));
          t1 = *a__;
        }

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle64.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t0));
        t0 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v8, %0, v24" ::"f"(t1));
        t1 = *a__;
      }

      asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t0));
      asm volatile("vse64.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v8, %0, v24" ::"f"(t1));
      asm volatile("vse64.v v8, (%0);" ::"r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 4xVL
// ---------------

void matmul_4xVL(double *c, const double *a, const double *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e64, m4, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const double *b_ = b + p;
    double *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 4) {
      const double *a_ = a + m * N;
      const double *a__ = a_;

      asm volatile("vle64.v v16, (%0);" ::"r"(b_));
      const double *b__ = b_ + P;

      double *c__ = c_ + m * P;

      double t0, t1, t2, t3;

      t0 = *a__;
      a__ += N;
      t1 = *a__;
      a__ += N;
      t2 = *a__;
      a__ += N;
      t3 = *a__;

      unsigned int n = 0;

      while (n < N) {
        asm volatile("vle64.v v20, (%0);" ::"r"(b__));
        b__ += P;

        a__ = a_ + ++n;

        if (n == 1) {
          asm volatile("vfmul.vf v0, v16, %0" ::"f"(t0));
          t0 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v4, v16, %0" ::"f"(t1));
          t1 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v8, v16, %0" ::"f"(t2));
          t2 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v12, v16, %0" ::"f"(t3));
          t3 = *a__;
        } else {
          asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
          t0 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t1));
          t1 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t2));
          t2 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v12, %0, v16" ::"f"(t3));
          t3 = *a__;
        }

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle64.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
        t0 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t1));
        t1 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t2));
        t2 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t3));
        t3 = *a__;
      }

      asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
      asm volatile("vse64.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t1));
      asm volatile("vse64.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t2));
      asm volatile("vse64.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t3));
      asm volatile("vse64.v v12, (%0);" ::"r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 8xVL
// ---------------

void matmul_8xVL(double *c, const double *a, const double *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e64, m2, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const double *b_ = b + p;
    double *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 8) {
      const double *a_ = a + m * N;
      const double *a__ = a_;

      asm volatile("vle64.v v18, (%0);" ::"r"(b_));
      const double *b__ = b_ + P;

      double *c__ = c_ + m * P;

      double t0, t1, t2, t3, t4, t5, t6, t7;

      t0 = *a__;
      a__ += N;
      t1 = *a__;
      a__ += N;
      t2 = *a__;
      a__ += N;
      t3 = *a__;
      a__ += N;
      t4 = *a__;
      a__ += N;
      t5 = *a__;
      a__ += N;
      t6 = *a__;
      a__ += N;
      t7 = *a__;

      unsigned int n = 0;

      while (n < N) {
        a__ = a_ + ++n;

        asm volatile("vle64.v v20, (%0);" ::"r"(b__));
        b__ += P;

        if (n == 1) {
          asm volatile("vfmul.vf v0, v18, %0" ::"f"(t0));
          t0 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v2, v18, %0" ::"f"(t1));
          t1 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v4, v18, %0" ::"f"(t2));
          t2 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v6, v18, %0" ::"f"(t3));
          t3 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v8, v18, %0" ::"f"(t4));
          t4 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v10, v18, %0" ::"f"(t5));
          t5 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v12, v18, %0" ::"f"(t6));
          t6 = *a__;
          a__ += N;
          asm volatile("vfmul.vf v14, v18, %0" ::"f"(t7));
          t7 = *a__;
        } else {
          asm volatile("vfmacc.vf v0, %0, v18" ::"f"(t0));
          t0 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v2, %0, v18" ::"f"(t1));
          t1 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v4, %0, v18" ::"f"(t2));
          t2 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t3));
          t3 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v8, %0, v18" ::"f"(t4));
          t4 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v10, %0, v18" ::"f"(t5));
          t5 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v12, %0, v18" ::"f"(t6));
          t6 = *a__;
          a__ += N;
          asm volatile("vfmacc.vf v14, %0, v18" ::"f"(t7));
          t7 = *a__;
        }

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle64.v v18, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
        t0 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
        t1 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
        t2 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
        t3 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
        t4 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
        t5 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
        t6 = *a__;
        a__ += N;
        asm volatile("vfmacc.vf v14, %0, v20" ::"f"(t7));
        t7 = *a__;
      }

      asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
      asm volatile("vse64.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
      asm volatile("vse64.v v2, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
      asm volatile("vse64.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
      asm volatile("vse64.v v6, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
      asm volatile("vse64.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
      asm volatile("vse64.v v10, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
      asm volatile("vse64.v v12, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v14, %0, v20" ::"f"(t7));
      asm volatile("vse64.v v14, (%0);" ::"r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 16xVL
// ---------------

void matmul_16xVL(double *c, const double *a, const double *b,
          const unsigned int m_start, const unsigned int m_end,
          const unsigned int N, const unsigned int P,
          const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
  // Calculate the vl
  size_t gvl;
  asm volatile("vsetvli %[gvl], %[vl], e64, m1, ta, ma"
         : [gvl] "=r"(gvl)
         : [vl] "r"(p_end - p));

  const double *b_ = b + p;
  double *c_ = c + p;

  for (unsigned int m = m_start; m < m_end; m += 16) {
    const double *a_ = a + m * N;
    const double *a__ = a_;

    asm volatile("vle64.v v16, (%0);" ::"r"(b_));
    const double *b__ = b_ + P;

    double *c__ = c_ + m * P;

    double t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14,
      t15;

    t0 = *a__;
    a__ += N;
    t1 = *a__;
    a__ += N;
    t2 = *a__;
    a__ += N;
    t3 = *a__;
    a__ += N;
    t4 = *a__;
    a__ += N;
    t5 = *a__;
    a__ += N;
    t6 = *a__;
    a__ += N;
    t7 = *a__;
    a__ += N;
    t8 = *a__;
    a__ += N;
    t9 = *a__;
    a__ += N;
    t10 = *a__;
    a__ += N;
    t11 = *a__;
    a__ += N;
    t12 = *a__;
    a__ += N;
    t13 = *a__;
    a__ += N;
    t14 = *a__;
    a__ += N;
    t15 = *a__;

    unsigned int n = 0;

    while (n < N) {
    a__ = a_ + ++n;

    asm volatile("vle64.v v17, (%0);" ::"r"(b__));
    b__ += P;

    if (n == 1) {
      asm volatile("vfmul.vf v0, v16, %0" ::"f"(t0));
      t0 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v1, v16, %0" ::"f"(t1));
      t1 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v2, v16, %0" ::"f"(t2));
      t2 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v3, v16, %0" ::"f"(t3));
      t3 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v4, v16, %0" ::"f"(t4));
      t4 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v5, v16, %0" ::"f"(t5));
      t5 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v6, v16, %0" ::"f"(t6));
      t6 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v7, v16, %0" ::"f"(t7));
      t7 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v8, v16, %0" ::"f"(t8));
      t8 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v9, v16, %0" ::"f"(t9));
      t9 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v10, v16, %0" ::"f"(t10));
      t10 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v11, v16, %0" ::"f"(t11));
      t11 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v12, v16, %0" ::"f"(t12));
      t12 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v13, v16, %0" ::"f"(t13));
      t13 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v14, v16, %0" ::"f"(t14));
      t14 = *a__;
      a__ += N;
      asm volatile("vfmul.vf v15, v16, %0" ::"f"(t15));
      t15 = *a__;
    } else {
      asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
      t0 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v1, %0, v16" ::"f"(t1));
      t1 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v2, %0, v16" ::"f"(t2));
      t2 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v3, %0, v16" ::"f"(t3));
      t3 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t4));
      t4 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v5, %0, v16" ::"f"(t5));
      t5 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v6, %0, v16" ::"f"(t6));
      t6 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v7, %0, v16" ::"f"(t7));
      t7 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t8));
      t8 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v9, %0, v16" ::"f"(t9));
      t9 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v10, %0, v16" ::"f"(t10));
      t10 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v11, %0, v16" ::"f"(t11));
      t11 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v12, %0, v16" ::"f"(t12));
      t12 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v13, %0, v16" ::"f"(t13));
      t13 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v14, %0, v16" ::"f"(t14));
      t14 = *a__;
      a__ += N;
      asm volatile("vfmacc.vf v15, %0, v16" ::"f"(t15));
      t15 = *a__;
    }

    a__ = a_ + ++n;

    if (n == N)
      break;

    asm volatile("vle64.v v16, (%0);" ::"r"(b__));
    b__ += P;

    asm volatile("vfmacc.vf v0, %0, v17" ::"f"(t0));
    t0 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v1, %0, v17" ::"f"(t1));
    t1 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v2, %0, v17" ::"f"(t2));
    t2 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v3, %0, v17" ::"f"(t3));
    t3 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v4, %0, v17" ::"f"(t4));
    t4 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v5, %0, v17" ::"f"(t5));
    t5 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v6, %0, v17" ::"f"(t6));
    t6 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v7, %0, v17" ::"f"(t7));
    t7 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v8, %0, v17" ::"f"(t8));
    t8 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v9, %0, v17" ::"f"(t9));
    t9 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v10, %0, v17" ::"f"(t10));
    t10 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v11, %0, v17" ::"f"(t11));
    t11 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v12, %0, v17" ::"f"(t12));
    t12 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v13, %0, v17" ::"f"(t13));
    t13 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v14, %0, v17" ::"f"(t14));
    t14 = *a__;
    a__ += N;
    asm volatile("vfmacc.vf v15, %0, v17" ::"f"(t15));
    t15 = *a__;
    }

    asm volatile("vfmacc.vf v0, %0, v17" ::"f"(t0));
    asm volatile("vse64.v v0, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v1, %0, v17" ::"f"(t1));
    asm volatile("vse64.v v1, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v2, %0, v17" ::"f"(t2));
    asm volatile("vse64.v v2, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v3, %0, v17" ::"f"(t3));
    asm volatile("vse64.v v3, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v4, %0, v17" ::"f"(t4));
    asm volatile("vse64.v v4, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v5, %0, v17" ::"f"(t5));
    asm volatile("vse64.v v5, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v6, %0, v17" ::"f"(t6));
    asm volatile("vse64.v v6, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v7, %0, v17" ::"f"(t7));
    asm volatile("vse64.v v7, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v8, %0, v17" ::"f"(t8));
    asm volatile("vse64.v v8, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v9, %0, v17" ::"f"(t9));
    asm volatile("vse64.v v9, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v10, %0, v17" ::"f"(t10));
    asm volatile("vse64.v v10, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v11, %0, v17" ::"f"(t11));
    asm volatile("vse64.v v11, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v12, %0, v17" ::"f"(t12));
    asm volatile("vse64.v v12, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v13, %0, v17" ::"f"(t13));
    asm volatile("vse64.v v13, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v14, %0, v17" ::"f"(t14));
    asm volatile("vse64.v v14, (%0);" ::"r"(c__));
    c__ += P;
    asm volatile("vfmacc.vf v15, %0, v17" ::"f"(t15));
    asm volatile("vse64.v v15, (%0);" ::"r"(c__));
  }

  p += gvl;
  }
}