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

#include "sdotp-fmatmul.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void matmul(__fp16 *c, const __fp16 *a, const __fp16 *b, const unsigned int M,
            const unsigned int N, const unsigned int P) {
  if (M <= 4) {
    matmul_2xVL(c, a, b, 0, M, N, P, 0, P, P);
  } else if (M <= 8) {
    matmul_4xVL(c, a, b, 0, M, N, P, 0, P, P);
  } else {
    matmul_8xVL(c, a, b, 0, M, N, P, 0, P, P);
  }
}

// ---------------
// 2xVL
// ---------------

void matmul_2xVL(__fp16 *c, const __fp16 *a, const __fp16 *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end,
                 const unsigned int vl) {

  for (unsigned int p = p_start; p < p_end; p += vl) {
    const __fp16 *b_ = b + p;
    __fp16 *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 2) {
      asm volatile("vsetvli zero, %0, e16, m8, ta, ma" ::"r"(2*vl));

      const __fp16 *a_ = a + m * N;
      const __fp16 *a__ = a_;

      asm volatile("vle32.v v16, (%0);" ::"r"(b_));
      const __fp16 *b__ = b_ + P;

      __fp16 *c__ = c_ + m * P;

      double t0, t1;

      asm volatile("vmv.v.x v0, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v8, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));

      unsigned int n = 0;

      while (n < N) {
        n += 2;
        a__ = a_ + n;

        asm volatile("vle16.v v24, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfwnmacc.vf v0, %0, v16" ::"f"(t0));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v8, %0, v16" ::"f"(t1));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));

        n += 2;
        a__ = a_ + n;

        if (n == N)
          break;

        asm volatile("vle16.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfwnmacc.vf v0, %0, v24" ::"f"(t0));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v8, %0, v24" ::"f"(t1));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));
      }

      asm volatile("vfwnmacc.vf v0, %0, v24" ::"f"(t0));
      asm volatile("vfwnmacc.vf v8, %0, v24" ::"f"(t1));

      asm volatile("vsetvli zero, %0, e16, m8, ta, ma" ::"r"(vl));

      asm volatile("vfncvt.f.f.w v0, v0");
      asm volatile("vse16.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v8, v8");
      asm volatile("vse16.v v8, (%0);" ::"r"(c__));
    }
  }
}

// ---------------
// 4xVL
// ---------------

void matmul_4xVL(__fp16 *c, const __fp16 *a, const __fp16 *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end,
                 const unsigned int vl) {

  for (unsigned int p = p_start; p < p_end; p += vl) {
    const __fp16 *b_ = b + p;
    __fp16 *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 4) {
      asm volatile("vsetvli zero, %0, e16, m4, ta, ma" ::"r"(2*vl));

      const __fp16 *a_ = a + m * N;
      const __fp16 *a__ = a_;

      asm volatile("vle16.v v16, (%0);" ::"r"(b_));
      const __fp16 *b__ = b_ + P;

      __fp16 *c__ = c_ + m * P;

      double t0, t1, t2, t3;

      asm volatile("vmv.v.x v0, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v4, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v8, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t2) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v12, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t3) : [a] "r" (a__));

      unsigned int n = 0;

      while (n < N) {
        n += 2;
        a__ = a_ + n;

        asm volatile("vle16.v v20, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfwnmacc.vf v0, %0, v16" ::"f"(t0));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v4, %0, v16" ::"f"(t1));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v8, %0, v16" ::"f"(t2));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t2) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v12, %0, v16" ::"f"(t3));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t3) : [a] "r" (a__));

        n += 2;
        a__ = a_ + n;

        if (n == N)
          break;

        asm volatile("vle16.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfwnmacc.vf v0, %0, v20" ::"f"(t0));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v4, %0, v20" ::"f"(t1));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v8, %0, v20" ::"f"(t2));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t2) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v12, %0, v20" ::"f"(t3));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t3) : [a] "r" (a__));
      }

      asm volatile("vfwnmacc.vf v0, %0, v20" ::"f"(t0));
      asm volatile("vfwnmacc.vf v4, %0, v20" ::"f"(t1));
      asm volatile("vfwnmacc.vf v8, %0, v20" ::"f"(t2));
      asm volatile("vfwnmacc.vf v12, %0, v20" ::"f"(t3));

      asm volatile("vsetvli zero, %0, e16, m4, ta, ma" ::"r"(vl));

      asm volatile("vfncvt.f.f.w v0, v0");
      asm volatile("vse16.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v4, v4");
      asm volatile("vse16.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v8, v8");
      asm volatile("vse16.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v12, v12");
      asm volatile("vse16.v v12, (%0);" ::"r"(c__));
    }
  }
}

