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
// Author: Diyou Shen,              ETH Zurich <dishen@iis.ee.ethz.ch>

#include "sa_gemv.h"

void sa_gemv_cache_v64b_m4(double *a, double* b, uint16_t* dense_idx, double* c, int M, int M_core, int N_nz) {
  unsigned int vl, avl = M_core;
  double *a_start = a;
  double *c_ = c;

  do {
    // Reset the sparse vector pointer for every vertical block
    double *b_ = b;
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

    // 1. Initialize accumulators to exactly 0.0 (1 cycle overhead)
    asm volatile("vmv.v.i v4, 0");
    asm volatile("vmv.v.i v12, 0");

    int col = 0;
    for (; col < N_nz - 1; col += 2) {
      // Direct pointer calculation to the scattered columns
      double *a_col0 = a_start + ((size_t)dense_idx[col] * M);
      double *a_col1 = a_start + ((size_t)dense_idx[col + 1] * M);

      asm volatile("vle64.v v0, (%0)" ::"r"(a_col0));
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
      b_++;

      asm volatile("vle64.v v8, (%0)" ::"r"(a_col1));
      asm volatile("vfmacc.vf v12, %0, v8" ::"f"(*b_));
      b_++;
    }

    // 2. Handle Odd N boundary
    if (col < N_nz) {
      double *a_col0 = a_start + ((size_t)dense_idx[col] * M);
      asm volatile("vle64.v v0, (%0)" ::"r"(a_col0));
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
    }

    // 3. Combine the unrolled registers
    asm volatile("vfadd.vv v4, v4, v12");

    // 4. Store out results
    asm volatile("vse64.v v4, (%0)" ::"r"(c_));

    avl -= vl;
    c_ += vl;
    a_start += vl;
  } while (avl > 0);
}

void sa_gemv_cache_v32b_m4(float *a, float* b, uint16_t* dense_idx, float* c, int M, int M_core, int N_nz) {
  unsigned int vl, avl = M_core;
  float *a_start = a;
  float *c_ = c;

  do {
    float *b_ = b;
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

    // Initialize accumulators to 0
    asm volatile("vmv.v.i v4, 0");
    asm volatile("vmv.v.i v12, 0");

    int col = 0;
    for (; col < N_nz - 1; col += 2) {
      float *a_col0 = a_start + ((size_t)dense_idx[col] * M);
      float *a_col1 = a_start + ((size_t)dense_idx[col + 1] * M);

      asm volatile("vle32.v v0, (%0)" ::"r"(a_col0));
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
      b_++;

      asm volatile("vle32.v v8, (%0)" ::"r"(a_col1));
      asm volatile("vfmacc.vf v12, %0, v8" ::"f"(*b_));
      b_++;
    }

    if (col < N_nz) {
      float *a_col0 = a_start + ((size_t)dense_idx[col] * M);
      asm volatile("vle32.v v0, (%0)" ::"r"(a_col0));
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
    }

    asm volatile("vfadd.vv v4, v4, v12");
    
    // Direct overwrite of memory
    asm volatile("vse32.v v4, (%0)" ::"r"(c_));

    avl -= vl;
    c_ += vl;
    a_start += vl;
  } while (avl > 0);
}

void sa_gemv_cache_v16b_m4(__fp16 *a, __fp16* b, uint16_t* dense_idx, __fp16* c, int M, int M_core, int N_nz) {
  unsigned int vl, avl = M_core;
  __fp16 *a_start = a;
  __fp16 *c_ = c;

  do {
    __fp16 *b_ = b;
    asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));

    // Initialize accumulators to 0
    asm volatile("vmv.v.i v4, 0");
    asm volatile("vmv.v.i v12, 0");

    int col = 0;
    for (; col < N_nz - 1; col += 2) {
      __fp16 *a_col0 = a_start + ((size_t)dense_idx[col] * M);
      __fp16 *a_col1 = a_start + ((size_t)dense_idx[col + 1] * M);

      // Load chunk 0
      asm volatile("vle16.v v0, (%0)" ::"r"(a_col0));
      float t0; // Temp float register for Zfh extension
      asm volatile("flh %[t], 0(%[b])" : [t] "=f"(t0) : [b] "r"(b_));
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(t0));
      b_++;

      // Load chunk 1
      asm volatile("vle16.v v8, (%0)" ::"r"(a_col1));
      float t1;
      asm volatile("flh %[t], 0(%[b])" : [t] "=f"(t1) : [b] "r"(b_));
      asm volatile("vfmacc.vf v12, %0, v8" ::"f"(t1));
      b_++;
    }

    // Handle odd boundary
    if (col < N_nz) {
      __fp16 *a_col0 = a_start + ((size_t)dense_idx[col] * M);
      asm volatile("vle16.v v0, (%0)" ::"r"(a_col0));
      float t0;
      asm volatile("flh %[t], 0(%[b])" : [t] "=f"(t0) : [b] "r"(b_));
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(t0));
    }

    asm volatile("vfadd.vv v4, v4, v12");
    
    // Direct overwrite of memory
    asm volatile("vse16.v v4, (%0)" ::"r"(c_));

    avl -= vl;
    c_ += vl;
    a_start += vl;
  } while (avl > 0);
}


