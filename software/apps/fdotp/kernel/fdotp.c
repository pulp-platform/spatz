// Copyright 2022 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include "fdotp.h"

// 64-bit dot-product: a * b
double fdotp_v64b(const double *a, const double *b, size_t avl) {
  size_t orig_avl = avl;
  size_t vl = 0;

  double red;

  double *a_ = (double *)a;
  double *b_ = (double *)b;

  // Clean the accumulator
  asm volatile("vmv.s.x v0, zero");

  // Stripmine and accumulate a partial reduced vector
  do {
    // Set the vl
    asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));

    // Load chunk a and b
    asm volatile("vle64.v v8,  (%0)" ::"r"(a_));
    asm volatile("vle64.v v16, (%0)" ::"r"(b_));

    // Multiply and accumulate
    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    // Bump pointers
    a_ += vl;
    b_ += vl;
    avl -= vl;
  } while (avl > 0);

  // Reduce and return
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));

  return red;
}

// 32-bit dot-product: a * b
float fdotp_v32b(const float *a, const float *b, size_t avl) {
  size_t orig_avl = avl;
  size_t vl = 0;

  float red;

  float *a_ = (float *)a;
  float *b_ = (float *)b;

  // Stripmine and accumulate a partial reduced vector
  do {
    // Set the vl
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));

    // Load chunk a and b
    asm volatile("vle32.v v8,  (%0)" ::"r"(a_));
    asm volatile("vle32.v v16, (%0)" ::"r"(b_));

    // Multiply and accumulate
    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    // Bump pointers
    a_ += vl;
    b_ += vl;
    avl -= vl;
  } while (avl > 0);

  // Reduce and return
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

// 16-bit dot-product: a * b
_Float16 fdotp_v16b(const _Float16 *a, const _Float16 *b, size_t avl) {
  size_t orig_avl = avl;
  size_t vl = 0;

  _Float16 red;

  _Float16 *a_ = (_Float16 *)a;
  _Float16 *b_ = (_Float16 *)b;

  // Stripmine and accumulate a partial reduced vector
  do {
    // Set the vl
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));

    // Load chunk a and b
    asm volatile("vle16.v v8,  (%0)" ::"r"(a_));
    asm volatile("vle16.v v16, (%0)" ::"r"(b_));

    // Multiply and accumulate
    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    // Bump pointers
    a_ += vl;
    b_ += vl;
    avl -= vl;
  } while (avl > 0);

  // Reduce and return
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

double fdotp_s64b(const double *a, const double *b, size_t avl) {
  double acc0, acc1, acc2, acc3, acc4, acc5, acc6, acc7;

  acc0 = 0;
  acc1 = 0;
  acc2 = 0;
  acc3 = 0;
  acc4 = 0;
  acc5 = 0;
  acc6 = 0;
  acc7 = 0;

  for (uint64_t i = 0; i < avl; i += 8) {
    acc0 += a[i + 0] * b[i + 0];
    acc1 += a[i + 1] * b[i + 1];
    acc2 += a[i + 2] * b[i + 2];
    acc3 += a[i + 3] * b[i + 3];
    acc4 += a[i + 4] * b[i + 4];
    acc5 += a[i + 5] * b[i + 5];
    acc6 += a[i + 6] * b[i + 6];
    acc7 += a[i + 7] * b[i + 7];
  }

  acc0 += acc1;
  acc2 += acc3;
  acc4 += acc5;
  acc6 += acc7;

  acc0 += acc2;
  acc4 += acc6;

  acc0 += acc4;

  return acc0;
}

float fdotp_s32b(const float *a, const float *b, size_t avl) {
  float acc0, acc1, acc2, acc3, acc4, acc5, acc6, acc7;

  acc0 = 0;
  acc1 = 0;
  acc2 = 0;
  acc3 = 0;
  acc4 = 0;
  acc5 = 0;
  acc6 = 0;
  acc7 = 0;

  for (uint64_t i = 0; i < avl; i += 8) {
    acc0 += a[i + 0] * b[i + 0];
    acc1 += a[i + 1] * b[i + 1];
    acc2 += a[i + 2] * b[i + 2];
    acc3 += a[i + 3] * b[i + 3];
    acc4 += a[i + 4] * b[i + 4];
    acc5 += a[i + 5] * b[i + 5];
    acc6 += a[i + 6] * b[i + 6];
    acc7 += a[i + 7] * b[i + 7];
  }

  acc0 += acc1;
  acc2 += acc3;
  acc4 += acc5;
  acc6 += acc7;

  acc0 += acc2;
  acc4 += acc6;

  acc0 += acc4;

  return acc0;
}

_Float16 fdotp_s16b(const _Float16 *a, const _Float16 *b, size_t avl) {
  _Float16 acc0, acc1, acc2, acc3, acc4, acc5, acc6, acc7;

  acc0 = 0;
  acc1 = 0;
  acc2 = 0;
  acc3 = 0;
  acc4 = 0;
  acc5 = 0;
  acc6 = 0;
  acc7 = 0;

  for (uint64_t i = 0; i < avl; i += 8) {
    acc0 += a[i + 0] * b[i + 0];
    acc1 += a[i + 1] * b[i + 1];
    acc2 += a[i + 2] * b[i + 2];
    acc3 += a[i + 3] * b[i + 3];
    acc4 += a[i + 4] * b[i + 4];
    acc5 += a[i + 5] * b[i + 5];
    acc6 += a[i + 6] * b[i + 6];
    acc7 += a[i + 7] * b[i + 7];
  }

  acc0 += acc1;
  acc2 += acc3;
  acc4 += acc5;
  acc6 += acc7;

  acc0 += acc2;
  acc4 += acc6;

  acc0 += acc4;

  return acc0;
}
