// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include "spmv.h"

#define SPMV_SMALL_ROW_THRESHOLD 4

// Reduce one CSR row worth of products for fp64.
// x_off contains 32-bit byte offsets into x. On this rv32 target, vluxei64 is
// not available, so the gather indices must remain 32-bit even though all data
// arrays are 8-byte aligned.
static inline double spmv_row_v64b(const double *val, const uint32_t *x_off,
                                   const double *x, uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;
  const uint32_t cid = snrt_cluster_core_idx();

  asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    // Stripmine the remaining non-zeros in this row.
    asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl));

    // v8  <- val[k : k+vl]
    // v16 <- 32-bit byte offsets for x[col_idx[k : k+vl]]
    asm volatile("vle64.v v8, (%0)" ::"r"(val));
    asm volatile("vle32.v v16, (%0)" ::"r"(x_off));

    // v24 <- gathered x values using the per-entry byte offsets.
    asm volatile("vluxei32.v v24, (%0), v16" ::"r"(x));

    if (avl == orig_avl) {
      // First chunk initializes the accumulation vector.
      asm volatile("vfmul.vv v28, v8, v24");
    } else {
      // Later chunks accumulate into the same vector accumulator.
      asm volatile("vfmacc.vv v28, v8, v24");
    }

    // Advance the stripmined row window.
    val += vl;
    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  // Reduce the accumulated products in v28 to one scalar sum in v0[0].
  asm volatile("vsetvli zero, %0, e64, m1, ta, ma" ::"r"(orig_avl));
  asm volatile("vfredusum.vs v0, v28, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));

  return red;
}

// Top-level fp64 SpMV: scalar fallback for very short rows, vector path
// otherwise.
void spmv_v64b(const uint32_t *row_ptr, const uint32_t *x_off, const double *val,
               const double *x, double *y, uint32_t row_start,
               uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;

    if (nnz < SPMV_SMALL_ROW_THRESHOLD) {
      double sum = 0.0;
      for (uint32_t k = start; k < end; ++k) {
        sum += val[k] * x[x_off[k] / sizeof(double)];
      }
      y[row] = sum;
    } else {
      y[row] = spmv_row_v64b(val + start, x_off + start, x, nnz);
    }
  }
}
