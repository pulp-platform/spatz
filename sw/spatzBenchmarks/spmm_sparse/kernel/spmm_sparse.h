// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef _SPMM_SPARSE_H
#define _SPMM_SPARSE_H

#include <stdint.h>

void spmm_sparse_v64b(const uint32_t *row_ptr, const uint32_t *col_idx,
                      uint32_t nnz_base, const double *dense_a,
                      uint32_t dense_ld, uint32_t row_base,
                      const double *b, uint32_t b_ld, double *c,
                      uint32_t c_ld, uint32_t row_start, uint32_t row_end);

#endif
