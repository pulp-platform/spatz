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

// Author: Yichao Zhang, ETH Zurich
// Author: Enis Mustafa, ETH Zurich

#include "mxfmatmul.h"

// This first implementation works only with perfect tiling, i.e., with M and N that perfectly
// divide kernel_m and 2*kernel_n, respectively
void matmul_tiled_Bx2(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int m_end, const unsigned int n_end, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) {

  // Setup pointers
  const double *a_;
  double *c_;

  // Iterate over the output rows
  for (unsigned int m = m_start; m < m_end; m += kernel_m) {
    // Update the C Mtx pointer
    c_ = c + m * N;
    // Iterate over the output columns
    // We need 2* on the n dimension since this is a Bx2 kernel
    for (unsigned int n = 0; n < n_end; n += (kernel_n << 1)) {
      // Update the A Mtx pointer
      a_ = a + m * K;
      // Update B Mtx pointers
      const double *b_  = b + n * K;
      const double *b__ = b_ + nrelem_b;

      asm volatile("msettilem t1, %0" ::"r" (kernel_m):"t1");
      asm volatile("msettilen t2, %0" ::"r" (kernel_n):"t2");
      asm volatile("msettilek t3, %0" ::"r" (kernel_k):"t3");
      asm volatile("vsetvli zero, %0, e64, m4, ta, ma" ::"r"(vl));

      // Reset the result registers
      asm volatile("vmv.v.i v16, 0");
      asm volatile("vmv.v.i v24, 0");

      // Load Matrix A and B with shifting for avoiding the conflicts.
      // Double-buffering on matrix B.
      // Set the store C address
      asm volatile("mle64.v.a v0, (%0), %1;" ::"r"(a_), "r"(K));
      asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));

      // Reset the loop control variable.
      unsigned int k = 0;

      while (k < inner_loops - 1) {
        // ------------------------------------------//
        // ------------Loop Unrolling 1 ------------ //
        // ------------------------------------------//
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        asm volatile("mxfmacc.vv v16, v0, v4");
        asm volatile("mxfmacc.vv v24, v0, v8");

        b_ += kernel_k;
        a_ += kernel_k;
        k++;

        asm volatile("mle64.v.a v12, (%0), %1;" ::"r"(a_), "r"(K));
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
        b__ = b_ + nrelem_b;

        if (k == inner_loops - 1) break; // Kernel must be broken at here

        // ------------------------------------------//
        // ------------Loop Unrolling 2 ------------ //
        // ------------------------------------------//
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        asm volatile("mxfmacc.vv v16, v12, v4");
        asm volatile("mxfmacc.vv v24, v12, v8");

        b_ += kernel_k;
        a_ += kernel_k;
        k++;

        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
        asm volatile("mle64.v.a v0, (%0), %1;" ::"r"(a_), "r"(K)); // Question: Move this one line upper, then does not work. Why?
        b__ = b_ + nrelem_b;
      }

      asm volatile("mxfmacc.vv v16, v12, v4");
      asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
      asm volatile("mxfmacc.vv v24, v12, v8");
      asm volatile("mse64.v.c v16, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;  // Question: If give the abs value 4 to here, then the performance is better. Why?
      asm volatile("mse64.v.c v24, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;
    }
  }
}

// This first implementation works only with perfect tiling, i.e., with M and N that perfectly
// divide kernel_m and 4*kernel_n, respectively
void matmul_tiled_Bx4(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int m_end, const unsigned int n_end, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) {

  // Setup pointers
  const double *a_;
  double *c_;

  // Iterate over the output rows
  for (unsigned int m = m_start; m < m_end; m += kernel_m) {
    // Update the C Mtx pointer
    c_ = c + m * N;
    // Iterate over the output columns
    // We need 2* on the n dimension since this is a Bx2 kernel
    for (unsigned int n = 0; n < n_end; n += (kernel_n << 2)) {
      // Update the A Mtx pointer
      a_ = a + m * K;
      // Update B Mtx pointers
      const double *b_  = b + n * K;
      const double *b__ = b_ + nrelem_b;

      asm volatile("msettilem t1, %0" ::"r" (kernel_m):"t1");
      asm volatile("msettilen t2, %0" ::"r" (kernel_n):"t2");
      asm volatile("msettilek t3, %0" ::"r" (kernel_k):"t3");
      asm volatile("vsetvli zero, %0, e64, m4, ta, ma" ::"r"(vl));

      // Reset the result registers
      asm volatile("vmv.v.i v16, 0");
      asm volatile("vmv.v.i v20, 0");
      asm volatile("vmv.v.i v24, 0");
      asm volatile("vmv.v.i v28, 0");

      // Load Matrix A and B with shifting for avoiding the conflicts.
      // Double-buffering on matrix B.
      // Set the store C address
      asm volatile("mle64.v.a v0, (%0), %1;" ::"r"(a_), "r"(K));
      asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));

      // Reset the loop control variable.
      unsigned int k = 0;

      while (k < inner_loops - 1) {
        // ------------------------------------------//
        // ------------Loop Unrolling 1 ------------ //
        // ------------------------------------------//

        asm volatile("mxfmacc.vv v16, v0, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v20, v0, v8");
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v24, v0, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));

        b_ += kernel_k;
        a_ += kernel_k;
        asm volatile("mxfmacc.vv v28, v0, v8");
        k++;

        asm volatile("mle64.v.a v12, (%0), %1;" ::"r"(a_), "r"(K));
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
        b__ = b_ + nrelem_b;

        if (k == inner_loops - 1) break; // Kernel must be broken at here

        // ------------------------------------------//
        // ------------Loop Unrolling 2 ------------ //
        // ------------------------------------------//
        asm volatile("mxfmacc.vv v16, v12, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v20, v12, v8");
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v24, v12, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));

        b_ += kernel_k;
        a_ += kernel_k;
        asm volatile("mxfmacc.vv v28, v12, v8");
        k++;

        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
        asm volatile("mle64.v.a v0, (%0), %1;" ::"r"(a_), "r"(K)); // Question: Move this one line upper, then does not work. Why?
        b__ = b_ + nrelem_b;
      }

      asm volatile("mxfmacc.vv v16, v12, v4");
      asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
      b__ += nrelem_b;

      asm volatile("mxfmacc.vv v20, v12, v8");
      asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
      b__ += nrelem_b;
      asm volatile("mse64.v.c v16, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mxfmacc.vv v24, v12, v4");
      asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
      asm volatile("mse64.v.c v20, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mxfmacc.vv v28, v12, v8");
      asm volatile("mse64.v.c v24, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mse64.v.c v28, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;
    }
  }
}

