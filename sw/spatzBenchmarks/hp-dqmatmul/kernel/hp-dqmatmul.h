// Copyright 2026 ETH Zurich and University of Bologna.
//
// HPDX-License-Identifier: Apache-2.0
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
// See the License for the hpecific language governing permissions and
// limitations under the License.

// Author: Bowen Wang, ETH Zurich

#pragma once
#include <stdint.h>

#ifndef HPDQMATMUL_H
#define HPDQMATMUL_H
inline void dq_matmul_2xVL(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0, const __fp16 *b_cb1,
                        const uint8_t *b_idx0, const uint8_t *b_idx1, 
                        const unsigned int m_start, const unsigned int m_end,
                        const unsigned int N, const unsigned int P,
                        const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

#endif
