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

#include "matmul.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

uint32_t matmul(int32_t *c, const int32_t *a, const int32_t *b,
             const unsigned long int M, const unsigned long int N,
             const unsigned long int P, const uint32_t threadId,
             const uint32_t numThreads) {
  if (M <= 8 && numThreads > 1) {
    return matmul_2x2(c, a, b, M, N, P, threadId, numThreads);
  } else if (M <= 16 && numThreads == 1 || (M <= 16 && numThreads > 1)) {
    return matmul_4x4(c, a, b, M, N, P, threadId, numThreads);
  } else {
    return matmul_8x8(c, a, b, M, N, P, threadId, numThreads);
  }
}

// ---------------
// 2x2
// ---------------

uint32_t matmul_2x2(int32_t *c, const int32_t *a, const int32_t *b,
                const unsigned long int M, const unsigned long int N,
                const unsigned long int P, const uint32_t threadId,
                const uint32_t numThreads) {
  // We work on 4 rows of the matrix at once
  const unsigned long int block_size = 2;
  unsigned long int block_size_p;

  // Set the vector configuration
#ifdef DISABLE_MULTICORE
  asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(block_size_p) : "r"(P));
#else
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(block_size_p) : "r"(P));
#endif

  // Slice the matrix into a manageable number of columns p_
  for (unsigned long int p = (threadId%(M/block_size))*block_size_p; p < P; p += block_size_p*(numThreads/(M/block_size))) {
    // Set the vector length
    const unsigned long int p_ = MIN(P - p, block_size_p);

    // Find pointers to the submatrices
    const int32_t *b_ = b + p;
    int32_t *c_ = c + p;

#ifdef DISABLE_MULTICORE
    asm volatile("vsetvli zero, %0, e32, m2, ta, ma" ::"r"(p_));
#else
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma" ::"r"(p_));
#endif

    // Iterate over the rows
    for (unsigned long int m = threadId*block_size; m < M; m += block_size*numThreads) {
      // Find pointer to the submatrices
      const int32_t *a_ = a + m * N;

      // Prefetch one row of matrix B
      int32_t *b__ = b_;
      asm volatile("vle32.v v16, (%0);" ::"r"(b__));
      b__ += P;

      int32_t *c__ = c_ + m * P;

      // Temporary variables
      int32_t t0, t1;

      // Original pointer
      int32_t *a__ = a_;


      // Prefetch one row of scalar values
      asm volatile("vmv.v.i v0,  0");
      t0 = *a__, a__ += N;
      asm volatile("vmv.v.i v4,  0");
      t1 = *a__;

      // Compute the multiplication
      unsigned long int n = 0;

      while (n < N) {
        // Calculate pointer to the matrix A
        a__ = a_ + ++n;

        // Load one row of B
        asm volatile("vle32.v v20, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vmacc.vx v0, %0, v16" ::"r"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vmacc.vx v4, %0, v16" ::"r"(t1));
        t1 = *a__;

        a__ = a_ + ++n;

        if (n == N)
          break;

        // Load one row of B
        asm volatile("vle32.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vmacc.vx v4, %0, v20" ::"r"(t1));
        t1 = *a__;
      }

      // Last iteration: store results
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
      asm volatile("vse32.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v4, %0, v20" ::"r"(t1));
      asm volatile("vse32.v v4, (%0);" ::"r"(c__));
    }
  }

  return block_size_p;
}

// ---------------
// 4x4
// ---------------

uint32_t matmul_4x4(int32_t *c, const int32_t *a, const int32_t *b,
                 const unsigned long int M, const unsigned long int N,
                 const unsigned long int P, const uint32_t threadId,
                 const uint32_t numThreads) {
  // We work on 4 rows of the matrix at once
  const unsigned long int block_size = 4;
  unsigned long int block_size_p;

  // Set the vector configuration
#ifdef DISABLE_MULTICORE
  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(block_size_p) : "r"(P));
#else
  asm volatile("vsetvli %0, %1, e32, mf2, ta, ma" : "=r"(block_size_p) : "r"(P));
#endif

  // Slice the matrix into a manageable number of columns p_
  uint32_t increment = numThreads/(M/block_size);
  increment = increment == 0 ? 1 : increment;
  for (unsigned long int p = (threadId%(M/block_size))*block_size_p; p < P; p += block_size_p*increment) {
    // Set the vector length
    const unsigned long int p_ = MIN(P - p, block_size_p);

    // Find pointers to the submatrices
    const int32_t *b_ = b + p;
    int32_t *c_ = c + p;

#ifdef DISABLE_MULTICORE
    asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(p_));