// ---------------
// 8xVL
// ---------------

void matmul_8xVL(__fp16 *c, const __fp16 *a, const __fp16 *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end,
                 const unsigned int vl) {

  for (unsigned int p = p_start; p < p_end; p += vl) {
    const __fp16 *b_ = b + p;
    __fp16 *c_ = c + p;

    for (unsigned int m = m_start; m < m_end; m += 8) {
      asm volatile("vsetvli zero, %0, e16, m2, ta, ma" ::"r"(2*vl));

      const __fp16 *a_ = a + m * N;
      const __fp16 *a__ = a_;

      asm volatile("vle16.v v18, (%0);" ::"r"(b_));
      const __fp16 *b__ = b_ + P;

      __fp16 *c__ = c_ + m * P;

      double t0, t1, t2, t3, t4, t5, t6, t7;

      asm volatile("vmv.v.x v0, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v2, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v4, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t2) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v6, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t3) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v8, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t4) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v10, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t5) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v12, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t6) : [a] "r" (a__));
      a__ += N;
      asm volatile("vmv.v.x v14, zero");
      asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t7) : [a] "r" (a__));

      unsigned int n = 0;

      while (n < N) {
        n += 2;
        a__ = a_ + n;

        asm volatile("vle16.v v20, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfwnmacc.vf v0, %0, v18" ::"f"(t0));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v2, %0, v18" ::"f"(t1));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v4, %0, v18" ::"f"(t2));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t2) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v6, %0, v18" ::"f"(t3));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t3) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v8, %0, v18" ::"f"(t4));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t4) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v10, %0, v18" ::"f"(t5));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t5) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v12, %0, v18" ::"f"(t6));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t6) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v14, %0, v18" ::"f"(t7));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t7) : [a] "r" (a__));

        n += 2;
        a__ = a_ + n;

        if (n == N)
          break;

        asm volatile("vle16.v v18, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vfwnmacc.vf v0, %0, v20" ::"f"(t0));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t0) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v2, %0, v20" ::"f"(t1));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t1) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v4, %0, v20" ::"f"(t2));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t2) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v6, %0, v20" ::"f"(t3));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t3) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v8, %0, v20" ::"f"(t4));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t4) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v10, %0, v20" ::"f"(t5));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t5) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v12, %0, v20" ::"f"(t6));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t6) : [a] "r" (a__));
        a__ += N;
        asm volatile("vfwnmacc.vf v14, %0, v20" ::"f"(t7));
        asm volatile("flw %[t], 0(%[a])" : [t] "=f" (t7) : [a] "r" (a__));
      }

      asm volatile("vfwnmacc.vf v0, %0, v20" ::"f"(t0));
      asm volatile("vfwnmacc.vf v2, %0, v20" ::"f"(t1));
      asm volatile("vfwnmacc.vf v4, %0, v20" ::"f"(t2));
      asm volatile("vfwnmacc.vf v6, %0, v20" ::"f"(t3));
      asm volatile("vfwnmacc.vf v8, %0, v20" ::"f"(t4));
      asm volatile("vfwnmacc.vf v10, %0, v20" ::"f"(t5));
      asm volatile("vfwnmacc.vf v12, %0, v20" ::"f"(t6));
      asm volatile("vfwnmacc.vf v14, %0, v20" ::"f"(t7));

      asm volatile("vsetvli zero, %0, e16, m2, ta, ma" ::"r"(vl));

      asm volatile("vfncvt.f.f.w v0, v0");
      asm volatile("vse16.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v2, v2");
      asm volatile("vse16.v v2, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v4, v4");
      asm volatile("vse16.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v6, v6");
      asm volatile("vse16.v v6, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v8, v8");
      asm volatile("vse16.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v10, v10");
      asm volatile("vse16.v v10, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v12, v12");
      asm volatile("vse16.v v12, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vfncvt.f.f.w v14, v14");
      asm volatile("vse16.v v14, (%0);" ::"r"(c__));
    }
  }
}
