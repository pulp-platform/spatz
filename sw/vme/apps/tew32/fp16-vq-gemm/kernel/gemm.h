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
    TE = 16,
    CHUNK_SIZE = 4 * TE,
    BLOCK_DIM = 2 * TE,
};

// Pre-expanded metadata for one 16-column tile and eight K values.
// Each array is loaded as one e16,m4 vector (128 elements).
typedef struct {
    uint16_t off0[128];
    uint16_t off1[128];
    uint16_t scale[128];
} vq_group;
_Static_assert(sizeof(vq_group) == 768, "VQ metadata layout changed");

void gemm_fp16_fp32_vq_1x4(const __fp16 *Atpack, const vq_group *Bmeta,
                           const __fp16 *cb0, const __fp16 *cb1, float *C);
