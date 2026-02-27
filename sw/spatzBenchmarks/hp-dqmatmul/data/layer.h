// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

typedef enum { FP64  = 8, FP32  = 4, FP16  = 2, FP8  = 1 } ele_precision_t;
typedef enum { INT64 = 8, INT32 = 4, INT16 = 2, INT8 = 1 } idx_precision_t;

/**
 * @struct gemm_layer_struct
 * @brief This structure contains all parameters necessary for GEMM.
 * @var gemm_layer_struct::M
 * Dimension of matrix product MxK * KxN
 * @var gemm_layer_struct::M_p
 * M divided by number of compute cores
 * @var gemm_layer_struct::N
 * Dimension of matrix product MxK * KxN
 * @var gemm_layer_struct::K
 * Dimension of matrix product MxK * KxN
 * @var gemm_layer_struct::TA
 * Transpose matrix A
 * @var gemm_layer_struct::TB
 * Transpose matrix B
 * @var gemm_layer_struct::TILE_M
 * Tile factor across M dimension
 * @var gemm_layer_struct::TILE_N
 * Tile factor across N dimension
 * @var gemm_layer_struct::TILE_K
 * Tile factor across K dimension
 * @var gemm_layer_struct::A
 * Pointer to matrix A
 * @var gemm_layer_struct::B
 * Pointer to matrix B
 * @var gemm_layer_struct::C
 * Pointer to matrix C
 * @var gemm_layer_struct::ALPHA
 * constant factor: A * B + ALPHA * C
 * @var gemm_layer_struct::dtype
 * Precision of GEMM
 * @var gemm_layer_struct::expand
 * Use expanding DOTP instructions
 */
typedef struct dq_gemm_layer_struct {
  uint32_t M;
  uint32_t M_p;
  uint32_t N;
  uint32_t K;

  uint32_t TA;
  uint32_t TB;

  uint32_t TILE_M;
  uint32_t TILE_N;
  uint32_t TILE_K;

  __fp16 *A;
  __fp16 *C;

  uint32_t ALPHA;

  ele_precision_t dtype;
  idx_precision_t itype;
  uint32_t expand;

  // codebook and index
  uint32_t CB0_N;  // number of centroids
  uint32_t CB0_D;  // centroid dimension
  uint32_t CB1_N;  // number of centroids
  uint32_t CB1_D;  // centroid dimension

  // This kernel expects 8-bit indices (EI8) and CB_D = 8
  uint8_t  CB0_IDX_WIDTH; // in byte
  uint8_t  CB1_IDX_WIDTH;
} dq_gemm_layer;