void gemv_v64b_m4(double *a, double* b, double* c, int M, int M_core, int N) {
  unsigned int vl, avl = M_core;
  double *a_, *a_start = a;
  double *c_ = c;

  do {
    a_ = a_start;
    double *b_ = b;
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));

    // 1. CLEAR ACCUMULATORS for every new vl block (0 encodes to +0.0 float)
    asm volatile("vmv.v.i v4, 0");
    asm volatile("vmv.v.i v12, 0");

    int col = 0;
    for (; col < N - 1; col += 2) {
      asm volatile("vle64.v v0, (%0)" ::"r"(a_));
      a_ += M;
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
      b_++;

      asm volatile("vle64.v v8, (%0)" ::"r"(a_));
      a_ += M;
      asm volatile("vfmacc.vf v12, %0, v8" ::"f"(*b_));
      b_++;
    }

    // 2. HANDLE ODD N BOUNDARY
    if (col < N) {
      asm volatile("vle64.v v0, (%0)" ::"r"(a_));
      a_ += M;
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
    }

    asm volatile("vfadd.vv v4, v4, v12");

    // 3. ACCUMULATE INTO MEMORY C (Load -> Add -> Store)
    asm volatile("vle64.v v16, (%0)" ::"r"(c_));
    asm volatile("vfadd.vv v4, v4, v16");
    asm volatile("vse64.v v4, (%0)" ::"r"(c_));

    avl -= vl;
    c_ += vl;
    a_start += vl;
  } while (avl > 0);
}

void gemv_v32b_m4(float *a, float* b, float* c, int M, int M_core, int N) {
  unsigned int vl, avl = M_core;
  float *a_, *a_start = a;
  float *c_ = c;

  do {
    a_ = a_start;
    float *b_ = b;
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

    asm volatile("vmv.v.i v4, 0");
    asm volatile("vmv.v.i v12, 0");

    int col = 0;
    for (; col < N - 1; col += 2) {
      asm volatile("vle32.v v0, (%0)" ::"r"(a_));
      a_ += M;
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
      b_++;

      asm volatile("vle32.v v8, (%0)" ::"r"(a_));
      a_ += M;
      asm volatile("vfmacc.vf v12, %0, v8" ::"f"(*b_));
      b_++;
    }

    if (col < N) {
      asm volatile("vle32.v v0, (%0)" ::"r"(a_));
      a_ += M;
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
    }

    asm volatile("vfadd.vv v4, v4, v12");

    asm volatile("vle32.v v16, (%0)" ::"r"(c_));
    asm volatile("vfadd.vv v4, v4, v16");
    asm volatile("vse32.v v12, (%0)" ::"r"(c_)); // wait, mapping v4 to v12? No, use v4.
    // Correction:
    // asm volatile("vse32.v v4, (%0)" ::"r"(c_));
    // Let's rewrite this block safely:
    asm volatile("vse32.v v4, (%0)" ::"r"(c_)); // Fixed register writeback

    avl -= vl;
    c_ += vl;
    a_start += vl;
  } while (avl > 0);
}

void gemv_v16b_m4(__fp16 *a, __fp16* b, __fp16* c, int M, int M_core, int N) {
  unsigned int vl, avl = M_core;
  __fp16 *a_, *a_start = a;
  __fp16 *c_ = c;

  do {
    a_ = a_start;
    __fp16 *b_ = b;
    asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));

    asm volatile("vmv.v.i v4, 0");
    asm volatile("vmv.v.i v12, 0");

    int col = 0;
    for (; col < N - 1; col += 2) {
      asm volatile("vle16.v v0, (%0)" ::"r"(a_));
      a_ += M;
      asm volatile("vle16.v v8, (%0)" ::"r"(a_));
      a_ += M;

      float t0, t1;
      asm volatile("flh %[t], 0(%[b])" : [t] "=f"(t0) : [b] "r"(b_));
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(t0));
      b_++;

      asm volatile("flh %[t], 0(%[b])" : [t] "=f"(t1) : [b] "r"(b_));
      asm volatile("vfmacc.vf v12, %0, v8" ::"f"(t1));
      b_++;
    }

    if (col < N) {
      asm volatile("vle16.v v0, (%0)" ::"r"(a_));
      a_ += M;
      float t0;
      asm volatile("flh %[t], 0(%[b])" : [t] "=f"(t0) : [b] "r"(b_));
      asm volatile("vfmacc.vf v4, %0, v0" ::"f"(t0));
    }

    asm volatile("vfadd.vv v4, v4, v12");

    asm volatile("vle16.v v16, (%0)" ::"r"(c_));
    asm volatile("vfadd.vv v4, v4, v16");
    asm volatile("vse16.v v4, (%0)" ::"r"(c_));

    avl -= vl;
    c_ += vl;
    a_start += vl;
  } while (avl > 0);
}
