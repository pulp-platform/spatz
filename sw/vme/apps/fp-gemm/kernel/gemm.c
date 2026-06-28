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

// gemm.c — fp32×fp32=fp32 VME outer-product GEMM kernel.
//
// A is stored row-major: A[i][k] = A[i*K + k]
// B is stored row-major: B[k][j] = B[k*N + j]
// C is stored row-major: C[i][j] = C[i*N + j], fp32 accumulator

#include "gemm.h"
#include "vector_macros.h"
#include "vector_matrix_macros.h"

#define STORE_4TILES(c00, c01, c10, c11) do {                               \
    for (int _r = 0; _r < (int)TM; _r++) {                                  \
        uintptr_t _t0  = TSS_ROW( 0, _r), _t4  = TSS_ROW( 4, _r);          \
        uintptr_t _t8  = TSS_ROW( 8, _r), _t12 = TSS_ROW(12, _r);          \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t0),  "r"((c00)+_r*(int)N)  : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t4),  "r"((c01)+_r*(int)N)  : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t8),  "r"((c10)+_r*(int)N)  : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t12), "r"((c11)+_r*(int)N)  : "memory"); \
    }                                                                         \
} while (0)

// fp32×fp32=fp32 VME GEMM.
// Requires: M % (2*TM) == 0, N % (2*TN) == 0, K >= 1.
void gemm_fp32(float *C, const float *A, const float *B,
               uint32_t M, uint32_t N, uint32_t K,
               uint32_t TM, uint32_t TN)
{
    const uintptr_t TK = 1;
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"((uintptr_t)TM) : "memory");
    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"((uintptr_t)TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"((uintptr_t)TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");  // clear alt_fmt (bit 8 in vtype)

    // Stride between successive rows of A in bytes: K fp32 elements.
    const uintptr_t a_stride = (uintptr_t)(K * sizeof(float));

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            // Zero the four output tile accumulators.
            asm volatile("vtzero mt0"  ::: "memory");
            asm volatile("vtzero mt4"  ::: "memory");
            asm volatile("vtzero mt8"  ::: "memory");
            asm volatile("vtzero mt12" ::: "memory");

            for (int k = 0; k < (int)K; k++) {
                // Pointers to column k of each A tile and row k of each B tile.
                const float *a0 = A + ti       * (int)TM * (int)K + k;
                const float *a1 = A + (ti + 1) * (int)TM * (int)K + k;
                const float *b0 = B + k * (int)N +  tj      * (int)TN;
                const float *b1 = B + k * (int)N + (tj + 1) * (int)TN;

                // Load TM-element column vectors from A (stride = one row).
                asm volatile("vlse32.v v8, (%0), %1" :: "r"(a0), "r"(a_stride) : "v8", "memory");
                asm volatile("vlse32.v v9, (%0), %1" :: "r"(a1), "r"(a_stride) : "v9", "memory");

                // Load TN-element row vectors from B (contiguous).
                asm volatile("vle32.v v16, (%0)" :: "r"(b0) : "v16", "memory");
                asm volatile("vle32.v v17, (%0)" :: "r"(b1) : "v17", "memory");

                // Four outer products: C[ti*2 tiles, tj*2 tiles].
                asm volatile("vtfmm.tvv mt0,  v8, v16" ::: "memory");
                asm volatile("vtfmm.tvv mt4,  v8, v17" ::: "memory");
                asm volatile("vtfmm.tvv mt8,  v9, v16" ::: "memory");
                asm volatile("vtfmm.tvv mt12, v9, v17" ::: "memory");
            }

            // Write the four tiles back to C (row-major).
            STORE_4TILES(C +  ti      * (int)TM * (int)N +  tj      * (int)TN,
                         C +  ti      * (int)TM * (int)N + (tj + 1) * (int)TN,
                         C + (ti + 1) * (int)TM * (int)N +  tj      * (int)TN,
                         C + (ti + 1) * (int)TM * (int)N + (tj + 1) * (int)TN);
        }
    }

    asm volatile("vtdiscard" ::: "memory");
}

#undef STORE_4TILES
