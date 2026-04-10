// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef _RS_SPMM_H
#define _RS_SPMM_H

#include <stdint.h>

void rs_spmm_v64b(const uint32_t *nz_row_idx, const double *dense_a,
                   uint32_t a_ld, const double *b, uint32_t b_ld, double *c,
                   uint32_t c_ld, uint32_t start, uint32_t end);

#endif
