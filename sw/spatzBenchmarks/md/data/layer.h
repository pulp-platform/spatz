// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

typedef enum { FP64 = 8, FP32 = 4, FP16 = 2, FP8 = 1 } precision_t;

// M: number of particles. K: total neighbor-list entries (CSR-style,
// sum of per-particle neighbor counts). Non-Newton pairwise force: each
// particle independently loops its own full neighbor list (no
// scatter-accumulate to the neighbor), so K counts each interacting pair
// twice (once from each side).
typedef struct md_layer_struct {
  uint32_t M;
  uint32_t K;
  precision_t dtype;
} md_layer;
