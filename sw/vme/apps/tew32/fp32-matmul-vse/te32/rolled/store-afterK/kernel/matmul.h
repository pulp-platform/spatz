// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Pei-Yu Lin, ETH Zürich

#pragma once

#include <stdint.h>

enum {
    CE = 8,
    TE = 32,
    CHUNK_SIZE = 4 * TE,
    BLOCK_DIM = 2 * TE,
    ROW_BLOCKS = (M_CHUNK + BLOCK_DIM - 1) / BLOCK_DIM,
    COL_BLOCKS = (N_CHUNK + BLOCK_DIM - 1) / BLOCK_DIM,
    TILE_STRIDE = TE * K_CHUNK,
};

enum {
    TCDM_BYTES = 512 * 1024,
    TILE_INPUTS = TE * K_CHUNK,
    TILE_BYTES = TILE_INPUTS * sizeof(float),
    A_TILES = M_CHUNK / TE,
    B_TILES = N_CHUNK / TE,
    A_BYTES = A_TILES * TILE_BYTES,
    B_BYTES = B_TILES * TILE_BYTES,
    C_ELEMENTS = M_CHUNK * N_CHUNK,
    C_BYTES = C_ELEMENTS * sizeof(float),
    A_OFFSET = 0,
    B_OFFSET = A_OFFSET + A_BYTES,
    C_OFFSET = B_OFFSET + B_BYTES,
    BUFFER_BYTES = C_OFFSET + C_BYTES,
    L1_BUFFER_COUNT = MATMUL_BUFFER_COUNT,
    L1_BUFFER_STRIDE = TCDM_BYTES / L1_BUFFER_COUNT,
    L1_BYTES = (L1_BUFFER_COUNT - 1) * L1_BUFFER_STRIDE + BUFFER_BYTES,
};

void matmul_fp32(const float *At, const float *B, float *C);