#else
    asm volatile("vsetvli zero, %0, e32, mf2, ta, ma" ::"r"(p_));
#endif

    // Iterate over the rows
    for (unsigned long int m = threadId*block_size; m < M; m += block_size*numThreads) {
      // Find pointer to the submatrices
      const int32_t *a_ = a + m * N;

      // Prefetch one row of matrix B
      int32_t *b__ = b_;
      asm volatile("vle32.v v16, (%0);" ::"r"(b__));
      b__ += P;

      int32_t *c__ = c_ + m * P;

      // Temporary variables
      int32_t t0, t1, t2, t3;

      // Original pointer
      int32_t *a__ = a_;

      // Prefetch one row of scalar values and clear vector registers
      asm volatile("vmv.v.i v0,  0");
      t0 = *a__, a__ += N;
      asm volatile("vmv.v.i v4,  0");
      t1 = *a__, a__ += N;
      asm volatile("vmv.v.i v8,  0");
      t2 = *a__, a__ += N;
      asm volatile("vmv.v.i v12,  0");
      t3 = *a__;

      // Compute the multiplication
      unsigned long int n = 0;

      while (n < N) {
        // Calculate pointer to the matrix A
        a__ = a_ + ++n;

        // Load one row of B
        asm volatile("vle32.v v20, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vmacc.vx v0, %0, v16" ::"r"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vmacc.vx v4, %0, v16" ::"r"(t1));
        t1 = *a__, a__ += N;
        asm volatile("vmacc.vx v8, %0, v16" ::"r"(t2));
        t2 = *a__, a__ += N;
        asm volatile("vmacc.vx v12, %0, v16" ::"r"(t3));
        t3 = *a__;

        a__ = a_ + ++n;

        if (n == N)
          break;

        // Load one row of B
        asm volatile("vle32.v v16, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vmacc.vx v4, %0, v20" ::"r"(t1));
        t1 = *a__, a__ += N;
        asm volatile("vmacc.vx v8, %0, v20" ::"r"(t2));
        t2 = *a__, a__ += N;
        asm volatile("vmacc.vx v12, %0, v20" ::"r"(t3));
        t3 = *a__;
      }

      // Last iteration: store results
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
      asm volatile("vse32.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v4, %0, v20" ::"r"(t1));
      asm volatile("vse32.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v8, %0, v20" ::"r"(t2));
      asm volatile("vse32.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v12, %0, v20" ::"r"(t3));
      asm volatile("vse32.v v12, (%0);" ::"r"(c__));
    }
  }

  return block_size_p;
}

// ---------------
// 8x8
// ---------------

uint32_t matmul_8x8(int32_t *c, const int32_t *a, const int32_t *b,
                 const unsigned long int M, const unsigned long int N,
                 const unsigned long int P, const uint32_t threadId,
                 const uint32_t numThreads) {
  // We work on 8 rows of the matrix at once
  const unsigned long int block_size = 8;
  unsigned long int block_size_p;

  // Set the vector configuration
#ifdef DISABLE_MULTICORE
  asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(block_size_p) : "r"(P));
#else
  if (M == 32) asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(block_size_p) : "r"(P));
  else asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(block_size_p) : "r"(P));
