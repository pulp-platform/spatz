// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>

#include "spmm_sparse.h"

#define SPMM_SPARSE_NOINLINE __attribute__((noinline))
#define SPMM_SPARSE_ALWAYS_INLINE __attribute__((always_inline)) inline

static SPMM_SPARSE_ALWAYS_INLINE void spmm_sparse_row_v64b_m4(
    const double *row_dense, const uint32_t *tile_col_idx, uint32_t start,
    uint32_t end, const double *b, uint32_t b_ld, double *c_row,
    uint32_t n_cols) {
  if (start == end) {
    for (uint32_t p = 0; p < n_cols; ++p) c_row[p] = 0.0;
    return;
  }

  uint32_t p = 0;
  while (p < n_cols) {
    uint32_t vl;
    const uint32_t avl = n_cols - p;
    const uint32_t first_col = tile_col_idx[start];
    const double first_a = row_dense[first_col];
    const double *b_row = b + (size_t)first_col * b_ld + p;

    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v16, (%0)" : : "r"(b_row));
    asm volatile("vfmul.vf v24, v16, %0" : : "f"(first_a));

    for (uint32_t k = start + 1; k < end; ++k) {
      const uint32_t col = tile_col_idx[k];
      const double a = row_dense[col];
      b_row = b + (size_t)col * b_ld + p;
      asm volatile("vle64.v v16, (%0)" : : "r"(b_row));
      asm volatile("vfmacc.vf v24, %0, v16" : : "f"(a));
    }

    asm volatile("vse64.v v24, (%0)" : : "r"(c_row + p));
    p += vl;
  }
}

SPMM_SPARSE_NOINLINE void spmm_sparse_v64b(
    const uint32_t *row_ptr, const uint32_t *col_idx, uint32_t nnz_base,
    const double *dense_a, uint32_t dense_ld, uint32_t row_base,
    const double *b, uint32_t b_ld, double *c, uint32_t c_ld,
    uint32_t row_start, uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row] - nnz_base;
    const uint32_t end = row_ptr[row + 1] - nnz_base;
    const double *row_dense = dense_a + (size_t)(row - row_base) * dense_ld;
    double *c_row = c + (size_t)row * c_ld;

    spmm_sparse_row_v64b_m4(row_dense, col_idx, start, end, b, b_ld, c_row,
                            c_ld);
  }
}
