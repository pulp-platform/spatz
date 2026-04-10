// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>

#include "rs_spmm.h"

#define RS_SPMM_NOINLINE __attribute__((noinline))
#define RS_SPMM_ALWAYS_INLINE __attribute__((always_inline)) inline

static RS_SPMM_ALWAYS_INLINE void rs_spmm_row_v64b_m4(
    const double *a_row, uint32_t a_ld, const double *b, uint32_t b_ld,
    double *c_row, uint32_t n_cols) {
  uint32_t p = 0;
  while (p < n_cols) {
    uint32_t vl;
    const uint32_t avl = n_cols - p;

    const double first_a = a_row[0];
    const double *b_row = b + p;

    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v16, (%0)" : : "r"(b_row));
    asm volatile("vfmul.vf v24, v16, %0" : : "f"(first_a));

    for (uint32_t k = 1; k < a_ld; ++k) {
      const double a = a_row[k];
      b_row = b + (size_t)k * b_ld + p;
      asm volatile("vle64.v v16, (%0)" : : "r"(b_row));
      asm volatile("vfmacc.vf v24, %0, v16" : : "f"(a));
    }

    asm volatile("vse64.v v24, (%0)" : : "r"(c_row + p));
    p += vl;
  }
}

RS_SPMM_NOINLINE void rs_spmm_v64b(const uint32_t *nz_row_idx,
                                     const double *dense_a, uint32_t a_ld,
                                     const double *b, uint32_t b_ld, double *c,
                                     uint32_t c_ld, uint32_t start,
                                     uint32_t end) {
  for (uint32_t i = start; i < end; ++i) {
    const uint32_t row = nz_row_idx[i];
    const double *a_row = dense_a + (size_t)row * a_ld;
    double *c_row = c + (size_t)row * c_ld;

    rs_spmm_row_v64b_m4(a_row, a_ld, b, b_ld, c_row, c_ld);
  }
}
