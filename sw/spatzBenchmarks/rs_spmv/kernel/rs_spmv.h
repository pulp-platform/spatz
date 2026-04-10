// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef _RS_SPMV_H
#define _RS_SPMV_H

#include <stdint.h>

void rs_spmv_v64b(const uint32_t *nz_row_idx, const double *dense_a,
                  uint32_t a_ld, const double *x, double *y, uint32_t start,
                  uint32_t end);

#endif
