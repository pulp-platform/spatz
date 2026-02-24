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

#include "sp-fmatmul-post-increment.h"
#include <stddef.h>
#include <stdint.h>

// ---------------
// 2xVL
// ---------------

void matmul_2xVL(float *c, const float *a, const float *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e32, m2, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const float *b_ = b + p;
    float *c_ = c + p;
    const intptr_t b_stride_bytes = (intptr_t)P * sizeof(float);
    const intptr_t a_stride_bytes = (intptr_t)N * sizeof(float);
    const intptr_t a_stride_bytes_1_n = (intptr_t)(1-N) * sizeof(float);

    for (unsigned int m = m_start; m < m_end; m += 2) {
      const float *a_ = a + m * N;
      const float *a__ = a_;
      const float *b__ = b_;

      asm volatile("p.vle32.v.rrpost v16, (%0), %1" : "+r"(b__) : "r"(b_stride_bytes) : "memory"); // v16 <- *b__, b__ += P

      float *c__ = c_ + m * P;

      float t0, t1;
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes) : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");

      //preload v24
      asm volatile("p.vle32.v.rrpost v24, (%0), %1" : "+r"(b__) : "r"(b_stride_bytes) : "memory");
      // compute with v16
      asm volatile("vfmul.vf v0, v16, %0" : : "f"(t0));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes) : "memory");

      asm volatile("vfmul.vf v8, v16, %0" : : "f"(t1));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");

      unsigned int n = 1;

      while (n < N-1) {
        // preload v16
        asm volatile("p.vle32.v.rrpost v16, (%0), %1" : "+r"(b__) : "r"(b_stride_bytes) : "memory");
        // compute with v24
        asm volatile("vfmacc.vf v0, %0, v24" : : "f"(t0));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes) : "memory");

        asm volatile("vfmacc.vf v8, %0, v24" : : "f"(t1));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");
        n+=2;
        // preload v24
        asm volatile("p.vle32.v.rrpost v24, (%0), %1" : "+r"(b__) : "r"(b_stride_bytes) : "memory");
        // compute with v16
        asm volatile("vfmacc.vf v0, %0, v16" : : "f"(t0));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes) : "memory");

        asm volatile("vfmacc.vf v8, %0, v16" : : "f"(t1));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");
      }

      asm volatile("vfmacc.vf v0, %0, v24" : : "f"(t0));
      asm volatile("vse32.v v0, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v8, %0, v24" : : "f"(t1));
      asm volatile("vse32.v v8, (%0)" : : "r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 4xVL
// ---------------

void matmul_4xVL(float *c, const float *a, const float *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e32, m2, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const float *b_ = b + p;
    float *c_ = c + p;

    const intptr_t b_stride_bytes = (intptr_t)P * sizeof(float);
    const intptr_t a_stride_bytes = (intptr_t)N * sizeof(float);
    const intptr_t a_stride_bytes_1_3n = (intptr_t)(1 - 3 * (intptr_t)N) * sizeof(float);

    for (unsigned int m = m_start; m < m_end; m += 4) {
      const float *a_ = a + m * N;
      const float *a__ = a_;
      const float *b__ = b_;

      // preload first B column-vector (k = 0)
      asm volatile("p.vle32.v.rrpost v16, (%0), %1"
                   : "+r"(b__)
                   : "r"(b_stride_bytes)
                   : "memory");

      float *c__ = c_ + m * P;

      float t0, t1, t2, t3;

      // load A(m+0..m+3, k=0), and position a__ to A(m+0, k=1)
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t2), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t3), "+r"(a__) : "r"(a_stride_bytes_1_3n): "memory");

      // preload second B column-vector (k = 1)
      asm volatile("p.vle32.v.rrpost v24, (%0), %1"
                   : "+r"(b__)
                   : "r"(b_stride_bytes)
                   : "memory");

      // initialize accumulators with v16 (k = 0)
      asm volatile("vfmul.vf v0,  v16, %0" : : "f"(t0));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v4,  v16, %0" : : "f"(t1));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v8,  v16, %0" : : "f"(t2));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t2), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v12, v16, %0" : : "f"(t3));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t3), "+r"(a__) : "r"(a_stride_bytes_1_3n): "memory");

      // We already handled k=0 and preloaded k=1 in v24.
      unsigned int n = 1;

      while (n < N - 1) {
        // preload next B vector into v16
        asm volatile("p.vle32.v.rrpost v16, (%0), %1"
                     : "+r"(b__)
                     : "r"(b_stride_bytes)
                     : "memory");

        // compute with v24
        asm volatile("vfmacc.vf v0,  %0, v24" : : "f"(t0));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v4,  %0, v24" : : "f"(t1));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v8,  %0, v24" : : "f"(t2));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t2), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v12, %0, v24" : : "f"(t3));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t3), "+r"(a__) : "r"(a_stride_bytes_1_3n): "memory");

        n += 2;

        // preload next B vector into v24
        asm volatile("p.vle32.v.rrpost v24, (%0), %1"
                     : "+r"(b__)
                     : "r"(b_stride_bytes)
                     : "memory");

        // compute with v16
        asm volatile("vfmacc.vf v0,  %0, v16" : : "f"(t0));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v4,  %0, v16" : : "f"(t1));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v8,  %0, v16" : : "f"(t2));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t2), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v12, %0, v16" : : "f"(t3));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t3), "+r"(a__) : "r"(a_stride_bytes_1_3n): "memory");
      }

      // final compute with v24
      asm volatile("vfmacc.vf v0,  %0, v24" : : "f"(t0));
      asm volatile("vse32.v v0, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v4,  %0, v24" : : "f"(t1));
      asm volatile("vse32.v v4, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v8,  %0, v24" : : "f"(t2));
      asm volatile("vse32.v v8, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v12, %0, v24" : : "f"(t3));
      asm volatile("vse32.v v12, (%0)" : : "r"(c__));
    }

    p += gvl;
  }
}

