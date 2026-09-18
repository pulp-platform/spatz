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

// main.c - fp16 x fp16 = fp16 VME GEMM functional check

#include <snrt.h>
#include <string.h>
#include <printf.h>

#include DATAHEADER
#include "kernel/matmul.c"

enum {
    TCDM_BYTES = 128 * 1024,
    M_CHUNK = CHUNK_SIZE,
    N_CHUNK = CHUNK_SIZE,
    K_CHUNK = CHUNK_SIZE * 2,
    TILE_INPUTS = TE * K_CHUNK,
    TILE_BYTES = TILE_INPUTS * sizeof(__fp16),
    A_TILES = M_CHUNK / TE,
    B_TILES = N_CHUNK / TE,
    A_BYTES = A_TILES * TILE_BYTES,
    B_BYTES = B_TILES * TILE_BYTES,
    C_ELEMENTS = M_CHUNK * N_CHUNK,
    C_BYTES = C_ELEMENTS * sizeof(__fp16),
    A_OFFSET = 0,
    B_OFFSET = A_OFFSET + A_BYTES,
    C_OFFSET = B_OFFSET + B_BYTES,
    BUFFER_BYTES = C_OFFSET + C_BYTES,
    L1_BUFFER_COUNT = 2,
    L1_BUFFER_STRIDE = TCDM_BYTES / L1_BUFFER_COUNT,
    L1_BYTES = L1_BUFFER_STRIDE + BUFFER_BYTES,
};

_Static_assert(BUFFER_BYTES <= L1_BUFFER_STRIDE,
               "one M64xN64xK128 TEW16 buffer must fit in 64 KiB");
_Static_assert(L1_BYTES <= TCDM_BYTES,
               "two matmul buffers exceed the 128 KiB TCDM");

static const __fp16 zero_tile_dram[TILE_INPUTS]
    __attribute__((section(".dram"), aligned(64))) = {0};

static uint8_t *l1_base;
static __fp16 *partials;
static float *c_out;

static inline uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static inline uint32_t chunk_extent(uint32_t total, uint32_t block,
                                    uint32_t chunk) {
    const uint32_t offset = block * chunk;
    return offset < total ? min_u32(chunk, total - offset) : 0;
}

static inline uint32_t float_bits(float value) {
    union {
        float f;
        uint32_t u;
    } bits = {.f = value};
    return bits.u;
}

static inline uint32_t cycle_count(void) {
    uint32_t cycles;
    asm volatile("csrr %0, mcycle" : "=r"(cycles));
    return cycles;
}
static snrt_dma_txid_t fill_panel(__fp16 *dst, const __fp16 *src,
                                  uint32_t panel_tiles,
                                  uint32_t valid_tiles,
                                  uint32_t valid_k,
                                  uint32_t src_tile_k) {
    if (valid_tiles == panel_tiles && valid_k == K_CHUNK &&
        src_tile_k == K_CHUNK) {
        return snrt_dma_start_1d(dst, src, panel_tiles * TILE_BYTES);
    }

    const snrt_dma_txid_t clear_tid = snrt_dma_start_2d(
        dst, zero_tile_dram, TILE_BYTES, TILE_BYTES, 0, panel_tiles);
    if (valid_tiles == 0 || valid_k == 0)
        return clear_tid;

    const size_t copy_bytes = valid_k * TE * sizeof(__fp16);
    const size_t src_stride = src_tile_k * TE * sizeof(__fp16);
    // The assembly kernel advances between packed tiles by 16 * valid_k
    // elements. Keep the L1 panel compact even though each buffer reserves
    // space for the maximum K_CHUNK.
    return snrt_dma_start_2d(dst, src, copy_bytes, copy_bytes, src_stride,
                             valid_tiles);
}

