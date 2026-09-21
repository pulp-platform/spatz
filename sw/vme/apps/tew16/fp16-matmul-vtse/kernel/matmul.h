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
    M_BLOCK = 4 * TE,
    N_BLOCK = 2 * TE,
    A_TILES_PER_BLOCK = M_BLOCK / TE,
    B_TILES_PER_BLOCK = N_BLOCK / TE,
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

// FP16 VME matmul over TE=16. One logical block is 4*TE by 2*TE:
//
//   mt0/mt4/mt8/mt12  = A0/A1/A2/A3 * B0
//   mt2/mt6/mt10/mt14 = A0/A1/A2/A3 * B1
//
// The current functionality path evaluates B0 and B1 in two passes using
// mt0/mt4/mt8/mt12. C is a padded M-by-CHUNK_SIZE L1 panel; M and N must not
// exceed 64.
void matmul_fp16(const __fp16 *Apack, const __fp16 *Bpack, __fp16 *C,
                 uint32_t M, uint32_t N, uint32_t K);
