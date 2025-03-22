// Copyright 2025 ETH Zurich and University of Bologna.
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

// Author: Navaneeth Kunhi Purayil, ETH Zurich <nkunhi@iis.ee.ethz.ch>

#include "gemv.h"

void gemv_v64b(double *a, double* b, double* c, int M, int N) {
  unsigned int vl, avl = N;
  double *a_ = a + (M-2) * N;
  double *b_ = b;

  for (int r=0; r < M/2; r++) {

    // Clean the accumulator
    asm volatile("vmv.s.x v24, zero");
    asm volatile("vmv.s.x v28, zero");

    // Stripmine and accumulate a partial reduced vector
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

      // Load chunk a and b
      asm volatile("vle64.v v4,  (%0)" ::"r"(b_));
      asm volatile("vle64.v v8, (%0)" ::"r"(a_));

      // Multiply and accumulate
      if (avl == N) {
        asm volatile("vfmul.vv v16, v8, v4");
      } else {
        asm volatile("vfmacc.vv v16, v8, v4");
      }

      asm volatile("vle64.v v12, (%0)" ::"r"(a_ + N));
      // Multiply and accumulate
      if (avl == N) {
        asm volatile("vfmul.vv v20, v12, v4");
      } else {
        asm volatile("vfmacc.vv v20, v12, v4");
      }

      // Bump pointers
      a_ += vl;
      b_ += vl;
      avl -= vl;
    } while (avl > 0);

    // Reduce and return
    asm volatile("vfredusum.vs v24, v20, v24");
    asm volatile("vfslide1up.vf v28, v24, %0" ::"f"(0.0));
    asm volatile("vfredusum.vs v28, v16, v28");
    asm volatile("vfslide1up.vf v24, v28, %0" ::"f"(0.0));
    b_ = b;
    a_ -= (3*N);
    avl = N;
  }
  
  asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(M));
  asm volatile("vse64.v v28,  (%0)" ::"r"(c));
  
}