#endif
  // Slice the matrix into a manageable number of columns p_
  uint32_t increment = numThreads/(M/block_size);
  increment = increment == 0 ? 1 : increment;
  for (unsigned long int p = (threadId%(M/block_size))*block_size_p; p < P; p += block_size_p*increment) {
    // Set the vector length
    const unsigned long int p_ = MIN(P - p, block_size_p);

    // Find pointers to the submatrices
    const int32_t *b_ = b + p;
    int32_t *c_ = c + p;

#ifdef DISABLE_MULTICORE
    asm volatile("vsetvli zero, %0, e32, m2, ta, ma" ::"r"(p_));
#else
    if (M == 32) asm volatile("vsetvli zero, %0, e32, m1, ta, ma" ::"r"(p_));
    else asm volatile("vsetvli zero, %0, e32, m2, ta, ma" ::"r"(p_));
#endif

    // Iterate over the rows
    for (unsigned long int m = threadId*block_size; m < M; m += block_size*numThreads) {
      // Find pointer to the submatrices
      const int32_t *a_ = a + m * N;

      // Prefetch one row of matrix B
      int32_t *b__ = b_;
      asm volatile("vle32.v v18, (%0);" ::"r"(b__));
      b__ += P;

      int32_t *c__ = c_ + m * P;

      // Temporary variables
      int32_t t0, t1, t2, t3, t4, t5, t6, t7;

      // Original pointer
      int32_t *a__ = a_;

      // Prefetch one row of scalar values
      asm volatile("vmv.v.i v0,  0");
      t0 = *a__, a__ += N;
      asm volatile("vmv.v.i v2,  0");
      t1 = *a__, a__ += N;
      asm volatile("vmv.v.i v4,  0");
      t2 = *a__, a__ += N;
      asm volatile("vmv.v.i v6,  0");
      t3 = *a__, a__ += N;
      asm volatile("vmv.v.i v8,  0");
      t4 = *a__, a__ += N;
      asm volatile("vmv.v.i v10,  0");
      t5 = *a__, a__ += N;
      asm volatile("vmv.v.i v12,  0");
      t6 = *a__, a__ += N;
      asm volatile("vmv.v.i v14,  0");
      t7 = *a__;

      // Compute the multiplication
      unsigned long int n = 0;

      while (n < N) {
        // Calculate pointer to the matrix A
        a__ = a_ + ++n;

        // Load one row of B
        asm volatile("vle32.v v20, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vmacc.vx v0, %0, v18" ::"r"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vmacc.vx v2, %0, v18" ::"r"(t1));
        t1 = *a__, a__ += N;
        asm volatile("vmacc.vx v4, %0, v18" ::"r"(t2));
        t2 = *a__, a__ += N;
        asm volatile("vmacc.vx v6, %0, v18" ::"r"(t3));
        t3 = *a__, a__ += N;
        asm volatile("vmacc.vx v8, %0, v18" ::"r"(t4));
        t4 = *a__, a__ += N;
        asm volatile("vmacc.vx v10, %0, v18" ::"r"(t5));
        t5 = *a__, a__ += N;
        asm volatile("vmacc.vx v12, %0, v18" ::"r"(t6));
        t6 = *a__, a__ += N;
        asm volatile("vmacc.vx v14, %0, v18" ::"r"(t7));
        t7 = *a__;

        a__ = a_ + ++n;

        if (n == N)
          break;

        // Load one row of B
        asm volatile("vle32.v v18, (%0);" ::"r"(b__));
        b__ += P;

        asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
        t0 = *a__, a__ += N;
        asm volatile("vmacc.vx v2, %0, v20" ::"r"(t1));
        t1 = *a__, a__ += N;
        asm volatile("vmacc.vx v4, %0, v20" ::"r"(t2));
        t2 = *a__, a__ += N;
        asm volatile("vmacc.vx v6, %0, v20" ::"r"(t3));
        t3 = *a__, a__ += N;
        asm volatile("vmacc.vx v8, %0, v20" ::"r"(t4));
        t4 = *a__, a__ += N;
        asm volatile("vmacc.vx v10, %0, v20" ::"r"(t5));
        t5 = *a__, a__ += N;
        asm volatile("vmacc.vx v12, %0, v20" ::"r"(t6));
        t6 = *a__, a__ += N;
        asm volatile("vmacc.vx v14, %0, v20" ::"r"(t7));
        t7 = *a__;
      }

      // Last iteration: store results
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
      asm volatile("vse32.v v0, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v2, %0, v20" ::"r"(t1));
      asm volatile("vse32.v v2, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v4, %0, v20" ::"r"(t2));
      asm volatile("vse32.v v4, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v6, %0, v20" ::"r"(t3));
      asm volatile("vse32.v v6, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v8, %0, v20" ::"r"(t4));
      asm volatile("vse32.v v8, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v10, %0, v20" ::"r"(t5));
      asm volatile("vse32.v v10, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v12, %0, v20" ::"r"(t6));
      asm volatile("vse32.v v12, (%0);" ::"r"(c__));
      c__ += P;
      asm volatile("vmacc.vx v14, %0, v20" ::"r"(t7));
      asm volatile("vse32.v v14, (%0);" ::"r"(c__));
    }
  }

  return block_size_p;
}
