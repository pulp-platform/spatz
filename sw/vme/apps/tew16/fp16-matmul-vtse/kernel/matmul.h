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
// Author: Pei-Yu Lin <peilin@ethz.ch>

#pragma once

#include <stdint.h>

enum {
    CE = 8,
    TE = 16,
    CHUNK_SIZE = 64,
    BLOCK_DIM = 2 * TE,
};

// Read the hardware cycle counter.
static inline uint32_t get_cycle(void)
{
    uint32_t c;
    asm volatile("csrr %0, mcycle" : "=r"(c));
    return c;
}

// Reading fcsr stalls Snitch while any Spatz scoreboard entry is live. Use it
// to turn an asynchronous kernel return into an architectural completion point.
static inline void wait_spatz(void)
{
    uint32_t fcsr;
    asm volatile("csrr %0, fcsr" : "=r"(fcsr) :: "memory");
}

// FP16 VME matmul over TE=16 with architectural tk=2. C is a padded
// M-by-CHUNK_SIZE L1 panel; M and N must not exceed 64.
void matmul_fp16(const __fp16 *Apack, const __fp16 *Bpack, __fp16 *C,
                 uint32_t M, uint32_t N, uint32_t K);
