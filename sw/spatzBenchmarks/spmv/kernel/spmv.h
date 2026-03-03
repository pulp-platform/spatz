// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef _SPMV_H
#define _SPMV_H

#include <stdint.h>

void spmv_v64b(const uint32_t *row_ptr, const uint32_t *x_off, const double *val,
               const double *x, double *y, uint32_t row_start,
               uint32_t row_end);

#endif
