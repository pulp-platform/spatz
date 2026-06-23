// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#pragma once

#include <stdint.h>

/**
 * @struct spmm_layer_struct
 * @brief Parameters for an n:m structured-sparse SpMM.
 *
 *   res[M x P]  =  A[M x N]  *  W[N x P]      (W is n:m structured sparse)
 *
 *   M  : batch / output row count
 *   N  : reduction dim
 *   P  : dense output column count
 *   P_W: compact column count per sparse row of W  ( = (P / m) * n )
 *
 * Sparsity is (n, m) where each block of m dense positions has n nonzeros.
 *   N_SPARSE = n, M_SPARSE = m.
 */
typedef struct spmm_layer_struct {
  uint32_t M;                  // batch dim (#rows of A and res)
  uint32_t N;                  // reduction dim
  uint32_t P;                  // dense output column count
  uint32_t P_W;                // compact column count per sparse row of W
  uint32_t N_SPARSE;           // n (nonzeros per block)
  uint32_t M_SPARSE;           // m (block width)
  uint32_t IDX_WIDTH;          // bits per packed index
  uint32_t NM_INDEX_ROW_WORDS; // u32 words to pack one row of indices
  uint32_t NM_INDEX_WORDS;     // total u32 words for all rows of indices
} spmm_layer;
