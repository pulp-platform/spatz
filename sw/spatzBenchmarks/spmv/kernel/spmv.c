// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include "spmv.h"

#define SPMV_SMALL_ROW_THRESHOLD 4
#define SPMV_NOINLINE __attribute__((noinline))
#define SPMV_ALWAYS_INLINE __attribute__((always_inline)) inline

static SPMV_ALWAYS_INLINE double spmv_row_scalar(const double *val,
                                                 const uint32_t *x_off,
                                                 const double *x,
                                                 uint32_t start,
                                                 uint32_t end);

static SPMV_ALWAYS_INLINE double spmv_row_v64b_m1(const double *val,
                                                  const uint32_t *x_off,
                                                  const double *x,
                                                  uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v8, (%0)" : : "r"(val));
    asm volatile("vle32.v v16, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v24, (%0), v16" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v28, v8, v24");
    } else {
      asm volatile("vfmacc.vv v28, v8, v24");
    }

    val += vl;
    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m1, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v28, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

static SPMV_ALWAYS_INLINE double spmv_row_v64b_m2(const double *val,
                                                  const uint32_t *x_off,
                                                  const double *x,
                                                  uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v8, (%0)" : : "r"(val));
    asm volatile("vle32.v v4, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v10, (%0), v4" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v12, v8, v10");
    } else {
      asm volatile("vfmacc.vv v12, v8, v10");
    }

    val += vl;
    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v12, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

static SPMV_ALWAYS_INLINE double spmv_row_v64b_m4(const double *val,
                                                  const uint32_t *x_off,
                                                  const double *x,
                                                  uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v8, (%0)" : : "r"(val));
    asm volatile("vle32.v v4, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v16, (%0), v4" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    val += vl;
    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m4, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

static SPMV_ALWAYS_INLINE void spmv_row_v64b_m4_bank0_accumulate(
    const double *val, const uint32_t *x_off, const double *x, uint32_t avl) {
  const uint32_t orig_avl = avl;
  uint32_t vl;

  asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v8, (%0)" : : "r"(val));
    asm volatile("vle32.v v4, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v12, (%0), v4" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v0, v8, v12");
    } else {
      asm volatile("vfmacc.vv v0, v8, v12");
    }

    val += vl;
    x_off += vl;
    avl -= vl;
  } while (avl > 0);
}

static SPMV_ALWAYS_INLINE void spmv_row_v64b_m4_bank1_accumulate(
    const double *val, const uint32_t *x_off, const double *x, uint32_t avl) {
  const uint32_t orig_avl = avl;
  uint32_t vl;

  asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v16, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v24, (%0)" : : "r"(val));
    asm volatile("vle32.v v20, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v28, (%0), v20" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v16, v24, v28");
    } else {
      asm volatile("vfmacc.vv v16, v24, v28");
    }

    val += vl;
    x_off += vl;
    avl -= vl;
  } while (avl > 0);
}

static SPMV_ALWAYS_INLINE void spmv_row_v64b_m4_bank0_start_reduce(
    uint32_t orig_avl) {
  asm volatile("vsetvli zero, %0, e64, m4, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v12, zero");
  asm volatile("vfredusum.vs v12, v0, v12");
}

static SPMV_ALWAYS_INLINE void spmv_row_v64b_m4_bank1_start_reduce(
    uint32_t orig_avl) {
  asm volatile("vsetvli zero, %0, e64, m4, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v28, zero");
  asm volatile("vfredusum.vs v28, v16, v28");
}

static SPMV_ALWAYS_INLINE double spmv_row_v64b_m4_bank0_finish_reduce(void) {
  double red;
  asm volatile("vfmv.f.s %0, v12" : "=f"(red));
  return red;
}

static SPMV_ALWAYS_INLINE double spmv_row_v64b_m4_bank1_finish_reduce(void) {
  double red;
  asm volatile("vfmv.f.s %0, v28" : "=f"(red));
  return red;
}

static SPMV_ALWAYS_INLINE void spmv_row_v64b_m4_unrolled(const uint32_t *row_ptr,
                                                          const uint32_t *x_off,
                                                          const double *val,
                                                          const double *x,
                                                          double *y,
                                                          uint32_t row_start,
                                                          uint32_t row_end) {
  uint32_t pending_row = 0;
  uint32_t pending_orig_avl = 0;
  int pending_bank0 = 0;
  int has_pending = 0;

  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;

    if (nnz < SPMV_SMALL_ROW_THRESHOLD) {
      if (has_pending) {
        if (pending_bank0) {
          spmv_row_v64b_m4_bank0_start_reduce(pending_orig_avl);
          y[pending_row] = spmv_row_v64b_m4_bank0_finish_reduce();
        } else {
          spmv_row_v64b_m4_bank1_start_reduce(pending_orig_avl);
          y[pending_row] = spmv_row_v64b_m4_bank1_finish_reduce();
        }
        has_pending = 0;
      }

      y[row] = spmv_row_scalar(val, x_off, x, start, end);
      continue;
    }

    if (!has_pending) {
      // Prime the two-bank pipeline with the first vector row in bank 0.
      spmv_row_v64b_m4_bank0_accumulate(val + start, x_off + start, x, nnz);
      pending_row = row;
      pending_orig_avl = nnz;
      pending_bank0 = 1;
      has_pending = 1;
      continue;
    }

    if (pending_bank0) {
      // Issue the reduction for the pending row in bank 0, then use bank 1
      // for the next row's load/gather/FMA work.
      spmv_row_v64b_m4_bank0_start_reduce(pending_orig_avl);
      spmv_row_v64b_m4_bank1_accumulate(val + start, x_off + start, x, nnz);
      y[pending_row] = spmv_row_v64b_m4_bank0_finish_reduce();
      pending_bank0 = 0;
    } else {
      // Swap the two banks and keep one vector row in flight.
      spmv_row_v64b_m4_bank1_start_reduce(pending_orig_avl);
      spmv_row_v64b_m4_bank0_accumulate(val + start, x_off + start, x, nnz);
      y[pending_row] = spmv_row_v64b_m4_bank1_finish_reduce();
      pending_bank0 = 1;
    }

    pending_row = row;
    pending_orig_avl = nnz;
    has_pending = 1;
  }

  if (has_pending) {
    if (pending_bank0) {
      spmv_row_v64b_m4_bank0_start_reduce(pending_orig_avl);
      y[pending_row] = spmv_row_v64b_m4_bank0_finish_reduce();
    } else {
      spmv_row_v64b_m4_bank1_start_reduce(pending_orig_avl);
      y[pending_row] = spmv_row_v64b_m4_bank1_finish_reduce();
    }
  }
}

static SPMV_ALWAYS_INLINE double spmv_row_v64b_m8(const double *val,
                                                  const uint32_t *x_off,
                                                  const double *x,
                                                  uint32_t avl) {
  if (avl == 0) return 0.0;

  const uint32_t orig_avl = avl;
  uint32_t vl;
  double red = 0.0;

  asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v8, (%0)" : : "r"(val));
    asm volatile("vle32.v v0, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v16, (%0), v0" : : "r"(x));

    if (avl == orig_avl) {
      asm volatile("vfmul.vv v24, v8, v16");
    } else {
      asm volatile("vfmacc.vv v24, v8, v16");
    }

    val += vl;
    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m8, ta, ma" : : "r"(orig_avl));
  asm volatile("vmv.s.x v0, zero");
  asm volatile("vfredusum.vs v0, v24, v0");
  asm volatile("vfmv.f.s %0, v0" : "=f"(red));
  return red;
}

static SPMV_ALWAYS_INLINE double spmv_row_scalar(const double *val,
                                                 const uint32_t *x_off,
                                                 const double *x,
                                                 uint32_t start,
                                                 uint32_t end) {
  double sum = 0.0;
  for (uint32_t k = start; k < end; ++k) {
    sum += val[k] * x[x_off[k] / sizeof(double)];
  }
  return sum;
}

SPMV_NOINLINE void spmv_v64b_m1(const uint32_t *row_ptr, const uint32_t *x_off,
                                const double *val, const double *x, double *y,
                                uint32_t row_start, uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;

    if (nnz < SPMV_SMALL_ROW_THRESHOLD) {
      y[row] = spmv_row_scalar(val, x_off, x, start, end);
    } else {
      y[row] = spmv_row_v64b_m1(val + start, x_off + start, x, nnz);
    }
  }
}

