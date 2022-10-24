// Copyright 2021 ETH Zurich and University of Bologna.
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

// Author: Domenic Wüthrich, ETH Zurich

#include "dp-fmatmul.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void matmul(double *c, const double *a, const double *b, const unsigned int M,
            const unsigned int N, const unsigned int P) {
  if (M <= 4) {
    matmul_2xVL(c, a, b, 0, M, N, P, 0, P, P);
  } else if (M <= 8) {
    matmul_4xVL(c, a, b, 0, M, N, P, 0, P, P);
  } else {
    matmul_8xVL(c, a, b, 0, M, N, P, 0, P, P);
  }
}

void matmul_single_unrolled(double *c, const double *a, const double *b,
                            const unsigned int N, const unsigned int P,
                            unsigned int vl) {
  // Zero constant
  const double zero = 0.0;

  // Set VL
  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(vl));

  // Temporary variables
  double t0, t1, t2, t3, t4, t5, t6, t7;
  double *a_ = (double *)a;
  double *b_ = (double *)b;
  double *c_ = (double *)c;

  double *a__ = a_;

  // Compute the multiplication
  unsigned int n = 0;

  t0 = *a__, a__ += N;
  t1 = *a__, a__ += N;
  t2 = *a__, a__ += N;
  t3 = *a__, a__ += N;
  t4 = *a__, a__ += N;
  t5 = *a__, a__ += N;
  t6 = *a__, a__ += N;
  t7 = *a__;

  // Calculate pointer to the matrix A
  a__ = a_ + ++n;

  asm volatile("vle64.v v16, (%0);" ::"r"(b_));
  b_ += P;
  asm volatile("vfmv.v.f v0, %0" ::"f"(zero));
  asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
  t0 = *a__, a__ += N;
  asm volatile("vfmv.v.f v2, %0" ::"f"(zero));
  asm volatile("vfmacc.vf v2, %0, v16" ::"f"(t1));
  t1 = *a__, a__ += N;
  asm volatile("vfmv.v.f v4, %0" ::"f"(zero));
  asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t2));
  t2 = *a__, a__ += N;
  asm volatile("vfmv.v.f v6, %0" ::"f"(zero));
  asm volatile("vfmacc.vf v6, %0, v16" ::"f"(t3));
  t3 = *a__, a__ += N;

  // Load one row of B
  asm volatile("vle64.v v18, (%0);" ::"r"(b_));
  b_ += P;

  asm volatile("vfmv.v.f v8, %0" ::"f"(zero));
  asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t4));
  t4 = *a__, a__ += N;
  asm volatile("vfmv.v.f v10, %0" ::"f"(zero));
  asm volatile("vfmacc.vf v10, %0, v16" ::"f"(t5));
  t5 = *a__, a__ += N;
  asm volatile("vfmv.v.f v12, %0" ::"f"(zero));
  asm volatile("vfmacc.vf v12, %0, v16" ::"f"(t6));
  t6 = *a__, a__ += N;
  asm volatile("vfmv.v.f v14, %0" ::"f"(zero));
  asm volatile("vfmacc.vf v14, %0, v16" ::"f"(t7));
  t7 = *a__;

  // Calculate pointer to the matrix A
  a__ = a_ + ++n;

  while (n < N) {
    // Load one row of B
    asm volatile("vle64.v v16, (%0);" ::"r"(b_));
    b_ += P;

    asm volatile("vfmacc.vf v0, %0, v18" ::"f"(t0));
    t0 = *a__, a__ += N;
    asm volatile("vfmacc.vf v2, %0, v18" ::"f"(t1));
    t1 = *a__, a__ += N;
    asm volatile("vfmacc.vf v4, %0, v18" ::"f"(t2));
    t2 = *a__, a__ += N;
    asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t3));
    t3 = *a__, a__ += N;
    asm volatile("vfmacc.vf v8, %0, v18" ::"f"(t4));
    t4 = *a__, a__ += N;
    asm volatile("vfmacc.vf v10, %0, v18" ::"f"(t5));
    t5 = *a__, a__ += N;
    asm volatile("vfmacc.vf v12, %0, v18" ::"f"(t6));
    t6 = *a__, a__ += N;
    asm volatile("vfmacc.vf v14, %0, v18" ::"f"(t7));
    t7 = *a__;

    // Calculate pointer to the matrix A
    a__ = a_ + ++n;

    // Load one row of B
    asm volatile("vle64.v v18, (%0);" ::"r"(b_));
    b_ += P;

    asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
    t0 = *a__, a__ += N;
    asm volatile("vfmacc.vf v2, %0, v16" ::"f"(t1));
    t1 = *a__, a__ += N;
    asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t2));
    t2 = *a__, a__ += N;
    asm volatile("vfmacc.vf v6, %0, v16" ::"f"(t3));
    t3 = *a__, a__ += N;
    asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t4));
    t4 = *a__, a__ += N;
    asm volatile("vfmacc.vf v10, %0, v16" ::"f"(t5));
    t5 = *a__, a__ += N;
    asm volatile("vfmacc.vf v12, %0, v16" ::"f"(t6));
    t6 = *a__, a__ += N;
    asm volatile("vfmacc.vf v14, %0, v16" ::"f"(t7));
    t7 = *a__;

    // Calculate pointer to the matrix A
    a__ = a_ + ++n;
  }

  asm volatile("vfmacc.vf v0, %0, v18" ::"f"(t0));
  asm volatile("vse64.v v0, (%0);" ::"r"(c_));
  c_ += P;
  asm volatile("vfmacc.vf v2, %0, v18" ::"f"(t1));
  asm volatile("vse64.v v2, (%0);" ::"r"(c_));
  c_ += P;
  asm volatile("vfmacc.vf v4, %0, v18" ::"f"(t2));
  asm volatile("vse64.v v4, (%0);" ::"r"(c_));
  c_ += P;
  asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t3));
  asm volatile("vse64.v v6, (%0);" ::"r"(c_));
  c_ += P;
  asm volatile("vfmacc.vf v8, %0, v18" ::"f"(t4));
  asm volatile("vse64.v v8, (%0);" ::"r"(c_));
  c_ += P;
  asm volatile("vfmacc.vf v10, %0, v18" ::"f"(t5));
  asm volatile("vse64.v v10, (%0);" ::"r"(c_));
  c_ += P;
  asm volatile("vfmacc.vf v12, %0, v18" ::"f"(t6));
  asm volatile("vse64.v v12, (%0);" ::"r"(c_));
  c_ += P;
  asm volatile("vfmacc.vf v14, %0, v18" ::"f"(t7));
  asm volatile("vse64.v v14, (%0);" ::"r"(c_));
  c_ += P;
}

