// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include "md.h"

#define MD_SMALL_ROW_THRESHOLD 4
#define MD_NOINLINE __attribute__((noinline))
#define MD_ALWAYS_INLINE __attribute__((always_inline)) inline

// Gathers neighbor positions (x_off contains 32-bit byte offsets into
// pos_x/y/z), computes the division-free pairwise force
// f = K*(RCUT2-r2)^2 from particle i onto each neighbor, and reduces to a
// net (fx, fy, fz) for i. Non-Newton: this only ever writes force[i] -- no
// scatter-accumulate to the neighbor, so no cross-lane write hazard.
static MD_ALWAYS_INLINE void md_row_v64b_m1(
    const uint32_t *x_off, const double *pos_x, const double *pos_y,
    const double *pos_z, double xi, double yi, double zi, uint32_t avl,
    double *fx_o, double *fy_o, double *fz_o) {
  // Every other kernel in this codebase issues exactly one vfredusum per
  // loop iteration; three back-to-back reductions (needed here for
  // fx/fy/fz) deadlocked the pipeline regardless of register choice. Avoid
  // the repeated-reduction pattern entirely: store the per-lane force
  // vectors and sum them with a plain scalar loop instead.
  double fx = 0.0, fy = 0.0, fz = 0.0;
  double buf_x[16], buf_y[16], buf_z[16];
  uint32_t vl;
  do {
    asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl));
    // Gather neighbor offsets, then x/y/z positions.
    asm volatile("vle32.v v1, (%0)" : : "r"(x_off));
    asm volatile("vluxei32.v v2, (%0), v1" : : "r"(pos_x));
    asm volatile("vluxei32.v v3, (%0), v1" : : "r"(pos_y));
    asm volatile("vluxei32.v v4, (%0), v1" : : "r"(pos_z));
    // dx, dy, dz = pos_j - pos_i.
    asm volatile("vfsub.vf v5, v2, %0" : : "f"(xi));
    asm volatile("vfsub.vf v6, v3, %0" : : "f"(yi));
    asm volatile("vfsub.vf v7, v4, %0" : : "f"(zi));
    // r2 = dx^2 + dy^2 + dz^2.
    asm volatile("vfmul.vv v8, v5, v5");
    asm volatile("vfmacc.vv v8, v6, v6");
    asm volatile("vfmacc.vv v8, v7, v7");
    // fpair_factor = K * (RCUT2 - r2)^2  (division-free, unconditional).
    asm volatile("vfrsub.vf v9, v8, %0" : : "f"((double)MD_FORCE_RCUT2));
    asm volatile("vfmul.vv v9, v9, v9");
    asm volatile("vfmul.vf v13, v9, %0" : : "f"((double)MD_FORCE_K));
    // f_j = fpair_factor * d_j, accumulated (reused into v5/v6/v7).
    asm volatile("vfmul.vv v5, v5, v13");
    asm volatile("vfmul.vv v6, v6, v13");
    asm volatile("vfmul.vv v7, v7, v13");

    asm volatile("vse64.v v5, (%0)" : : "r"(buf_x));
    asm volatile("vse64.v v6, (%0)" : : "r"(buf_y));
    asm volatile("vse64.v v7, (%0)" : : "r"(buf_z));
    for (uint32_t e = 0; e < vl; ++e) {
      fx += buf_x[e]; fy += buf_y[e]; fz += buf_z[e];
    }

    x_off += vl;
    avl -= vl;
  } while (avl > 0);

  *fx_o = fx; *fy_o = fy; *fz_o = fz;
}

#define DEFINE_MD_KERNEL(FN, ROW_FN)                                          \
  MD_NOINLINE void FN(const uint32_t *neigh_ptr, const uint32_t *neigh_idx,    \
                      const uint32_t *x_off, const double *pos_x,              \
                      const double *pos_y, const double *pos_z,                \
                      double *force_x, double *force_y, double *force_z,       \
                      uint32_t row_start, uint32_t row_end) {                  \
    for (uint32_t i = row_start; i < row_end; ++i) {                           \
      const uint32_t start = neigh_ptr[i];                                     \
      const uint32_t end = neigh_ptr[i + 1];                                   \
      const uint32_t nnz = end - start;                                        \
      const double xi = pos_x[i], yi = pos_y[i], zi = pos_z[i];                \
      double fx, fy, fz;                                                       \
      if (nnz == 0) {                                                          \
        fx = fy = fz = 0.0;                                                    \
      } else if (nnz < MD_SMALL_ROW_THRESHOLD) {                               \
        fx = fy = fz = 0.0;                                                    \
        for (uint32_t k = start; k < end; ++k) {                               \
          const uint32_t j = neigh_idx[k];                                     \
          const double dx = pos_x[j] - xi, dy = pos_y[j] - yi,                 \
                       dz = pos_z[j] - zi;                                     \
          const double r2 = dx * dx + dy * dy + dz * dz;                       \
          const double diff = MD_FORCE_RCUT2 - r2;                             \
          const double fpair = MD_FORCE_K * diff * diff;                       \
          fx += fpair * dx; fy += fpair * dy; fz += fpair * dz;                \
        }                                                                      \
      } else {                                                                 \
        ROW_FN(x_off + start, pos_x, pos_y, pos_z, xi, yi, zi, nnz, &fx, &fy,   \
              &fz);                                                            \
      }                                                                        \
      force_x[i] = fx; force_y[i] = fy; force_z[i] = fz;                       \
    }                                                                          \
  }

DEFINE_MD_KERNEL(md_lj_v64b_m1, md_row_v64b_m1)

void md_lj_v64b(const uint32_t *neigh_ptr, const uint32_t *neigh_idx,
               const uint32_t *x_off, const double *pos_x,
               const double *pos_y, const double *pos_z, double *force_x,
               double *force_y, double *force_z, uint32_t row_start,
               uint32_t row_end) {
  md_lj_v64b_m1(neigh_ptr, neigh_idx, x_off, pos_x, pos_y, pos_z, force_x,
               force_y, force_z, row_start, row_end);
}
