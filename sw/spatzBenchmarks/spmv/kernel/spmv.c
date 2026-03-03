// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include "spmv.h"

#define SPMV_SMALL_ROW_THRESHOLD 4

// x_off contains 32-bit byte offsets into x. On this rv32 target, vluxei64 is
// not available, so gather indices stay 32-bit even for fp64 data.
#define DEFINE_SPMV_ROW_FN(FN, LMUL, VIDX, VVAL, VX, VACC)                     \
  static inline double FN(const double *val, const uint32_t *x_off,            \
                          const double *x, uint32_t avl) {                      \
    if (avl == 0) return 0.0;                                                   \
    const uint32_t orig_avl = avl;                                              \
    uint32_t vl;                                                                 \
    double red = 0.0;                                                            \
    asm volatile("vsetvli %0, %1, e64, " LMUL ", tu, ma"                        \
                 : "=r"(vl)                                                      \
                 : "r"(avl));                                                    \
    asm volatile("vmv.s.x v0, zero");                                           \
    do {                                                                         \
      asm volatile("vsetvli %0, %1, e64, " LMUL ", tu, ma"                      \
                   : "=r"(vl)                                                    \
                   : "r"(avl));                                                  \
      asm volatile("vle64.v " VVAL ", (%0)" : : "r"(val));                      \
      asm volatile("vle32.v " VIDX ", (%0)" : : "r"(x_off));                    \
      asm volatile("vluxei32.v " VX ", (%0), " VIDX : : "r"(x));                \
      if (avl == orig_avl) {                                                     \
        asm volatile("vfmul.vv " VACC ", " VVAL ", " VX);                       \
      } else {                                                                   \
        asm volatile("vfmacc.vv " VACC ", " VVAL ", " VX);                      \
      }                                                                          \
      val += vl;                                                                 \
      x_off += vl;                                                               \
      avl -= vl;                                                                 \
    } while (avl > 0);                                                           \
    asm volatile("vsetvli zero, %0, e64, " LMUL ", tu, ma" : : "r"(orig_avl)); \
    asm volatile("vfredusum.vs v0, " VACC ", v0");                              \
    asm volatile("vfmv.f.s %0, v0" : "=f"(red));                                \
    return red;                                                                  \
  }

DEFINE_SPMV_ROW_FN(spmv_row_v64b_m1, "m1", "v16", "v8", "v24", "v28")
DEFINE_SPMV_ROW_FN(spmv_row_v64b_m2, "m2", "v4", "v8", "v10", "v12")
DEFINE_SPMV_ROW_FN(spmv_row_v64b_m4, "m4", "v4", "v8", "v16", "v24")
DEFINE_SPMV_ROW_FN(spmv_row_v64b_m8, "m8", "v4", "v8", "v16", "v24")

#define DEFINE_SPMV_KERNEL(FN, ROW_FN)                                          \
  void FN(const uint32_t *row_ptr, const uint32_t *x_off, const double *val,    \
          const double *x, double *y, uint32_t row_start, uint32_t row_end) {   \
    for (uint32_t row = row_start; row < row_end; ++row) {                       \
      const uint32_t start = row_ptr[row];                                       \
      const uint32_t end = row_ptr[row + 1];                                     \
      const uint32_t nnz = end - start;                                          \
      if (nnz < SPMV_SMALL_ROW_THRESHOLD) {                                      \
        double sum = 0.0;                                                        \
        for (uint32_t k = start; k < end; ++k) {                                 \
          sum += val[k] * x[x_off[k] / sizeof(double)];                          \
        }                                                                        \
        y[row] = sum;                                                            \
      } else {                                                                   \
        y[row] = ROW_FN(val + start, x_off + start, x, nnz);                    \
      }                                                                          \
    }                                                                            \
  }

DEFINE_SPMV_KERNEL(spmv_v64b_m1, spmv_row_v64b_m1)
DEFINE_SPMV_KERNEL(spmv_v64b_m2, spmv_row_v64b_m2)
DEFINE_SPMV_KERNEL(spmv_v64b_m4, spmv_row_v64b_m4)
DEFINE_SPMV_KERNEL(spmv_v64b_m8, spmv_row_v64b_m8)

void spmv_v64b(const uint32_t *row_ptr, const uint32_t *x_off, const double *val,
               const double *x, double *y, uint32_t row_start,
               uint32_t row_end) {
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
