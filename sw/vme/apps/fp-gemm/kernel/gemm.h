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

// gemm.h — interface for the fp32×fp32=fp32 VME GEMM kernel.

#pragma once

#include <stdint.h>

// Read the hardware cycle counter.
static inline uint32_t get_cycle(void)
{
    uint32_t c;
    asm volatile("csrr %0, mcycle" : "=r"(c));
    return c;
}

// fp32×fp32=fp32 VME GEMM.
// A : row-major [M×K], A[i*K+k]
// B : row-major [K×N], B[k*N+j]
// C : row-major [M×N] fp32 accumulator
// Requires: M%(2*TM)==0, N%(2*TN)==0
void gemm_fp32(float *C, const float *A, const float *B,
               uint32_t M, uint32_t N, uint32_t K,
               uint32_t TM, uint32_t TN);
