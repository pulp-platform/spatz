// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include "sp-fmatmul-post-increment.h"
#include <stddef.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// Inline-asm helpers using compiler-supported custom mnemonics
// -----------------------------------------------------------------------------

// Vector post-increment loads (e32)
#define P_VLE32_RRPOST_TO_V16(ptr_, inc_bytes_)                                            \
  asm volatile("p.vle32.v.rrpost v16, (%0), %1"                                             \
               : "+r"(ptr_)                                                                 \
               : "r"(inc_bytes_)                                                            \
               : "memory")

#define P_VLE32_RRPOST_TO_V18(ptr_, inc_bytes_)                                            \
  asm volatile("p.vle32.v.rrpost v18, (%0), %1"                                             \
               : "+r"(ptr_)                                                                 \
               : "r"(inc_bytes_)                                                            \
               : "memory")

#define P_VLE32_RRPOST_TO_V20(ptr_, inc_bytes_)                                            \
  asm volatile("p.vle32.v.rrpost v20, (%0), %1"                                             \
               : "+r"(ptr_)                                                                 \
               : "r"(inc_bytes_)                                                            \
               : "memory")

#define P_VLE32_RRPOST_TO_V24(ptr_, inc_bytes_)                                            \
  asm volatile("p.vle32.v.rrpost v24, (%0), %1"                                             \
               : "+r"(ptr_)                                                                 \
               : "r"(inc_bytes_)                                                            \
               : "memory")

