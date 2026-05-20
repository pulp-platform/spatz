// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#ifndef SP_SPMM_H
#define SP_SPMM_H

#include <stdint.h>

// Two implementations of structured-sparse SpMM (n:m, fp32):
//
//   res[M x P]  =  A[M x N]  *  W[N x P]      (W is n:m structured sparse)
//
// `spmm_ventaglio`: uses Ventaglio's vfx instructions (vfxmul.vrf,
// vfxmacc.vrf, vlx32, vventclr). Software-pipelined inner loop with
// double-buffered weights/indices, accumulators v16/v18 (unrolled by 2
// over M).
//
// `spmm_baseline`: standard RVV gather-modify-scatter via vluxei32/vfmacc/
// vsuxei32, with software N:M index unpacking into a `byte_offsets[P_W]`
// scratch buffer supplied by the caller. M loop also unrolled by 2.

void spmm_ventaglio(float *res,
                    const float *a,
                    const float *w,
                    const uint32_t *nm_index,
                    uint32_t M,
                    uint32_t N,
                    uint32_t P,
                    uint32_t P_W,
                    uint32_t NM_INDEX_ROW_WORDS,
                    uint32_t idx_width,
                    uint32_t m_sparse,
                    uint32_t n_sparse);

void spmm_baseline(float          *res,
                   const float    *a,
                   const float    *w,
                   const uint32_t *nm_index,
                   uint32_t       *byte_offsets,
                   uint32_t M,
                   uint32_t N,
                   uint32_t P,
                   uint32_t P_W,
                   uint32_t NM_INDEX_ROW_WORDS,
                   uint32_t idx_width,
                   uint32_t m_sparse,
                   uint32_t n_sparse);

#endif // SP_SPMM_H