// ---------------
// 2xVL
// ---------------

void matmul_2xVL(double *c, const double *a, const double *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end,
                 const unsigned int vl) {

  // Zero constant
  const double zero = 0.0;

  asm volatile("vsetvli zero, %0, e64, m8, ta, ma" ::"r"(vl));

  for (unsigned int p = p_start; p < p_end; p += vl) {
    const double *b_ = b + p;
    double *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 2) {
      const double *a_ = a + m * N;
      const double *a__ = a_;

      asm volatile("vle64.v v16, (%0);" ::"r"(b_));
      const double *b__ = b_ + P;

      double *c__ = c_ + m * P;

      double t0, t1;

      asm volatile("vfmv.v.f v0, %0" ::"f"(zero));
      t0 = *a__, a__ += N;
      asm volatile("vfmv.v.f v8, %0" ::"f"(zero));
      t1 = *a__;

      unsigned int n = 0;

      while (n < N) {
        a__ = a_ + ++n;

        asm volatile("vle64.v v24, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t1));
        t1 = *a__;

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle64.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vfmacc.vf v8, %0, v24" ::"f"(t1));
        t1 = *a__;
      }

      asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t0));
      asm volatile("vse64.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v8, %0, v24" ::"f"(t1));
      asm volatile("vse64.v v8, (%0);" ::"r"(c__));
    }
  }
}

// ---------------
// 4xVL
// ---------------

