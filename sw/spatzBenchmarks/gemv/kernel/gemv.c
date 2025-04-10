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

/*
// 34 % util
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
  
}*/

// 44 % util
void gemv_v64b(double *a, double* b, double* c, int M, int N) {
  unsigned int vl, avl = N;
  double *a_ = a + (M-2) * N;
  double *b_ = b;

  asm volatile("vmv.s.x v16, zero");
  asm volatile("vmv.s.x v20, zero");

  for (int r=0; r < M/2; r++) {

    // Clean the accumulator
    
    double *a2 = a_ + N; 

    // Stripmine and accumulate a partial reduced vector
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

      // Load chunk a and b
      asm volatile("vle64.v v0,  (%0)" ::"r"(b_));
      b_ += vl;
      asm volatile("vle64.v v4, (%0)" ::"r"(a_));
      a_ += vl;

      // Multiply and accumulate
      if (avl == N) {
        asm volatile("vfmul.vv v24, v4, v0");
      } else {
        asm volatile("vfmacc.vv v24, v4, v0");
      }

      asm volatile("vle64.v v8, (%0)" ::"r"(a2));
      a2 += vl;
      // Multiply and accumulate
      if (avl == N) {
        asm volatile("vfmul.vv v28, v8, v0");
      } else {
        asm volatile("vfmacc.vv v28, v8, v0");
      }

      /////// Unroll - 2 ////////
      avl-= vl;
      // Load chunk a and b
      asm volatile("vle64.v v12, (%0)" ::"r"(b_));
      b_ += vl;
      asm volatile("vle64.v v4, (%0)" ::"r"(a_));
      a_ += vl;

      // Multiply and accumulate
      asm volatile("vfmacc.vv v24, v4, v12");

      asm volatile("vle64.v v8, (%0)" ::"r"(a2));
      a2 += vl;

      // Multiply and accumulate
      asm volatile("vfmacc.vv v28, v8, v12");

      // Bump pointers
      avl -= vl;
    } while (avl > 0);

    // Reduce and return
    asm volatile("vfredusum.vs v16, v28, v16");
    asm volatile("vfslide1up.vf v20, v16, %0" ::"f"(0.0));
    asm volatile("vfredusum.vs v20, v24, v20");
    asm volatile("vfslide1up.vf v16, v20, %0" ::"f"(0.0));
    b_ = b;
    a_ -= (3*N);
    avl = N;
  }
  
  asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(M));
  asm volatile("vse64.v v20,  (%0)" ::"r"(c));
  
}

/*
// 35 % util
void gemv_v64b(double *a, double* b, double* c, int M, int N) {
  unsigned int vl, avl = N;
  double *a_ = a + (M-2) * N;
  double *b_ = b;
  double red;
  
  for (int r=0; r < M/2; r++) {

    // Clean the accumulator
    
    double *a2 = a_ + N; 
    asm volatile("vmv.s.x v16, zero");
    asm volatile("vmv.s.x v20, zero");

    // Stripmine and accumulate a partial reduced vector
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

      // Load chunk a and b
      asm volatile("vle64.v v0,  (%0)" ::"r"(b_));
      b_ += vl;
      asm volatile("vle64.v v4, (%0)" ::"r"(a_));
      a_ += vl;

      // Multiply and accumulate
      if (avl == N) {
        asm volatile("vfmul.vv v24, v4, v0");
      } else {
        asm volatile("vfmacc.vv v24, v4, v0");
      }

      asm volatile("vle64.v v8, (%0)" ::"r"(a2));
      a2 += vl;
      // Multiply and accumulate
      if (avl == N) {
        asm volatile("vfmul.vv v28, v8, v0");
      } else {
        asm volatile("vfmacc.vv v28, v8, v0");
      }

      /////// Unroll - 2 ////////
      avl-= vl;
      // Load chunk a and b
      asm volatile("vle64.v v12, (%0)" ::"r"(b_));
      b_ += vl;
      asm volatile("vle64.v v4, (%0)" ::"r"(a_));
      a_ += vl;

      // Multiply and accumulate
      asm volatile("vfmacc.vv v24, v4, v12");

      asm volatile("vle64.v v8, (%0)" ::"r"(a2));
      a2 += vl;

      // Multiply and accumulate
      asm volatile("vfmacc.vv v28, v8, v12");

      // Bump pointers
      avl -= vl;
    } while (avl > 0);

    // Reduce and return
    asm volatile("vfredusum.vs v16, v28, v16");
    asm volatile("vfmv.f.s %0, v16" : "=f"(red));
    c[M-2*r-1] = red; 
    asm volatile("vfredusum.vs v20, v24, v20");
    asm volatile("vfmv.f.s %0, v20" : "=f"(red));
    c[M-2*r-2] = red; 
    b_ = b;
    a_ -= (3*N);
    avl = N;
  }
}
*/

/*
// 29 % util
void gemv_v64b(double *a, double* b, double* c, int M, int N) {
  unsigned int vl, avl = N;
  double *a_ = a; // + (M-1) * N;
  double *b_ = b;

  for (int r=0; r < M; r++) {

    // Clean the accumulator
    double red;

    // Stripmine and accumulate a partial reduced vector
    do {
    // Set the vl
    asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));

    // Load chunk a and b
    asm volatile("vle64.v v8,  (%0)" ::"r"(a_));
    asm volatile("vle64.v v16, (%0)" ::"r"(b_));

    // Multiply and accumulate
    if (avl == N) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    // Bump pointers
    a_ += vl;
    b_ += vl;
    avl -= vl;

    if (avl <= 0) break;

    // Set the vl
    asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));

    // Load chunk a and b
    asm volatile("vle64.v v0, (%0)" ::"r"(a_));
    asm volatile("vle64.v v8, (%0)" ::"r"(b_));

    // Multiply and accumulate
    asm volatile("vfmacc.vv v24, v0, v8");

    // Bump pointers
    a_ += vl;
    b_ += vl;
    avl -= vl;

    if (avl <= 0) break;

    // Set the vl
    asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));

    // Load chunk a and b
    asm volatile("vle64.v v16, (%0)" ::"r"(a_));
    asm volatile("vle64.v v0, (%0)" ::"r"(b_));

    // Multiply and accumulate
    asm volatile("vfmacc.vv v24, v0, v16");

    // Bump pointers
    a_ += vl;
    b_ += vl;
    avl -= vl;

    } while (avl > 0);

    // Reduce and return
    asm volatile("vmv.s.x v8, zero");
    asm volatile("vfredusum.vs v8, v24, v8");
    asm volatile("vfmv.f.s %0, v8" : "=f"(red));
    c[r] = red;
    
    b_ = b;
    avl = N;
  }
  
  //asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(M));
  //asm volatile("vse64.v v28,  (%0)" ::"r"(c));
  
}*/
