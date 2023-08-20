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

void matmul_tiled_Bx2_conflict_shift(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int n_start, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) {

  asm volatile("msettilem t1, %0" ::"r" (kernel_m):"t1");
  asm volatile("msettilen t2, %0" ::"r" (kernel_n):"t2");
  asm volatile("msettilek t3, %0" ::"r" (kernel_k):"t3");
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(vl));

  // Reset the result registers
  asm volatile("vmv.v.i v16, 0");
  asm volatile("vmv.v.i v24, 0");

  // Load Matrix A and B with shifting for avoiding the conflicts.
  // Double-buffering on matrix B.
  // Set the store C address
  const double *a_ = a;
  asm volatile("mle32.v.a v0, (%0), %1;" ::"r"(a_), "r"(K));
  const double *b_ = b;
  asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
  const double *b__ = b_ + nrelem_b;
  double *c_ = c;

  // Reset the loop control variable.
  unsigned int n = 0;

  while (n < inner_loops - 1) {
    // ------------------------------------------//
    // ------------Loop Unrolling 1 ------------ //
    // ------------------------------------------//
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    asm volatile("mxfmacc.vv v16, v0, v4");
    asm volatile("mxfmacc.vv v24, v0, v8");

    b_ += kernel_k;
    a_ += kernel_k;
    n++;

    asm volatile("mle32.v.a v12, (%0), %1;" ::"r"(a_), "r"(K));
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
    b__ = b_ + nrelem_b;

    if (n == inner_loops - 1) break; // Kernel must be broken at here

    // ------------------------------------------//
    // ------------Loop Unrolling 2 ------------ //
    // ------------------------------------------//
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    asm volatile("mxfmacc.vv v16, v12, v4");
    asm volatile("mxfmacc.vv v24, v12, v8");

    b_ += kernel_k;
    a_ += kernel_k;
    n++;

    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
    asm volatile("mle32.v.a v0, (%0), %1;" ::"r"(a_), "r"(K)); // Question: Move this one line upper, then does not work. Why?
    b__ = b_ + nrelem_b;
  }

  asm volatile("mxfmacc.vv v16, v12, v4");
  asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
  asm volatile("mxfmacc.vv v24, v12, v8");
  asm volatile("mse32.v.c v16, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;  // Question: If give the abs value 4 to here, then the performance is better. Why?
  asm volatile("mse32.v.c v24, (%0), %1;" ::"r"(c_), "r"(N));
}

void matmul_tiled_Bx4_conflict_shift(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int n_start, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) {

  asm volatile("msettilem t1, %0" ::"r" (kernel_m):"t1");
  asm volatile("msettilen t2, %0" ::"r" (kernel_n):"t2");
  asm volatile("msettilek t3, %0" ::"r" (kernel_k):"t3");

  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(vl));
  // Reset the result registers
  asm volatile("vmv.v.i v16, 0");
  asm volatile("vmv.v.i v20, 0");
  asm volatile("vmv.v.i v24, 0");
  asm volatile("vmv.v.i v28, 0");

  // Load Matrix A and B with shifting for avoiding the conflicts.
  // Double-buffering on matrix B.
  // Set the store C address
  const double *a_ = a;
  asm volatile("mle32.v.a v0, (%0), %1;" ::"r"(a_), "r"(K));
  const double *b_ = b;
  asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
  const double *b__ = b_ + nrelem_b;
  double *c_ = c;

  // Reset the loop control variable.
  unsigned int n = 0;

  while (n < inner_loops - 1) {
    // ------------------------------------------//
    // ------------Loop Unrolling 1 ------------ //
    // ------------------------------------------//

    asm volatile("mxfmacc.vv v16, v0, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v20, v0, v8");
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v24, v0, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));

    b_ += kernel_k;
    a_ += kernel_k;
    asm volatile("mxfmacc.vv v28, v0, v8");
    n++;

    asm volatile("mle32.v.a v12, (%0), %1;" ::"r"(a_), "r"(K));
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
    b__ = b_ + nrelem_b;

    if (n == inner_loops - 1) break; // Kernel must be broken at here

    // ------------------------------------------//
    // ------------Loop Unrolling 2 ------------ //
    // ------------------------------------------//
    asm volatile("mxfmacc.vv v16, v12, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v20, v12, v8");
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v24, v12, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));

    b_ += kernel_k;
    a_ += kernel_k;
    asm volatile("mxfmacc.vv v28, v12, v8");
    n++;

    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
    asm volatile("mle32.v.a v0, (%0), %1;" ::"r"(a_), "r"(K)); // Question: Move this one line upper, then does not work. Why?
    b__ = b_ + nrelem_b;
  }

  asm volatile("mxfmacc.vv v16, v12, v4");
  asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
  b__ += nrelem_b;

  asm volatile("mxfmacc.vv v20, v12, v8");
  asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
  b__ += nrelem_b;
  asm volatile("mse32.v.c v16, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mxfmacc.vv v24, v12, v4");
  asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
  asm volatile("mse32.v.c v20, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mxfmacc.vv v28, v12, v8");
  asm volatile("mse32.v.c v24, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mse32.v.c v28, (%0), %1;" ::"r"(c_), "r"(N));
}

