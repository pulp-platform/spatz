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

#ifndef MATMUL_H
#define MATMUL_H

#include <stdint.h>

void matmul(int32_t *c, const int32_t *a, const int32_t *b,
            const unsigned long int M, const unsigned long int N,
            const unsigned long int P);

void matmul_2x2(int32_t *c, const int32_t *a, const int32_t *b,
                const unsigned long int M, const unsigned long int N,
                const unsigned long int P);
void matmul_4x4(int32_t *c, const int32_t *a, const int32_t *b,
                const unsigned long int M, const unsigned long int N,
                const unsigned long int P);

void matmul_8x8(int32_t *c, const int32_t *a, const int32_t *b,
                const unsigned long int M, const unsigned long int N,
                const unsigned long int P);

#define MATMUL_8XVLEN_PARA(c, a, b, m_start, m_end, N, p_start, p_end, P, vlen) do { \
\
  asm volatile("vsetvli zero, %0, e32, m2, ta, ma" :: "r"(vlen)); \
\
  for (unsigned long int p = p_start; p < p_end; p += vlen) { \
    int32_t *b_ = b + p; \
    int32_t *c_ = c + p; \
\
    int32_t *a_ = a + m_start * N; \
\
    asm volatile("vle32.v v18, (%0);" ::"r"(b_)); \
    int32_t *b__ = b_ + P; \
\
    int32_t *c__ = c_ + m_start * P; \
\
    int32_t t0, t1, t2, t3, t4, t5, t6, t7; \
\
    int32_t *a__ = a_; \
\
    asm volatile("vmv.v.i v0,  0"); \
    t0 = *a__, a__ += N; \
    asm volatile("vmv.v.i v2,  0"); \
    t1 = *a__, a__ += N; \
    asm volatile("vmv.v.i v4,  0"); \
    t2 = *a__, a__ += N; \
    asm volatile("vmv.v.i v6,  0"); \
    t3 = *a__, a__ += N; \
    asm volatile("vmv.v.i v8,  0"); \
    t4 = *a__, a__ += N; \
    asm volatile("vmv.v.i v10,  0"); \
    t5 = *a__, a__ += N; \
    asm volatile("vmv.v.i v12,  0"); \
    t6 = *a__, a__ += N; \
    asm volatile("vmv.v.i v14,  0"); \
    t7 = *a__; \
\
    unsigned long int n = 0; \
\
    while (n < N) { \
      a__ = a_ + ++n; \
\
      asm volatile("vle32.v v20, (%0);" ::"r"(b__)); \
      b__ += P; \
\
      asm volatile("vmacc.vx v0, %0, v18" ::"r"(t0)); \
      t0 = *a__, a__ += N; \
      asm volatile("vmacc.vx v2, %0, v18" ::"r"(t1)); \
      t1 = *a__, a__ += N; \
      asm volatile("vmacc.vx v4, %0, v18" ::"r"(t2)); \
      t2 = *a__, a__ += N; \
      asm volatile("vmacc.vx v6, %0, v18" ::"r"(t3)); \
      t3 = *a__, a__ += N; \
      asm volatile("vmacc.vx v8, %0, v18" ::"r"(t4)); \
      t4 = *a__, a__ += N; \
      asm volatile("vmacc.vx v10, %0, v18" ::"r"(t5)); \
      t5 = *a__, a__ += N; \
      asm volatile("vmacc.vx v12, %0, v18" ::"r"(t6)); \
      t6 = *a__, a__ += N; \
      asm volatile("vmacc.vx v14, %0, v18" ::"r"(t7)); \
      t7 = *a__; \
\
      a__ = a_ + ++n; \
\
      if (n == N) \
        break; \
\
      asm volatile("vle32.v v18, (%0);" ::"r"(b__)); \
      b__ += P; \
\
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0)); \
      t0 = *a__, a__ += N; \
      asm volatile("vmacc.vx v2, %0, v20" ::"r"(t1)); \
      t1 = *a__, a__ += N; \
      asm volatile("vmacc.vx v4, %0, v20" ::"r"(t2)); \
      t2 = *a__, a__ += N; \
      asm volatile("vmacc.vx v6, %0, v20" ::"r"(t3)); \
      t3 = *a__, a__ += N; \
      asm volatile("vmacc.vx v8, %0, v20" ::"r"(t4)); \
      t4 = *a__, a__ += N; \
      asm volatile("vmacc.vx v10, %0, v20" ::"r"(t5)); \
      t5 = *a__, a__ += N; \
      asm volatile("vmacc.vx v12, %0, v20" ::"r"(t6)); \
      t6 = *a__, a__ += N; \
      asm volatile("vmacc.vx v14, %0, v20" ::"r"(t7)); \
      t7 = *a__; \
    } \
\
    asm volatile("add zero, zero, %0" ::"r"(p_end)); \
\
    asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0)); \
    asm volatile("vse32.v v0, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v2, %0, v20" ::"r"(t1)); \
    asm volatile("vse32.v v2, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v4, %0, v20" ::"r"(t2)); \
    asm volatile("vse32.v v4, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v6, %0, v20" ::"r"(t3)); \
    asm volatile("vse32.v v6, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v8, %0, v20" ::"r"(t4)); \
    asm volatile("vse32.v v8, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v10, %0, v20" ::"r"(t5)); \
    asm volatile("vse32.v v10, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v12, %0, v20" ::"r"(t6)); \
    asm volatile("vse32.v v12, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v14, %0, v20" ::"r"(t7)); \
    asm volatile("vse32.v v14, (%0);" ::"r"(c__)); \
  } \
} while (0)

