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

// Author: Bowen Wang, ETH Zurich

#pragma once
#include <stdint.h>

#ifndef HPDQMATMULBLK8EI16_H
#define HPDQMATMULBLK8EI16_H

// Case 2: CB_D = 8, EI16 indices, two codebooks with additive reconstruction.
inline void dq_matmul_4xVL_blk8ei16(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0, const __fp16 *b_cb1,
                        const uint16_t *b_idx0, const uint16_t *b_idx1,
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

inline void dq_matmul_4xVL_dp_blk8ei16(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0, const __fp16 *b_cb1,
                        const uint16_t *b_idx0, const uint16_t *b_idx1,
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

#endif
