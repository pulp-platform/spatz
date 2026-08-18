// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang, ETH Zurich

#pragma once

#include <stdint.h>

typedef struct dictdecode_layer_struct {
  // Number of codes in the compressed column.
  uint32_t N_CODES;
  // Number of dictionary entries.
  uint32_t K;
  // Elements (fp32) per dictionary entry: one fixed-width record slot.
  uint32_t D;
  // Bytes per code (1 -> uint8 codes, 2 -> uint16 codes).
  uint32_t CODE_BYTES;
} dictdecode_layer;
