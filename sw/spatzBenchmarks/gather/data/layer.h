// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#pragma once

#include <stdint.h>

typedef enum { FP64 = 8, FP32 = 4, FP16 = 2, FP8 = 1 } precision_t;

/**
 * @struct gather_layer_struct
 * @brief Parameters for an indexed (gather) load of matrix rows.
 *
 *  The DRAM matrix has ROWS entries, each a DIM-element vector (e.g. a KV
 *  cache: ROWS tokens x DIM channels). At runtime an index stream of G entries
 *  selects G rows to gather into on-chip L1.
 */
typedef struct gather_layer_struct {
  uint32_t ROWS;     // number of matrix entries in DRAM
  uint32_t DIM;      // elements per entry
  uint32_t G;        // number of gathered indices
  precision_t dtype; // element precision (FP16 here)
} gather_layer;