void matmul_tiled_Bx8_conflict_shift(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int n_start, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) {

  asm volatile("msettilem t1, %0" ::"r" (kernel_m):"t1");
  asm volatile("msettilen t2, %0" ::"r" (kernel_n):"t2");
  asm volatile("msettilek t3, %0" ::"r" (kernel_k):"t3");

  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(32));
  // Reset the result registers
  asm volatile("vmv.v.i v16, 0");
  asm volatile("vmv.v.i v18, 0");
  asm volatile("vmv.v.i v20, 0");
  asm volatile("vmv.v.i v22, 0");
  asm volatile("vmv.v.i v24, 0");
  asm volatile("vmv.v.i v26, 0");
  asm volatile("vmv.v.i v28, 0");
  asm volatile("vmv.v.i v30, 0");
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(vl));

  // Load Matrix A and B with shifting for avoiding the conflicts.
  // Double-buffering on matrix B.
  // Set the store C address
  const double *a_ = a;
  asm volatile("mle32.v.a v0, (%0), %1;" ::"r"(a_), "r"(K));
  const double *b_ = b;
  asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
  const double *b__ = b_ + nrelem_b;
  double *c_ = c;

  // Reset the loop control variable.
  unsigned int n = 0;

  while (n < inner_loops - 1) {
    // ------------------------------------------//
    // ------------Loop Unrolling 1 ------------ //
    // ------------------------------------------//
    asm volatile("mxfmacc.vv v16, v0, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v18, v0, v8");
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v20, v0, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v22, v0, v8");
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v24, v0, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v26, v0, v8");
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v28, v0, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));

    b_ += kernel_k;
    a_ += kernel_k;
    asm volatile("mxfmacc.vv v30, v0, v8");
    n++;

    asm volatile("mle32.v.a v12, (%0), %1;" ::"r"(a_), "r"(K));
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
    b__ = b_ + nrelem_b;

    if (n == inner_loops - 1) break; // Kernel must be broken at here

    // ------------------------------------------//
    // ------------Loop Unrolling 2 ------------ //
    // ------------------------------------------//
    asm volatile("mxfmacc.vv v16, v12, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v18, v12, v8");
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v20, v12, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v22, v12, v8");
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v24, v12, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v26, v12, v8");
    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
    b__ += nrelem_b;

    asm volatile("mxfmacc.vv v28, v12, v4");
    asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));

    b_ += kernel_k;
    a_ += kernel_k;
    asm volatile("mxfmacc.vv v30, v12, v8");
    n++;

    asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(K));
    asm volatile("mle32.v.a v0, (%0), %1;" ::"r"(a_), "r"(K)); // Question: Move this one line upper, then does not work. Why?
    b__ = b_ + nrelem_b;
  }

  asm volatile("mxfmacc.vv v16, v12, v4");
  asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
  b__ += nrelem_b;

  asm volatile("mxfmacc.vv v18, v12, v8");
  asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
  b__ += nrelem_b;
  asm volatile("mse32.v.c v16, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mxfmacc.vv v20, v12, v4");
  asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
  b__ += nrelem_b;
  asm volatile("mse32.v.c v18, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mxfmacc.vv v22, v12, v8");
  asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
  b__ += nrelem_b;
  asm volatile("mse32.v.c v20, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mxfmacc.vv v24, v12, v4");
  asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
  b__ += nrelem_b;
  asm volatile("mse32.v.c v22, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mxfmacc.vv v26, v12, v8");
  asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b__), "r"(K));
  b__ += nrelem_b;
  asm volatile("mse32.v.c v24, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mxfmacc.vv v28, v12, v4");
  asm volatile("mle32.v.b v8, (%0), %1;" ::"r"(b__), "r"(K));
  asm volatile("mse32.v.c v26, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mxfmacc.vv v30, v12, v8");
  asm volatile("mse32.v.c v28, (%0), %1;" ::"r"(c_), "r"(N));
  c_ += kernel_n;

  asm volatile("mse32.v.c v30, (%0), %1;" ::"r"(c_), "r"(N));
}