// This first implementation works only with perfect tiling, i.e., with M and N that perfectly
// divide kernel_m and 8*kernel_n, respectively
void matmul_tiled_Bx8(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int m_end, const unsigned int n_end, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) {

  // Setup pointers
  const double *a_;
  double *c_;

  // Iterate over the output rows
  for (unsigned int m = m_start; m < m_end; m += kernel_m) {
    // Update the C Mtx pointer
    c_ = c + m * N;
    // Iterate over the output columns
    // We need 2* on the n dimension since this is a Bx2 kernel
    for (unsigned int n = 0; n < n_end; n += (kernel_n << 3)) {
      // Update the A Mtx pointer
      a_ = a + m * K;
      // Update B Mtx pointers
      const double *b_  = b + n * K;
      const double *b__ = b_ + nrelem_b;

      asm volatile("msettilem t1, %0" ::"r" (kernel_m):"t1");
      asm volatile("msettilen t2, %0" ::"r" (kernel_n):"t2");
      asm volatile("msettilek t3, %0" ::"r" (kernel_k):"t3");
      asm volatile("vsetvli zero, %0, e64, m4, ta, ma" ::"r"(vl));

      // Reset the result registers
      asm volatile("vmv.v.i v16, 0");
      asm volatile("vmv.v.i v18, 0");
      asm volatile("vmv.v.i v20, 0");
      asm volatile("vmv.v.i v22, 0");
      asm volatile("vmv.v.i v24, 0");
      asm volatile("vmv.v.i v26, 0");
      asm volatile("vmv.v.i v28, 0");
      asm volatile("vmv.v.i v30, 0");
      asm volatile("vsetvli zero, %0, e64, m4, ta, ma" ::"r"(vl));

      // Load Matrix A and B with shifting for avoiding the conflicts.
      // Double-buffering on matrix B.
      // Set the store C address
      asm volatile("mle64.v.a v0, (%0), %1;" ::"r"(a_), "r"(K));
      asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));

      // Reset the loop control variable.
      unsigned int k = 0;

      while (k < inner_loops - 1) {
        // ------------------------------------------//
        // ------------Loop Unrolling 1 ------------ //
        // ------------------------------------------//
        asm volatile("mxfmacc.vv v16, v0, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v18, v0, v8");
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v20, v0, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v22, v0, v8");
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v24, v0, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v26, v0, v8");
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v28, v0, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));

        b_ += kernel_k;
        a_ += kernel_k;
        asm volatile("mxfmacc.vv v30, v0, v8");
        k++;

        asm volatile("mle64.v.a v12, (%0), %1;" ::"r"(a_), "r"(K));
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
        b__ = b_ + nrelem_b;

        if (k == inner_loops - 1) break; // Kernel must be broken at here

        // ------------------------------------------//
        // ------------Loop Unrolling 2 ------------ //
        // ------------------------------------------//
        asm volatile("mxfmacc.vv v16, v12, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v18, v12, v8");
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v20, v12, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v22, v12, v8");
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v24, v12, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v26, v12, v8");
        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
        b__ += nrelem_b;

        asm volatile("mxfmacc.vv v28, v12, v4");
        asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));

        b_ += kernel_k;
        a_ += kernel_k;
        asm volatile("mxfmacc.vv v30, v12, v8");
        k++;

        asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
        asm volatile("mle64.v.a v0, (%0), %1;" ::"r"(a_), "r"(K)); // Question: Move this one line upper, then does not work. Why?
        b__ = b_ + nrelem_b;
      }

      asm volatile("mxfmacc.vv v16, v12, v4");
      asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
      b__ += nrelem_b;

      asm volatile("mxfmacc.vv v18, v12, v8");
      asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
      b__ += nrelem_b;
      asm volatile("mse64.v.c v16, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mxfmacc.vv v20, v12, v4");
      asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
      b__ += nrelem_b;
      asm volatile("mse64.v.c v18, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mxfmacc.vv v22, v12, v8");
      asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
      b__ += nrelem_b;
      asm volatile("mse64.v.c v20, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mxfmacc.vv v24, v12, v4");
      asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
      b__ += nrelem_b;
      asm volatile("mse64.v.c v22, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mxfmacc.vv v26, v12, v8");
      asm volatile("mle64.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
      b__ += nrelem_b;
      asm volatile("mse64.v.c v24, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mxfmacc.vv v28, v12, v4");
      asm volatile("mle64.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
      asm volatile("mse64.v.c v26, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mxfmacc.vv v30, v12, v8");
      asm volatile("mse64.v.c v28, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;

      asm volatile("mse64.v.c v30, (%0), %1;" ::"r"(c_), "r"(N));
      c_ += kernel_n;
    }
  }
}