// ---------------
// 8xVL
// ---------------

void matmul_8xVL(float *c, const float *a, const float *b,
                 const unsigned int m_start, const unsigned int m_end,
                 const unsigned int N, const unsigned int P,
                 const unsigned int p_start, const unsigned int p_end) {

  unsigned int p = p_start;
  while (p < p_end) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e32, m2, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(p_end - p));

    const float *b_ = b + p;
    float *c_ = c + p;

    const intptr_t b_stride_bytes = (intptr_t)P * sizeof(float);
    const intptr_t a_stride_bytes = (intptr_t)N * sizeof(float);
    const intptr_t a_stride_bytes_1_7n = (intptr_t)(1 - 7 * (intptr_t)N) * sizeof(float);

    for (unsigned int m = m_start; m < m_end; m += 8) {
      const float *a_ = a + m * N;
      const float *a__ = a_;
      const float *b__ = b_;

      // preload first B column-vector (k = 0)
      asm volatile("p.vle32.v.rrpost v18, (%0), %1"
                   : "+r"(b__)
                   : "r"(b_stride_bytes)
                   : "memory");

      float *c__ = c_ + m * P;

      float t0, t1, t2, t3, t4, t5, t6, t7;

      // load A(m+0..m+7, k=0), and position a__ to A(m+0, k=1)
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t2), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t3), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t4), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t5), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t6), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t7), "+r"(a__) : "r"(a_stride_bytes_1_7n): "memory");

      // preload second B column-vector (k = 1)
      asm volatile("p.vle32.v.rrpost v20, (%0), %1"
                   : "+r"(b__)
                   : "r"(b_stride_bytes)
                   : "memory");

      // initialize accumulators with v18 (k = 0)
      asm volatile("vfmul.vf v0,  v18, %0" : : "f"(t0));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v2,  v18, %0" : : "f"(t1));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v4,  v18, %0" : : "f"(t2));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t2), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v6,  v18, %0" : : "f"(t3));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t3), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v8,  v18, %0" : : "f"(t4));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t4), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v10, v18, %0" : : "f"(t5));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t5), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v12, v18, %0" : : "f"(t6));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t6), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

      asm volatile("vfmul.vf v14, v18, %0" : : "f"(t7));
      asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t7), "+r"(a__) : "r"(a_stride_bytes_1_7n): "memory");

      // We already handled k=0 and preloaded k=1 in v20.
      unsigned int n = 1;

      while (n < N - 1) {
        // preload next B vector into v18
        asm volatile("p.vle32.v.rrpost v18, (%0), %1"
                     : "+r"(b__)
                     : "r"(b_stride_bytes)
                     : "memory");

        // compute with v20
        asm volatile("vfmacc.vf v0,  %0, v20" : : "f"(t0));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v2,  %0, v20" : : "f"(t1));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v4,  %0, v20" : : "f"(t2));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t2), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v6,  %0, v20" : : "f"(t3));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t3), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v8,  %0, v20" : : "f"(t4));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t4), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v10, %0, v20" : : "f"(t5));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t5), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v12, %0, v20" : : "f"(t6));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t6), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v14, %0, v20" : : "f"(t7));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t7), "+r"(a__) : "r"(a_stride_bytes_1_7n): "memory");

        n += 2;

        // preload next B vector into v20
        asm volatile("p.vle32.v.rrpost v20, (%0), %1"
                     : "+r"(b__)
                     : "r"(b_stride_bytes)
                     : "memory");

        // compute with v18
        asm volatile("vfmacc.vf v0,  %0, v18" : : "f"(t0));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v2,  %0, v18" : : "f"(t1));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v4,  %0, v18" : : "f"(t2));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t2), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v6,  %0, v18" : : "f"(t3));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t3), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v8,  %0, v18" : : "f"(t4));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t4), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v10, %0, v18" : : "f"(t5));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t5), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v12, %0, v18" : : "f"(t6));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t6), "+r"(a__) : "r"(a_stride_bytes)     : "memory");

        asm volatile("vfmacc.vf v14, %0, v18" : : "f"(t7));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t7), "+r"(a__) : "r"(a_stride_bytes_1_7n): "memory");
      }

      // final compute with v20
      asm volatile("vfmacc.vf v0,  %0, v20" : : "f"(t0));
      asm volatile("vse32.v v0, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v2,  %0, v20" : : "f"(t1));
      asm volatile("vse32.v v2, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v4,  %0, v20" : : "f"(t2));
      asm volatile("vse32.v v4, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v6,  %0, v20" : : "f"(t3));
      asm volatile("vse32.v v6, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v8,  %0, v20" : : "f"(t4));
      asm volatile("vse32.v v8, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v10, %0, v20" : : "f"(t5));
      asm volatile("vse32.v v10, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v12, %0, v20" : : "f"(t6));
      asm volatile("vse32.v v12, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v14, %0, v20" : : "f"(t7));
      asm volatile("vse32.v v14, (%0)" : : "r"(c__));
    }

    p += gvl;
  }
}