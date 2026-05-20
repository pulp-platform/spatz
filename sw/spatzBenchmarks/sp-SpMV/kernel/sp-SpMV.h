// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#ifndef SP_SPMV_H
#define SP_SPMV_H

#include <stdint.h>

// Two implementations of structured-sparse SpMV (n:m, fp32):
//
//   res[P]  =  W[N x P]^T * a[N]      (W is n:m structured sparse, compact P_W)
//
// `spmv_ventaglio`: uses Ventaglio's vfx instructions (vfxmul.vrf,
// vfxmacc.vrf, vlx32, vventclr) — in-bank accumulation, no L1 round-trip.
//
// `spmv_baseline`: standard RVV gather-modify-scatter via vluxei32/vfmacc/
// vsuxei32, with software N:M index unpacking into a `byte_offsets[P_W]`
// scratch buffer supplied by the caller. Reference for A/B comparison.

void spmv_ventaglio(float          *res,
                    const float    *a,
                    const float    *w,
                    const uint32_t *nm_index,
                    uint32_t N,
                    uint32_t P_W,
                    uint32_t NM_INDEX_ROW_WORDS,
                    uint32_t idx_width,
                    uint32_t m_sparse,
                    uint32_t n_sparse);

void spmv_baseline(float          *res,
                   const float    *a,
                   const float    *w,
                   const uint32_t *nm_index,
                   uint32_t       *byte_offsets,
                   uint32_t N,
                   uint32_t P_W,
                   uint32_t NM_INDEX_ROW_WORDS,
                   uint32_t idx_width,
                   uint32_t m_sparse,
                   uint32_t n_sparse);

#endif // SP_SPMV_H