static snrt_dma_txid_t fill_a(__fp16 *dst, uint32_t mb, uint32_t kb) {
    const uint32_t first_tile = mb * A_TILES;
    const uint32_t tile_count =
        (matmul_l.M + TE - 1) / TE;
    const uint32_t valid_tiles = first_tile < tile_count
                                     ? min_u32(A_TILES,
                                               tile_count - first_tile)
                                     : 0;
    const uint32_t valid_k = chunk_extent(matmul_l.K, kb, K_CHUNK);
    const uint32_t k0 = kb * K_CHUNK;
    const __fp16 *src = valid_tiles == 0
                            ? zero_tile_dram
                            : matmul_Apack_dram +
                                  (first_tile * matmul_l.K + k0) * TE;
    return fill_panel(dst, src, A_TILES, valid_tiles, valid_k, matmul_l.K);
}

static snrt_dma_txid_t fill_b(__fp16 *dst, uint32_t nb, uint32_t kb) {
    const uint32_t first_tile = nb * B_TILES;
    const uint32_t tile_count =
        (matmul_l.N + TE - 1) / TE;
    const uint32_t valid_tiles = first_tile < tile_count
                                     ? min_u32(B_TILES,
                                               tile_count - first_tile)
                                     : 0;
    const uint32_t valid_k = chunk_extent(matmul_l.K, kb, K_CHUNK);
    const uint32_t k0 = kb * K_CHUNK;
    const __fp16 *src = valid_tiles == 0
                            ? zero_tile_dram
                            : matmul_Bpack_dram +
                                  (first_tile * matmul_l.K + k0) * TE;
    return fill_panel(dst, src, B_TILES, valid_tiles, valid_k, matmul_l.K);
}

static snrt_dma_txid_t fill_buffer(uint32_t buffer, uint32_t mb, uint32_t nb,
                                   uint32_t kb) {
    uint8_t *base = l1_base + buffer * L1_BUFFER_STRIDE;
    fill_a((__fp16 *)(base + A_OFFSET), mb, kb);
    return fill_b((__fp16 *)(base + B_OFFSET), nb, kb);
}

static inline float partial_at(const __fp16 *partial, uint32_t row,
                               uint32_t col) {
    return (float)partial[(size_t)row * N_CHUNK + col];
}

