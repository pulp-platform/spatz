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

#include <snrt.h>
#include "runtime.h"
#include <printf.h>

#ifndef APP_FUSED
#define APP_FUSED 1
#endif

#include "gemm.h"
#include DATAHEADER
#include "kernel/gemm.c"

enum { M_DIM = COUT_SIZE, N_DIM = T_SIZE, K_DIM = C_SIZE };

static inline uint32_t float_bits(float value) {
    union { float f; uint32_t u; } bits = {.f = value};
    return bits.u;
}

int main(void) {
    if (snrt_cluster_core_idx() != 0)
        return 0;

    if (M_DIM != 192 || N_DIM != 64 || K_DIM != 128) {
        printf("Unsupported P2 shape: expected M192 N64 K128\n");
        return 2;
    }
    enum {
        A_BYTES = sizeof(d_w),
        B_BYTES = sizeof(d_xpk),
        C_BYTES = sizeof(d_zinit),
        BUFFER_BYTES = A_BYTES + B_BYTES + C_BYTES
    };
    _Static_assert(BUFFER_BYTES <= SNRT_TCDM_SIZE, "GEMM buffers exceed TCDM");
    uint8_t *const buffer = (uint8_t *)snrt_l1alloc(BUFFER_BYTES);
    if (buffer == 0)
        return 2;
    __fp16 *const Atpack = (__fp16 *)buffer;
    __fp16 *const Bpack = (__fp16 *)(buffer + A_BYTES);
    float *const C = (float *)(buffer + A_BYTES + B_BYTES);

    uint32_t *const a_words = (uint32_t *)Atpack;
    uint32_t *const b_words = (uint32_t *)Bpack;
    for (uint32_t block = 0; block < M_DIM / TE; ++block)
        for (uint32_t pair = 0; pair < K_DIM / 2; ++pair)
            for (uint32_t lane = 0; lane < TE; ++lane) {
                const uint32_t word = d_w[pair * M_DIM + block * TE + lane];
                a_words[(block * (K_DIM / 2) + pair) * TE + lane] =
                    (word << 16) | (word >> 16);
            }
    for (uint32_t block = 0; block < N_DIM / TE; ++block)
        for (uint32_t pair = 0; pair < K_DIM / 2; ++pair)
            for (uint32_t lane = 0; lane < TE; ++lane) {
                const uint32_t word = d_xpk[pair * N_DIM + block * TE + lane];
                b_words[(block * (K_DIM / 2) + pair) * TE + lane] =
                    (word << 16) | (word >> 16);
            }
    snrt_dma_start_1d(C, d_zinit, C_BYTES);
    snrt_dma_wait_all();

    const uint32_t start = get_cycle();
#if APP_FUSED
    gemm_gelu_fused(Atpack, Bpack, C, M_DIM, N_DIM, K_DIM);
#else
    gemm_gelu_non_fused(Atpack, Bpack, C, M_DIM, N_DIM, K_DIM);
    gelu_non_fused(C, M_DIM * N_DIM);
#endif
    const uint32_t cycles = get_cycle() - start;

    uint32_t errors = 0;
    float max_abs = 0.0f;
    for (uint32_t row = 0; row < M_DIM; ++row) {
        for (uint32_t col = 0; col < N_DIM; ++col) {
            const uint32_t index = row * N_DIM + col;
            union { uint32_t u; float f; } golden = {.u = d_golden[index]};
            const float expected = golden.f;
            const float delta = C[index] > expected ? C[index] - expected : expected - C[index];
            const float tolerance = 0.001f + 0.001f * (expected < 0.0f ? -expected : expected);
            if (delta > max_abs)
                max_abs = delta;
            if (!(delta <= tolerance)) {
                if (errors < 8)
                    printf("mismatch row %u col %u: got 0x%08x ref 0x%08x\n",
                           row, col, float_bits(C[index]), golden.u);
                ++errors;
            }
        }
    }
    printf("\n=== VME P2 GEMM+GELU %s M%u_N%u_K%u "
           "(baseline C128 T64 O192) ===\n",
           APP_FUSED ? "fused" : "non-fused", M_DIM, N_DIM, K_DIM);
    printf("Cycles: %u\n", cycles);
    printf("MaxAbsErr: 0x%08x\nErrors: %u\nStatus: %s\n",
           float_bits(max_abs), errors, errors ? "FAIL" : "PASS");
    return errors ? 1 : 0;
}