#define MATMUL_4XVLEN_PARA(c, a, b, m_start, m_end, N, p_start, p_end, P, vlen) do { \
\
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"(vlen)); \
\
  for (unsigned long int p = p_start; p < p_end; p += vlen) { \
    int32_t *b_ = b + p; \
    int32_t *c_ = c + p; \
\
    int32_t *a_ = a + m_start * N; \
\
    asm volatile("vle32.v v16, (%0);" ::"r"(b_)); \
\
    int32_t *b__ = b_ + P; \
    int32_t *c__ = c_ + m_start * P; \
\
    int32_t t0, t1, t2, t3; \
\
    int32_t *a__ = a_; \
\
    asm volatile("vmv.v.i v0,  0"); \
    t0 = *a__, a__ += N; \
    asm volatile("vmv.v.i v4,  0"); \
    t1 = *a__, a__ += N; \
    asm volatile("vmv.v.i v8,  0"); \
    t2 = *a__, a__ += N; \
    asm volatile("vmv.v.i v12,  0"); \
    t3 = *a__; \
\
    unsigned long int n = 0; \
\
    while (n < N) { \
      a__ = a_ + ++n; \
\
      asm volatile("vle32.v v20, (%0);" ::"r"(b__)); \
      b__ += P; \
\
      asm volatile("vmacc.vx v0, %0, v16" ::"r"(t0)); \
      t0 = *a__, a__ += N; \
      asm volatile("vmacc.vx v4, %0, v16" ::"r"(t1)); \
      t1 = *a__, a__ += N; \
      asm volatile("vmacc.vx v8, %0, v16" ::"r"(t2)); \
      t2 = *a__, a__ += N; \
      asm volatile("vmacc.vx v12, %0, v16" ::"r"(t3)); \
      t3 = *a__; \
\
      a__ = a_ + ++n; \
\
      if (n == N) \
        break; \
\
      asm volatile("vle32.v v16, (%0);" ::"r"(b__)); \
      b__ += P; \
\
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0)); \
      t0 = *a__, a__ += N; \
      asm volatile("vmacc.vx v4, %0, v20" ::"r"(t1)); \
      t1 = *a__, a__ += N; \
      asm volatile("vmacc.vx v8, %0, v20" ::"r"(t2)); \
      t2 = *a__, a__ += N; \
      asm volatile("vmacc.vx v12, %0, v20" ::"r"(t3)); \
      t3 = *a__; \
    } \
\
    asm volatile("add zero, zero, %0" ::"r"(p_end)); \
\
    asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0)); \
    asm volatile("vse32.v v0, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v4, %0, v20" ::"r"(t1)); \
    asm volatile("vse32.v v4, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v8, %0, v20" ::"r"(t2)); \
    asm volatile("vse32.v v8, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v12, %0, v20" ::"r"(t3)); \
    asm volatile("vse32.v v12, (%0);" ::"r"(c__)); \
  } \
} while (0)

#define MATMUL_2XVLEN_PARA(c, a, b, m_start, m_end, N, p_start, p_end, P, vlen) do { \
\
  asm volatile("vsetvli zero, %0, e32, m8, ta, ma" :: "r"(vlen)); \
\
  for (unsigned long int p = p_start; p < p_end; p += vlen) { \
    int32_t *b_ = b + p; \
    int32_t *c_ = c + p; \
\
    int32_t *a_ = a + m_start * N; \
\
    asm volatile("vle32.v v16, (%0);" ::"r"(b_)); \
\
    int32_t *b__ = b_ + P; \
    int32_t *c__ = c_ + m_start * P; \
\
    int32_t t0, t1; \
\
    int32_t *a__ = a_; \
\
    asm volatile("vmv.v.i v0,  0"); \
    t0 = *a__, a__ += N; \
    asm volatile("vmv.v.i v8,  0"); \
    t1 = *a__; \
\
    unsigned long int n = 0; \
\
    while (n < N) { \
      a__ = a_ + ++n; \
\
      asm volatile("vle32.v v24, (%0);" ::"r"(b__)); \
      b__ += P; \
\
      asm volatile("vmacc.vx v0, %0, v16" ::"r"(t0)); \
      t0 = *a__, a__ += N; \
      asm volatile("vmacc.vx v8, %0, v16" ::"r"(t1)); \
      t1 = *a__; \
\
      a__ = a_ + ++n; \
\
      if (n == N) \
        break; \
\
      asm volatile("vle32.v v16, (%0);" ::"r"(b__)); \
      b__ += P; \
\
      asm volatile("vmacc.vx v0, %0, v24" ::"r"(t0)); \
      t0 = *a__, a__ += N; \
      asm volatile("vmacc.vx v8, %0, v24" ::"r"(t1)); \
      t1 = *a__; \
    } \
\
    asm volatile("add zero, zero, %0" ::"r"(p_end)); \
\
    asm volatile("vmacc.vx v0, %0, v24" ::"r"(t0)); \
    asm volatile("vse32.v v0, (%0);" ::"r"(c__)); \
    c__ += P; \
    asm volatile("vmacc.vx v8, %0, v24" ::"r"(t1)); \
    asm volatile("vse32.v v8, (%0);" ::"r"(c__)); \
  } \
} while (0)

#endif