void matmul_4xVL(double *c, const double *a, const double *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end,
                 const unsigned int vl) {

  // Zero constant
  const double zero = 0.0;

  asm volatile("vsetvli zero, %0, e64, m4, ta, ma" ::"r"(vl));

  for (unsigned int p = p_start; p < p_end; p += vl) {
    const double *b_ = b + p;
    double *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 4) {
      const double *a_ = a + m * N;
      const double *a__ = a_;

      asm volatile("vle64.v v16, (%0);" ::"r"(b_));
      const double *b__ = b_ + P;

      double *c__ = c_ + m * P;

      double t0, t1, t2, t3;

      asm volatile("vfmv.v.f v0, %0" ::"f"(zero));
      t0 = *a__, a__ += N;
      asm volatile("vfmv.v.f v4, %0" ::"f"(zero));
      t1 = *a__, a__ += N;
      asm volatile("vfmv.v.f v8, %0" ::"f"(zero));
      t2 = *a__, a__ += N;
      asm volatile("vfmv.v.f v12, %0" ::"f"(zero));
      t3 = *a__;

      unsigned int n = 0;

      while (n < N) {
        a__ = a_ + ++n;

        asm volatile("vle64.v v20, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t1));
        t1 = *a__, a__ += N;
        asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t2));
        t2 = *a__, a__ += N;
        asm volatile("vfmacc.vf v12, %0, v16" ::"f"(t3));
        t3 = *a__;

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle64.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t1));
        t1 = *a__, a__ += N;
        asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t2));
        t2 = *a__, a__ += N;
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
  }
}

// ---------------
// 8xVL
// ---------------

void matmul_8xVL(double *c, const double *a, const double *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end,
                 const unsigned int vl) {

  // Zero constant
  const double zero = 0.0;

  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(vl));

  for (unsigned int p = p_start; p < p_end; p += vl) {
    const double *b_ = b + p;
    double *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 8) {
      const double *a_ = a + m * N;
      const double *a__ = a_;

      asm volatile("vle64.v v18, (%0);" ::"r"(b_));
      const double *b__ = b_ + P;

      double *c__ = c_ + m * P;

      double t0, t1, t2, t3, t4, t5, t6, t7;

      asm volatile("vfmv.v.f v0, %0" ::"f"(zero));
      t0 = *a__, a__ += N;
      asm volatile("vfmv.v.f v2, %0" ::"f"(zero));
      t1 = *a__, a__ += N;
      asm volatile("vfmv.v.f v4, %0" ::"f"(zero));
      t2 = *a__, a__ += N;
      asm volatile("vfmv.v.f v6, %0" ::"f"(zero));
      t3 = *a__, a__ += N;
      asm volatile("vfmv.v.f v8, %0" ::"f"(zero));
      t4 = *a__, a__ += N;
      asm volatile("vfmv.v.f v10, %0" ::"f"(zero));
      t5 = *a__, a__ += N;
      asm volatile("vfmv.v.f v12, %0" ::"f"(zero));
      t6 = *a__, a__ += N;
      asm volatile("vfmv.v.f v14, %0" ::"f"(zero));
      t7 = *a__;

      unsigned int n = 0;

      while (n < N) {
        a__ = a_ + ++n;

        asm volatile("vle64.v v20, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v18" ::"f"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vfmacc.vf v2, %0, v18" ::"f"(t1));
        t1 = *a__, a__ += N;
        asm volatile("vfmacc.vf v4, %0, v18" ::"f"(t2));
        t2 = *a__, a__ += N;
        asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t3));
        t3 = *a__, a__ += N;
        asm volatile("vfmacc.vf v8, %0, v18" ::"f"(t4));
        t4 = *a__, a__ += N;
        asm volatile("vfmacc.vf v10, %0, v18" ::"f"(t5));
        t5 = *a__, a__ += N;
        asm volatile("vfmacc.vf v12, %0, v18" ::"f"(t6));
        t6 = *a__, a__ += N;
        asm volatile("vfmacc.vf v14, %0, v18" ::"f"(t7));
        t7 = *a__;

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle64.v v18, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
        t1 = *a__, a__ += N;
        asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
        t2 = *a__, a__ += N;
        asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
        t3 = *a__, a__ += N;
        asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
        t4 = *a__, a__ += N;
        asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
        t5 = *a__, a__ += N;
        asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
        t6 = *a__, a__ += N;
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
  }
}