SPMV_NOINLINE void spmv_v64b_m2(const uint32_t *row_ptr, const uint32_t *x_off,
                                const double *val, const double *x, double *y,
                                uint32_t row_start, uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;

    if (nnz < SPMV_SMALL_ROW_THRESHOLD) {
      y[row] = spmv_row_scalar(val, x_off, x, start, end);
    } else {
      y[row] = spmv_row_v64b_m2(val + start, x_off + start, x, nnz);
    }
  }
}

#define SPMV_UNROLL 1

SPMV_NOINLINE void spmv_v64b_m4(const uint32_t *row_ptr, const uint32_t *x_off,
                                const double *val, const double *x, double *y,
                                uint32_t row_start, uint32_t row_end) {
#if SPMV_UNROLL
  spmv_row_v64b_m4_unrolled(row_ptr, x_off, val, x, y, row_start, row_end);
#else
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;

    if (nnz < SPMV_SMALL_ROW_THRESHOLD) {
      y[row] = spmv_row_scalar(val, x_off, x, start, end);
    } else {
      y[row] = spmv_row_v64b_m4(val + start, x_off + start, x, nnz);
    }
  }
#endif
}

SPMV_NOINLINE void spmv_v64b_m8(const uint32_t *row_ptr, const uint32_t *x_off,
                                const double *val, const double *x, double *y,
                                uint32_t row_start, uint32_t row_end) {
  for (uint32_t row = row_start; row < row_end; ++row) {
    const uint32_t start = row_ptr[row];
    const uint32_t end = row_ptr[row + 1];
    const uint32_t nnz = end - start;

    if (nnz < SPMV_SMALL_ROW_THRESHOLD) {
      y[row] = spmv_row_scalar(val, x_off, x, start, end);
    } else {
      y[row] = spmv_row_v64b_m8(val + start, x_off + start, x, nnz);
    }
  }
}

SPMV_NOINLINE void spmv_v64b(const uint32_t *row_ptr, const uint32_t *x_off,
                             const double *val, const double *x, double *y,
                             uint32_t row_start, uint32_t row_end) {
#if (SPMV_LMUL == 1)
  spmv_v64b_m1(row_ptr, x_off, val, x, y, row_start, row_end);
#elif (SPMV_LMUL == 2)
  spmv_v64b_m2(row_ptr, x_off, val, x, y, row_start, row_end);
#elif (SPMV_LMUL == 4)
  spmv_v64b_m4(row_ptr, x_off, val, x, y, row_start, row_end);
#elif (SPMV_LMUL == 8)
  spmv_v64b_m8(row_ptr, x_off, val, x, y, row_start, row_end);
#else
#error "SPMV_LMUL must be one of 1, 2, 4, 8"
#endif
}
