// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>

#include "spmv_sparse.h"

#define SPMV_SPARSE_SMALL_ROW_THRESHOLD 4
#define SPMV_SPARSE_NOINLINE __attribute__((noinline))
#define SPMV_SPARSE_ALWAYS_INLINE __attribute__((always_inline)) inline

static SPMV_SPARSE_ALWAYS_INLINE double spmv_sparse_row_scalar(
    const double *row_dense, const uint32_t *x_off, const double *x,
    uint32_t start, uint32_t end) {
  double sum = 0.0;
  for (uint32_t k = start; k < end; ++k) {
    const uint32_t col = x_off[k] / sizeof(double);
    sum += row_dense[col] * x[col];
  }
  return sum;
}

static SPMV_SPARSE_ALWAYS_INLINE double spmv_sparse_row_v64b_m1(
    const double *row_dense, const uint32_t *x_off, const double *x,
    uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle32.v v16, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v8, (%0), v16" : : "r"(row_dense));
    asm volatile("vluxei32.v v24, (%0), v16" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v28, v8, v24");
    } else {
      asm volatile("vfmacc.vv v28, v8, v24");
    }

    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m1, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v28, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

static SPMV_SPARSE_ALWAYS_INLINE double spmv_sparse_row_v64b_m2(
    const double *row_dense, const uint32_t *x_off, const double *x,
    uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle32.v v4, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v8, (%0), v4" : : "r"(row_dense));
    asm volatile("vluxei32.v v10, (%0), v4" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v12, v8, v10");
    } else {
      asm volatile("vfmacc.vv v12, v8, v10");
    }

    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v12, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

static SPMV_SPARSE_ALWAYS_INLINE double spmv_sparse_row_v64b_m4(
    const double *row_dense, const uint32_t *x_off, const double *x,
    uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle32.v v4, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v8, (%0), v4" : : "r"(row_dense));
    asm volatile("vluxei32.v v16, (%0), v4" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m4, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

static SPMV_SPARSE_ALWAYS_INLINE double spmv_sparse_row_v64b_m8(
    const double *row_dense, const uint32_t *x_off, const double *x,
    uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle32.v v0, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v8, (%0), v0" : : "r"(row_dense));
    asm volatile("vluxei32.v v16, (%0), v0" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m8, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

SPMV_SPARSE_NOINLINE void spmv_sparse_v64b_m1(
    const uint32_t *row_ptr, const uint32_t *x_off, const double *dense,
    uint32_t dense_ld, const double *x, double *y, uint32_t row_start,
    uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;
    const double *row_dense = dense + (size_t)row * dense_ld;

    if (nnz < SPMV_SPARSE_SMALL_ROW_THRESHOLD) {
      y[row] = spmv_sparse_row_scalar(row_dense, x_off, x, start, end);
    } else {
      y[row] = spmv_sparse_row_v64b_m1(row_dense, x_off + start, x, nnz);
    }
  }
}

SPMV_SPARSE_NOINLINE void spmv_sparse_v64b_m2(
    const uint32_t *row_ptr, const uint32_t *x_off, const double *dense,
    uint32_t dense_ld, const double *x, double *y, uint32_t row_start,
    uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;
    const double *row_dense = dense + (size_t)row * dense_ld;

    if (nnz < SPMV_SPARSE_SMALL_ROW_THRESHOLD) {
      y[row] = spmv_sparse_row_scalar(row_dense, x_off, x, start, end);
    } else {
      y[row] = spmv_sparse_row_v64b_m2(row_dense, x_off + start, x, nnz);
    }
  }
}

SPMV_SPARSE_NOINLINE void spmv_sparse_v64b_m4(
    const uint32_t *row_ptr, const uint32_t *x_off, const double *dense,
    uint32_t dense_ld, const double *x, double *y, uint32_t row_start,
    uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;
    const double *row_dense = dense + (size_t)row * dense_ld;

    if (nnz < SPMV_SPARSE_SMALL_ROW_THRESHOLD) {
      y[row] = spmv_sparse_row_scalar(row_dense, x_off, x, start, end);
    } else {
      y[row] = spmv_sparse_row_v64b_m4(row_dense, x_off + start, x, nnz);
    }
  }
}

SPMV_SPARSE_NOINLINE void spmv_sparse_v64b_m8(
    const uint32_t *row_ptr, const uint32_t *x_off, const double *dense,
    uint32_t dense_ld, const double *x, double *y, uint32_t row_start,
    uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;
    const double *row_dense = dense + (size_t)row * dense_ld;

    if (nnz < SPMV_SPARSE_SMALL_ROW_THRESHOLD) {
      y[row] = spmv_sparse_row_scalar(row_dense, x_off, x, start, end);
    } else {
      y[row] = spmv_sparse_row_v64b_m8(row_dense, x_off + start, x, nnz);
    }
  }
}

SPMV_SPARSE_NOINLINE void spmv_sparse_v64b(
    const uint32_t *row_ptr, const uint32_t *x_off, const double *dense,
    uint32_t dense_ld, const double *x, double *y, uint32_t row_start,
    uint32_t row_end) {
#if (SPMV_LMUL == 1)
  spmv_sparse_v64b_m1(row_ptr, x_off, dense, dense_ld, x, y, row_start,
                      row_end);
#elif (SPMV_LMUL == 2)
  spmv_sparse_v64b_m2(row_ptr, x_off, dense, dense_ld, x, y, row_start,
                      row_end);
#elif (SPMV_LMUL == 4)
  spmv_sparse_v64b_m4(row_ptr, x_off, dense, dense_ld, x, y, row_start,
                      row_end);
#elif (SPMV_LMUL == 8)
  spmv_sparse_v64b_m8(row_ptr, x_off, dense, dense_ld, x, y, row_start,
                      row_end);
#else
#error "SPMV_LMUL must be one of 1, 2, 4, 8"
#endif
}
