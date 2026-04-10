// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef _SPMV_SPARSE_H
#define _SPMV_SPARSE_H

#include <stdint.h>

#ifndef SPMV_LMUL
#define SPMV_LMUL 4
#endif

void spmv_sparse_v64b_m1(const uint32_t *row_ptr, const uint32_t *x_off,
                         const double *dense, uint32_t dense_ld,
                         const double *x, double *y, uint32_t row_start,
                         uint32_t row_end);
void spmv_sparse_v64b_m2(const uint32_t *row_ptr, const uint32_t *x_off,
                         const double *dense, uint32_t dense_ld,
                         const double *x, double *y, uint32_t row_start,
                         uint32_t row_end);
void spmv_sparse_v64b_m4(const uint32_t *row_ptr, const uint32_t *x_off,
                         const double *dense, uint32_t dense_ld,
                         const double *x, double *y, uint32_t row_start,
                         uint32_t row_end);
void spmv_sparse_v64b_m8(const uint32_t *row_ptr, const uint32_t *x_off,
                         const double *dense, uint32_t dense_ld,
                         const double *x, double *y, uint32_t row_start,
                         uint32_t row_end);

void spmv_sparse_v64b(const uint32_t *row_ptr, const uint32_t *x_off,
                      const double *dense, uint32_t dense_ld, const double *x,
                      double *y, uint32_t row_start, uint32_t row_end);

#endif
