// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>

#include "rs_spmv.h"

#define RS_SPMV_NOINLINE __attribute__((noinline))
#define RS_SPMV_ALWAYS_INLINE __attribute__((always_inline)) inline

// Compute dot product of a single dense row A[row,:] with x[:] using LMUL=m8.
// The row has K elements loaded contiguously with vle64.v.
static RS_SPMV_ALWAYS_INLINE double rs_spmv_row_v64b_m8(const double *a_row,
                                                         const double *x,
                                                         uint32_t K) {
  if (K == 0) return 0.0;

  const uint32_t orig_avl = K;
  uint32_t avl = K;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v8, (%0)" : : "r"(a_row));
    asm volatile("vle64.v v16, (%0)" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    a_row += vl;
    x += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m8, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

// Top-level row-sparse SpMV kernel.
// For each non-zero row index in nz_row_idx[start:end], compute
// y[row] = dot(A[row, :], x[:]).
RS_SPMV_NOINLINE void rs_spmv_v64b(const uint32_t *nz_row_idx,
                                    const double *dense_a, uint32_t a_ld,
                                    const double *x, double *y, uint32_t start,
                                    uint32_t end) {
  for (uint32_t i = start; i < end; ++i) {
    const uint32_t row = nz_row_idx[i];
    const double *a_row = dense_a + (size_t)row * a_ld;
    y[row] = rs_spmv_row_v64b_m8(a_row, x, a_ld);
  }
}
