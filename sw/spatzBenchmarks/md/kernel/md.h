// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef _MD_H
#define _MD_H

#include <stdint.h>

#ifndef MD_LMUL
#define MD_LMUL 1
#endif

// Division-free, MD-flavored pairwise repulsive force: f = K*(RCUT2-r2)^2,
// applied to every listed neighbor unconditionally (no cutoff mask/branch --
// the neighbor list itself is the pre-filtered working set). Not literal
// Lennard-Jones (this hardware has no float div/sqrt support), but keeps
// the same gather + per-neighbor FMA-heavy compute signature.
#define MD_FORCE_RCUT2 200.0
#define MD_FORCE_K 0.0001

// neigh_idx: raw neighbor particle indices (CSR-style, sliced by neigh_ptr).
// x_off: neigh_idx[k] * sizeof(double), precomputed once by the caller --
// same split as SpMV's col_idx/x_off, needed because the vector gather path
// wants byte offsets while the scalar fallback wants raw indices.
void md_lj_v64b_m1(const uint32_t *neigh_ptr, const uint32_t *neigh_idx,
                   const uint32_t *x_off, const double *pos_x,
                   const double *pos_y, const double *pos_z, double *force_x,
                   double *force_y, double *force_z, uint32_t row_start,
                   uint32_t row_end);

void md_lj_v64b(const uint32_t *neigh_ptr, const uint32_t *neigh_idx,
               const uint32_t *x_off, const double *pos_x,
               const double *pos_y, const double *pos_z, double *force_x,
               double *force_y, double *force_z, uint32_t row_start,
               uint32_t row_end);

#endif
