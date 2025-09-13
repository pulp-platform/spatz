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

#include "hp-fmatmul.h"
#include <stddef.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void matmul(__fp16 *c, const __fp16 *a, const __fp16 *b, const unsigned int M,
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

void matmul_2xVL(__fp16 *c, const __fp16 *a, const __fp16 *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e32, m8, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const __fp16 *b_ = b + p;
    __fp16 *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 2) {
      const __fp16 *a_ = a + m * N;
      const __fp16 *a__ = a_;

      asm volatile("vle32.v v16, (%0);" ::"r"(b_));
      const __fp16 *b__ = b_ + P;

      __fp16 *c__ = c_ + m * P;

      float t0, t1;

      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));

      unsigned int n = 0;

      while (n < N) {
        a__ = a_ + ++n;

        asm volatile("vle32.v v24, (%0);" ::"r"(b__));
        b__ += P;

        if (n == 1) {
          asm volatile("vfmul.vf v0, v16, %0" ::"f"(t0));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v8, v16, %0" ::"f"(t1));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        } else {
          asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t1));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        }

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle32.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v8, %0, v24" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
      }

      asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t0));
      asm volatile("vse32.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v8, %0, v24" ::"f"(t1));
      asm volatile("vse32.v v8, (%0);" ::"r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 4xVL
// ---------------

void matmul_4xVL(__fp16 *c, const __fp16 *a, const __fp16 *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e32, m4, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const __fp16 *b_ = b + p;
    __fp16 *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 4) {
      const __fp16 *a_ = a + m * N;
      const __fp16 *a__ = a_;

      asm volatile("vle32.v v16, (%0);" ::"r"(b_));
      const __fp16 *b__ = b_ + P;

      __fp16 *c__ = c_ + m * P;

      float t0, t1, t2, t3;

      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));

      unsigned int n = 0;

      while (n < N) {
        asm volatile("vle32.v v20, (%0);" ::"r"(b__));
        b__ += P;

        a__ = a_ + ++n;

        if (n == 1) {
          asm volatile("vfmul.vf v0, v16, %0" ::"f"(t0));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v4, v16, %0" ::"f"(t1));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v8, v16, %0" ::"f"(t2));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v12, v16, %0" ::"f"(t3));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
        } else {
          asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t1));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t2));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v12, %0, v16" ::"f"(t3));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
        }

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle32.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t2));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t3));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
      }

      asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
      asm volatile("vse32.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t1));
      asm volatile("vse32.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t2));
      asm volatile("vse32.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t3));
      asm volatile("vse32.v v12, (%0);" ::"r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 8xVL
// ---------------

void matmul_8xVL(__fp16 *c, const __fp16 *a, const __fp16 *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    // Calculate the vl
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e16, m2, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const __fp16 *b_ = b + p;
    __fp16 *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 8) {
      const __fp16 *a_ = a + m * N;
      const __fp16 *a__ = a_;

      asm volatile("vle16.v v18, (%0);" ::"r"(b_));
      const __fp16 *b__ = b_ + P;

      __fp16 *c__ = c_ + m * P;

      float t0, t1, t2, t3, t4, t5, t6, t7;

      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
      a__ += N;
      asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));

      unsigned int n = 0;

      while (n < N) {
        a__ = a_ + ++n;

        asm volatile("vle16.v v20, (%0);" ::"r"(b__));
        b__ += P;

        if (n == 1) {
          asm volatile("vfmul.vf v0, v18, %0" ::"f"(t0));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v2, v18, %0" ::"f"(t1));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v4, v18, %0" ::"f"(t2));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v6, v18, %0" ::"f"(t3));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v8, v18, %0" ::"f"(t4));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v10, v18, %0" ::"f"(t5));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v12, v18, %0" ::"f"(t6));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmul.vf v14, v18, %0" ::"f"(t7));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
        } else {
          asm volatile("vfmacc.vf v0, %0, v18" ::"f"(t0));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v2, %0, v18" ::"f"(t1));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v4, %0, v18" ::"f"(t2));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t3));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v8, %0, v18" ::"f"(t4));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v10, %0, v18" ::"f"(t5));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v12, %0, v18" ::"f"(t6));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
          a__ += N;
          asm volatile("vfmacc.vf v14, %0, v18" ::"f"(t7));
          asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
        }

        a__ = a_ + ++n;

        if (n == N)
          break;

        asm volatile("vle16.v v18, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t0) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t1) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t2) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t3) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t4) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t5) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t6) : [a] "r"(a__));
        a__ += N;
        asm volatile("vfmacc.vf v14, %0, v20" ::"f"(t7));
        asm volatile("flh %[t], 0(%[a])" : [t] "=f"(t7) : [a] "r"(a__));
      }

      asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));
      asm volatile("vse16.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v2, %0, v20" ::"f"(t1));
      asm volatile("vse16.v v2, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v4, %0, v20" ::"f"(t2));
      asm volatile("vse16.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v6, %0, v20" ::"f"(t3));
      asm volatile("vse16.v v6, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v8, %0, v20" ::"f"(t4));
      asm volatile("vse16.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v10, %0, v20" ::"f"(t5));
      asm volatile("vse16.v v10, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v12, %0, v20" ::"f"(t6));
      asm volatile("vse16.v v12, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfmacc.vf v14, %0, v20" ::"f"(t7));
      asm volatile("vse16.v v14, (%0);" ::"r"(c__));
    }

    p += gvl;
  }
}