int main(void) {
    if (snrt_cluster_core_idx() != 0)
        return 0;

    const uint32_t mb_count = (matmul_l.M + M_CHUNK - 1) / M_CHUNK;
    const uint32_t nb_count = (matmul_l.N + N_CHUNK - 1) / N_CHUNK;
    const uint32_t kb_count = (matmul_l.K + K_CHUNK - 1) / K_CHUNK;
    const uint32_t jobs = mb_count * nb_count;
    const uint32_t iterations = jobs * kb_count;

    l1_base = (uint8_t *)snrt_l1alloc(L1_BYTES);
    partials = (__fp16 *)snrt_l3alloc((size_t)iterations * C_BYTES);
    c_out = (float *)snrt_l3alloc((size_t)matmul_l.M * matmul_l.N *
                                  sizeof(float));
    if (l1_base == 0 || partials == 0 || c_out == 0) {
        printf("fp16 matmul allocation failure\n");
        return 2;
    }

    snrt_dma_txid_t input_tid[L1_BUFFER_COUNT] = {0, 0};
    uint32_t current_buffer = 0;

    input_tid[0] = fill_buffer(0, 0, 0, 0);
    snrt_dma_wait(input_tid[0]);
    if (iterations > 1) {
        const uint32_t next_job = 1 / kb_count;
        input_tid[1] = fill_buffer(1, next_job / nb_count,
                                   next_job % nb_count, 1 % kb_count);
    }

    uint32_t start_cycle = 0;
    uint32_t end_cycle = 0;
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        const uint32_t job = iter / kb_count;
        const uint32_t mb = job / nb_count;
        const uint32_t nb = job % nb_count;
        const uint32_t kb = iter % kb_count;
        const uint32_t valid_m = chunk_extent(matmul_l.M, mb, M_CHUNK);
        const uint32_t valid_n = chunk_extent(matmul_l.N, nb, N_CHUNK);
        const uint32_t valid_k = chunk_extent(matmul_l.K, kb, K_CHUNK);
        uint8_t *base = l1_base + current_buffer * L1_BUFFER_STRIDE;
        uint32_t fcsr;

        if (iter == 0)
            start_cycle = cycle_count();
        matmul_fp16((const __fp16 *)(base + A_OFFSET),
                    (const __fp16 *)(base + B_OFFSET),
                    (__fp16 *)(base + C_OFFSET), valid_m, valid_n, valid_k);
        end_cycle = cycle_count();
        // Keep completion synchronization outside the measured kernel window.
        wait_spatz();

        snrt_dma_start_1d(partials + (size_t)iter * C_ELEMENTS,
                          base + C_OFFSET, C_BYTES);

        const uint32_t next_iter = iter + 1;
        if (next_iter < iterations) {
            const uint32_t next_buffer = current_buffer ^ 1;
            snrt_dma_wait(input_tid[next_buffer]);

            const uint32_t refill_iter = iter + 2;
            if (refill_iter < iterations) {
                const uint32_t refill_job = refill_iter / kb_count;
                input_tid[current_buffer] = fill_buffer(
                    current_buffer, refill_job / nb_count,
                    refill_job % nb_count, refill_iter % kb_count);
            }
            current_buffer = next_buffer;
        }
    }
    snrt_dma_wait_all();

    for (uint32_t row = 0; row < matmul_l.M; ++row) {
        for (uint32_t col = 0; col < matmul_l.N; ++col) {
            const uint32_t mb = row / M_CHUNK;
            const uint32_t nb = col / N_CHUNK;
            const uint32_t job = mb * nb_count + nb;
            const uint32_t local_row = row % M_CHUNK;
            const uint32_t local_col = col % N_CHUNK;
            float sum = 0.0f;
            for (uint32_t kb = 0; kb < kb_count; ++kb) {
                const __fp16 *partial = partials +
                    (size_t)(job * kb_count + kb) * C_ELEMENTS;
                sum += partial_at(partial, local_row, local_col);
            }
            c_out[(size_t)row * matmul_l.N + col] = sum;
        }
    }

    uint32_t errors = 0;
    float max_abs = 0.0f;
    for (uint32_t row = 0; row < matmul_l.M; ++row) {
        uint32_t row_errors = 0;
        for (uint32_t col = 0; col < matmul_l.N; ++col) {
            const size_t index = (size_t)row * matmul_l.N + col;
            const float actual = c_out[index];
            const float expected = matmul_Cref_dram[index];
            const float delta = actual > expected ? actual - expected
                                                  : expected - actual;
            const float tolerance = 0.05f + 0.02f *
                (expected < 0.0f ? -expected : expected);
            if (delta > max_abs)
                max_abs = delta;
            if (delta > tolerance) {
                if (row_errors == 0)
                    printf("row %u: errors at cols ", row);
                else
                    printf(",");
                printf("%u", col);
                ++row_errors;
                ++errors;
            }
        }
        if (row_errors != 0)
            printf(" (%u errors)\n", row_errors);
    }

    const uint32_t cycles = end_cycle - start_cycle;
    const uint64_t useful_fmas =
        (uint64_t)matmul_l.M * matmul_l.N * matmul_l.K;
    const uint64_t peak_fmas = (uint64_t)cycles * CE * CE;
    const uint32_t utilization_bp =
        peak_fmas != 0
            ? (uint32_t)((useful_fmas * 10000u) / peak_fmas)
            : 0;

    printf("\n=== TEW16 FP16 matmul %ux%ux%u "
           "(M64xN64xK128 chunks, 2 buffers, 1 core) ===\n",
           matmul_l.M, matmul_l.N, matmul_l.K);
    printf("Cycles:    %u\n", cycles);
    printf("Utilization: %u.%02u%%\n", utilization_bp / 100,
           utilization_bp % 100);
    printf("MaxAbsErr: 0x%08x\n", float_bits(max_abs));
    printf("Errors:    %u\n", errors);
    printf("Status:    %s\n", errors ? "FAIL" : "PASS");
    return errors ? 1 : 0;
}
