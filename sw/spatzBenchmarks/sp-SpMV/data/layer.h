// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#pragma once

#include <stdint.h>

/**
 * @struct spmv_layer_struct
 * @brief Parameters for an n:m structured-sparse SpMV.
 *
 *  Dense output dimension : P
 *  Reduction dimension    : N (one fp32 activation per row of W)
 *  Compact column count   : P_W = (P / m) * n   (per sparse row of W)
 *
 * Sparsity is described by the (n, m) pair where each block of m
 * dense positions contains n nonzeros.  n = N_SPARSE, m = M_SPARSE.
 */
typedef struct spmv_layer_struct {
  uint32_t N;                  // reduction dim (# activation entries)
  uint32_t P;                  // dense output dim
  uint32_t P_W;                // compact column count per sparse row of W
  uint32_t N_SPARSE;           // n (nonzeros per block)
  uint32_t M_SPARSE;           // m (block width)
  uint32_t IDX_WIDTH;          // bits per packed index
  uint32_t NM_INDEX_ROW_WORDS; // u32 words to pack one row of indices
  uint32_t NM_INDEX_WORDS;     // total u32 words for all rows of indices
} spmv_layer;