// Scalar FP post-increment load (single precision)
// Writes directly to a C float output (compiler picks an FP reg for dst)
#define P_FLW_RRPOST_TO_SCALAR(dst_, ptr_, inc_bytes_)                                     \
  asm volatile("pflw.rrpost %0, (%1), %2"                                                  \
               : "=f"(dst_), "+r"(ptr_)                                                    \
               : "r"(inc_bytes_)                                                           \
               : "memory")

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

      unsigned int n = 0;

      while (n < N) {
        asm volatile("p.vle32.v.rrpost v24, (%0), %1" : "+r"(b__) : "r"(b_stride_bytes) : "memory");
        n+=1;
        if (n == 1) {
          asm volatile("vfmul.vf v0, v16, %0" : : "f"(t0));
          asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes) : "memory");

          asm volatile("vfmul.vf v8, v16, %0" : : "f"(t1));
          asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");
        } else {
          asm volatile("vfmacc.vf v0, %0, v16" : : "f"(t0));
          asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes) : "memory");

          asm volatile("vfmacc.vf v8, %0, v16" : : "f"(t1));
          asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");
        }
        n+=1;
        if (n == N)
          break;

        asm volatile("p.vle32.v.rrpost v16, (%0), %1" : "+r"(b__) : "r"(b_stride_bytes) : "memory");

        asm volatile("vfmacc.vf v0, %0, v24" : : "f"(t0));
        asm volatile("pflw.rrpost %0, (%1), %2" : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes) : "memory");

        asm volatile("vfmacc.vf v8, %0, v24" : : "f"(t1));
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
    const uintptr_t b_stride_bytes = (uintptr_t)P * sizeof(float);
    const uintptr_t a_stride_bytes = (uintptr_t)N * sizeof(float);

    for (unsigned int m = m_start; m < m_end; m += 4) {
      const float *a_ = a + m * N;
      const float *a__ = a_;

      const float *b__ = b_;
      P_VLE32_RRPOST_TO_V16(b__, b_stride_bytes);  // v16 <- *b__, b__ += P

      float *c__ = c_ + m * P;

      float t0, t1, t2, t3;

      P_FLW_RRPOST_TO_SCALAR(t0, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t1, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t2, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t3, a__, a_stride_bytes);

      unsigned int n = 0;

      while (n < N) {
        P_VLE32_RRPOST_TO_V20(b__, b_stride_bytes);  // v20 <- *b__, b__ += P

        a__ = a_ + ++n;

        if (n == 1) {
          asm volatile("vfmul.vf v0, v16, %0" : : "f"(t0));
          P_FLW_RRPOST_TO_SCALAR(t0, a__, a_stride_bytes);

          asm volatile("vfmul.vf v4, v16, %0" : : "f"(t1));
          P_FLW_RRPOST_TO_SCALAR(t1, a__, a_stride_bytes);

          asm volatile("vfmul.vf v8, v16, %0" : : "f"(t2));
          P_FLW_RRPOST_TO_SCALAR(t2, a__, a_stride_bytes);

          asm volatile("vfmul.vf v12, v16, %0" : : "f"(t3));
          P_FLW_RRPOST_TO_SCALAR(t3, a__, a_stride_bytes);
        } else {
          asm volatile("vfmacc.vf v0, %0, v16" : : "f"(t0));
          P_FLW_RRPOST_TO_SCALAR(t0, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v4, %0, v16" : : "f"(t1));
          P_FLW_RRPOST_TO_SCALAR(t1, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v8, %0, v16" : : "f"(t2));
          P_FLW_RRPOST_TO_SCALAR(t2, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v12, %0, v16" : : "f"(t3));
          P_FLW_RRPOST_TO_SCALAR(t3, a__, a_stride_bytes);
        }

        a__ = a_ + ++n;
        if (n == N)
          break;

        P_VLE32_RRPOST_TO_V16(b__, b_stride_bytes);  // v16 <- *b__, b__ += P

        asm volatile("vfmacc.vf v0, %0, v20" : : "f"(t0));
        P_FLW_RRPOST_TO_SCALAR(t0, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v4, %0, v20" : : "f"(t1));
        P_FLW_RRPOST_TO_SCALAR(t1, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v8, %0, v20" : : "f"(t2));
        P_FLW_RRPOST_TO_SCALAR(t2, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v12, %0, v20" : : "f"(t3));
        P_FLW_RRPOST_TO_SCALAR(t3, a__, a_stride_bytes);
      }

      asm volatile("vfmacc.vf v0, %0, v20" : : "f"(t0));
      asm volatile("vse32.v v0, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v4, %0, v20" : : "f"(t1));
      asm volatile("vse32.v v4, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v8, %0, v20" : : "f"(t2));
      asm volatile("vse32.v v8, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v12, %0, v20" : : "f"(t3));
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
    const uintptr_t b_stride_bytes = (uintptr_t)P * sizeof(float);
    const uintptr_t a_stride_bytes = (uintptr_t)N * sizeof(float);

    for (unsigned int m = m_start; m < m_end; m += 8) {
      const float *a_ = a + m * N;
      const float *a__ = a_;

      const float *b__ = b_;
      P_VLE32_RRPOST_TO_V18(b__, b_stride_bytes);  // v18 <- *b__, b__ += P

      float *c__ = c_ + m * P;

      float t0, t1, t2, t3, t4, t5, t6, t7;

      P_FLW_RRPOST_TO_SCALAR(t0, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t1, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t2, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t3, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t4, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t5, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t6, a__, a_stride_bytes);
      P_FLW_RRPOST_TO_SCALAR(t7, a__, a_stride_bytes);

      unsigned int n = 0;

      while (n < N) {
        a__ = a_ + ++n;

        P_VLE32_RRPOST_TO_V20(b__, b_stride_bytes);  // v20 <- *b__, b__ += P

        if (n == 1) {
          asm volatile("vfmul.vf v0, v18, %0" : : "f"(t0));
          P_FLW_RRPOST_TO_SCALAR(t0, a__, a_stride_bytes);

          asm volatile("vfmul.vf v2, v18, %0" : : "f"(t1));
          P_FLW_RRPOST_TO_SCALAR(t1, a__, a_stride_bytes);

          asm volatile("vfmul.vf v4, v18, %0" : : "f"(t2));
          P_FLW_RRPOST_TO_SCALAR(t2, a__, a_stride_bytes);

          asm volatile("vfmul.vf v6, v18, %0" : : "f"(t3));
          P_FLW_RRPOST_TO_SCALAR(t3, a__, a_stride_bytes);

          asm volatile("vfmul.vf v8, v18, %0" : : "f"(t4));
          P_FLW_RRPOST_TO_SCALAR(t4, a__, a_stride_bytes);

          asm volatile("vfmul.vf v10, v18, %0" : : "f"(t5));
          P_FLW_RRPOST_TO_SCALAR(t5, a__, a_stride_bytes);

          asm volatile("vfmul.vf v12, v18, %0" : : "f"(t6));
          P_FLW_RRPOST_TO_SCALAR(t6, a__, a_stride_bytes);

          asm volatile("vfmul.vf v14, v18, %0" : : "f"(t7));
          P_FLW_RRPOST_TO_SCALAR(t7, a__, a_stride_bytes);
        } else {
          asm volatile("vfmacc.vf v0, %0, v18" : : "f"(t0));
          P_FLW_RRPOST_TO_SCALAR(t0, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v2, %0, v18" : : "f"(t1));
          P_FLW_RRPOST_TO_SCALAR(t1, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v4, %0, v18" : : "f"(t2));
          P_FLW_RRPOST_TO_SCALAR(t2, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v6, %0, v18" : : "f"(t3));
          P_FLW_RRPOST_TO_SCALAR(t3, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v8, %0, v18" : : "f"(t4));
          P_FLW_RRPOST_TO_SCALAR(t4, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v10, %0, v18" : : "f"(t5));
          P_FLW_RRPOST_TO_SCALAR(t5, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v12, %0, v18" : : "f"(t6));
          P_FLW_RRPOST_TO_SCALAR(t6, a__, a_stride_bytes);

          asm volatile("vfmacc.vf v14, %0, v18" : : "f"(t7));
          P_FLW_RRPOST_TO_SCALAR(t7, a__, a_stride_bytes);
        }

        a__ = a_ + ++n;
        if (n == N)
          break;

        P_VLE32_RRPOST_TO_V18(b__, b_stride_bytes);  // v18 <- *b__, b__ += P

        asm volatile("vfmacc.vf v0, %0, v20" : : "f"(t0));
        P_FLW_RRPOST_TO_SCALAR(t0, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v2, %0, v20" : : "f"(t1));
        P_FLW_RRPOST_TO_SCALAR(t1, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v4, %0, v20" : : "f"(t2));
        P_FLW_RRPOST_TO_SCALAR(t2, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v6, %0, v20" : : "f"(t3));
        P_FLW_RRPOST_TO_SCALAR(t3, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v8, %0, v20" : : "f"(t4));
        P_FLW_RRPOST_TO_SCALAR(t4, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v10, %0, v20" : : "f"(t5));
        P_FLW_RRPOST_TO_SCALAR(t5, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v12, %0, v20" : : "f"(t6));
        P_FLW_RRPOST_TO_SCALAR(t6, a__, a_stride_bytes);

        asm volatile("vfmacc.vf v14, %0, v20" : : "f"(t7));
        P_FLW_RRPOST_TO_SCALAR(t7, a__, a_stride_bytes);
      }

      asm volatile("vfmacc.vf v0, %0, v20" : : "f"(t0));
      asm volatile("vse32.v v0, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v2, %0, v20" : : "f"(t1));
      asm volatile("vse32.v v2, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v4, %0, v20" : : "f"(t2));
      asm volatile("vse32.v v4, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v6, %0, v20" : : "f"(t3));
      asm volatile("vse32.v v6, (%0)" : : "r"(c__));
      c__ += P;

      asm volatile("vfmacc.vf v8, %0, v20" : : "f"(t4));
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