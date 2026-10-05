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

// gemm.h — interface for the fp32×fp32=fp32 VME GEMM kernel.

#include <stdint.h>

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

// FP32 VME GEMM over TE=16 with architectural tk=1 and RES_K=8 software
// residency. [ti_lo, ti_hi) selects an even-aligned range of 16-row tiles.
void gemm_fp32(float *C, const float *Apack, const float *Bpack,
               uint32_t ti_lo, uint32_t ti_hi);
