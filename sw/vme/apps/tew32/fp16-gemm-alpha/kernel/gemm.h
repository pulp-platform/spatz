// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

enum {
    CE = 8,
    TE = 16,
    CHUNK_SIZE = 4 * TE,
    BLOCK_DIM = 2 * TE,
};

void gemm_fp16_fp32_alpha_vle(const __fp16 *Atpack, const __fp16 *Bpack,
                              float *C, const float *alpha);
