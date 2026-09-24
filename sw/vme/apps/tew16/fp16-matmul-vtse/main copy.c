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

#include <snrt.h>
#include <string.h>
#include <printf.h>

#include DATAHEADER
#include "kernel/gemm.c"

enum {
    TCDM_BYTES = 128 * 1024,
    M_CHUNK = 64,
    N_CHUNK = 64,
    K_CHUNK = 128,
    TE      = 16,
    KMAX    = 2,
    TILE_BYTES = TE * K_CHUNK * sizeof(__fp16),
    A_TILES = M_CHUNK / TE,
    B_TILES = N_CHUNK / TE,
    A_BYTES = A_TILES * TILE_BYTES,
    B_BYTES = B_TILES * TILE_BYTES,
    C_BYTES = M_CHUNK * N_CHUNK * sizeof(__fp16),
    A_OFFSET = 0,
    B_OFFSET = A_BYTES,
    C_OFFSET = B_OFFSET + B_BYTES,
    CHUNK_SIZE = M_CHUNK * N_CHUNK, // for what
    BUFFER_BYTES = C_OFFSET + C_BYTES,
    BUFFER_COUNT = 2,
    BUFFER_STRIDE = TCDM_BYTES / BUFFER_COUNT,
    L1_BYTES = BUFFER_STRIDE + BUFFER_BYTES,
}

_Static_assert(BUFFER_BYTES <= BUFFER_STRIDE,
               "one M64xN64xK128 buffer must fit in half of TCDM");
_Static_assert(L1_BYTES <= TCDM_BYTES,
               "ping-pong GEMM buffers exceed TCDM");

static const __fp16 zero_tile_dram[TILE_INPUTS]
    __attribute__((section(".dram"), aligned(64))) = {0};   // why align 64? = TE?

static uint8_t *l1_base;
static float *partials;
static float *c_out;
static uint32_t allocation_failed;
static volatile uint32_t core_cycles[2];
static volatile uint32_t verify_errors[2];
static volatile float verify_max_abs[2];

// static const float *Apack, *Bpack, *Cref;
// static float *C;

static snrt_dma_txid_t fill_panel(__fp16 *dst, const __fp16 *src, 
                                    uint32_t src_tiles, uint32_t dst_tiles,
                                    uint32_t dst_K, uint32_t src_K) {
    if (dst_tiles == src_tiles && dst_K == src_K)
        return snrt_dma_start_1d(dst, src, src_tiles * TILE_BYTES);
    const snrt_dma_txid_t clear_tid = 
            snrt_dma_start_2d(dst, zero_tile_dram, TILE_BYTES, TILE_BYTES, 0, src_tiles);
    if (dst_tiles == 0 || dst_K == 0) return clear_tid;

    const size_t copy_bytes = dst_K * TE * sizeof(__fp16);
    const size_t stride = src_K * TE * sizeof(__fp16);
    return snrt_dma_start_2d(dst, src, copy_bytes, TILE_BYTES, stride, dst_tiles);
}

static inline uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static inline uint32_t chunk_extent(uint32_t total, uint32_t block,
                                    uint32_t chunk) {
    const uint32_t offset = block * chunk;
    return offset < total ? min_u32(chunk, total - offset) : 0;
}

static snrt_dma_txid_t fill_buffer(unit32_t buffer, unit32_t mb, uint32_t nb, uint32_t kb) {
    uint8_t *base = l1_base + buffer * BUFFER_STRIDE;
    const uint32_t k0 = kb * K_CHUNK;
    const uint32_t src_K = chunk_extent(gemm_l.K, kb, K_CHUNK);
    const uint32_t m_tiles = (gemm_l.M + TE - 1) / TE;
    const uint32_t n_tiles = (gemm_l.N + TE - 1) / TE;
    const uint32_t first_mt = mb * A_TILES;
    const uint32_t first_nt = nb * B_TILES;
    const uint32_t src_a_tiles = first_mt < m_tiles ? min_u32(A_TILES, m_tiles - first_mt) : 0;
    const uint32_t src_b_tiles = first_nt < n_tiles ? min_u32(B_TILES, n_tiles - first_nt) : 0;
    const __fp16 *src_a = src_a_tiles==0 ? zero_tile_dram
                                         : gemm_Apack_dram + (first_mt * KMAX + k0) * TE;   // KMAX? or K
    const __fp16 *src_b = src_b_tiles==0 ? zero_tile_dram
                                         : gemm_Bpack_dram + (first_nt * KMAX + k0) * TE;
    const __fp16 *dst_a = (__fp16 *)(base + A_OFFSET);
    const __fp16 *dst_b = (__fp16 *)(base + B_OFFSET);

    // fill A tile
    fill_panel(dst_a, src_a, A_TILES, src_a_tiles, src_K, KMAX);  // KMAX or K?
    // fill B tile
    return fill_panel(dst_b, src_b, B_TILES, src_b_tiles, src_K, KMAX);                 
}

int main(void)
{
    const uint32_t cid = snrt_cluster_core_idx();
    const uint32_t ncores = snrt_cluster_core_num();
#ifdef SPATZ_GEMM_SINGLE_CORE
    const uint32_t active_cores = 1;
#else
    const uint32_t active_cores = ncores;
#endif

    const uint32_t row_pairs = M_CHUNK / (2 * TE);
    const uint32_t mb_count = (gemm_l.M + M_CHUNK - 1) / M_CHUNK;
    const uint32_t nb_count = (gemm_l.N + N_CHUNK - 1) / N_CHUNK;
    const uint32_t kb_count = (gemm_l.K + K_CHUNK - 1) / K_CHUNK;
    const uint32_t jobs = mb_count * nb_count;
    const uint32_t iterations = jobs * kb_count;

    if (cid == 0) {
        // core 0 has DMA
        l1_base = (uint8_t *)snrt_l1alloc(L1_BYTES);
        partials = (__fp16 *)snrt_l3alloc(iterations * CHUNK_SIZE * sizeof(__fp16));    // variable? TEW
        c_out = (float *)snrt_l3alloc(TE * TE * sizeof(__fp16));
        allocation_failed = l1_base == 0 || partials == 0 || c_out == 0;
    }
    // snrt_cluster_hw_barrier();
    snrt_dma_txid_t input_tid[BUFFER_COUNT] = {0, 0};

    uint32_t current_buffer = 0;
    uint32_t kernel_cycles = 0;

    if (cid == 0) {
        input_tid[0] = fill_buffer(0, 0, 0, 0);
        snrt_dma_wait(input_tid[0]);
    }
    // snrt_cluster_hw_barrier();

    // Iter 0 computes from buffer 0 while this descriptor chain fills buffer 1.
    if (cid == 0 && iterations > 1) {
        const uint32_t next_job = 1 / kb_count;
        input_tid[1] = fill_buffer(1, next_job / nb_count, next_job % nb_count, 1 % kb_count);
    }
    
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        const uint32_t job = iter / kb_count;
        const uint32_t mb = job / nb_count;
        const uint32_t nb = job % nb_count;
        const uint32_t kb = job % nb_count;
        const uint32_t valid_m = chunk_extent(gemm_l.M, mb, M_CHUNK);
        const uint32_t valid_n = chunk_extent(gemm_l.N, nb, N_CHUNK);
        const uint32_t valid_k = chunk_extent(gemm_l.K, kb, K_CHUNK);
        const uint32_t next_iter = iter + 1;
        const uint32_t has_next = next_iter < iterations;
        const uint32_t refill_iter = iter + 2;
        const uint32_t has_refill = refill_iter < iterations;
        const uint32_t next_buffer = current_buffer ^ 1;

        if (cid < active_cores) {
            uint8_t *base = l1_base + current_buffer * BUFFER_STRIDE;
            __fp16 *a = (__fp16 *)(base + A_OFFSET);
            __fp16 *b = (__fp16 *)(base + B_OFFSET);
            __fp16 *c = (__fp16 *)(base + C_OFFSET);
            const uint32_t t0 = gemm_cycle();
            uint32_t did_compute = 0;
            // since only use core 0
            // TODO: remove m pair, for-loop, remove finish_spatz(), replace core_m, core_row concept
            for (uint32_t pair = cid; pair < row_pairs;
                    pair += active_cores) {
                const uint32_t core_row = 2 * TILE_DIM;
                const uint32_t core_m = core_row < valid_m
                                            ? min_u32(2 * TILE_DIM,
                                                        valid_m - core_row)
                                            : 0;
                if (core_m != 0) {
                    gemm_block_fp16(
                        (void *)((uint8_t *)a +
                                    core_row * K_CHUNK * sizeof(__fp16)),
                        (void *)b,
                        (void *)((uint8_t *)c +
                                    core_row * N_CHUNK * sizeof(float)),
                        (int)valid_k, (int)valid_n, (int)core_m, 0);
                    did_compute = 1;
                }
            }

            if (did_compute) {
                finish_spatz();
                kernel_cycles += gemm_cycle() - t0;
            }
        }
        if (cid == 0) {
            // Do not switch to the alternate buffer until all of its A/B
            // descriptors have completed.
            if (has_next)
                snrt_dma_wait(input_tid[next_buffer]);

            float *src = (float *)(l1_base + current_buffer * BUFFER_STRIDE + C_OFFSET);
            snrt_dma_start_1d(partials + iter * PARTIAL_FLOATS, src, C_BYTES);

            // Reuse the just-computed buffer for iteration i+2.  The DMA
            // executes descriptors in issue order, so C(i) is read before the
            // following A/B refill overwrites this 64 KiB half.  This complete
            // chain overlaps compute(i+1) in the other half.
            if (has_refill) {
                const uint32_t refill_job = refill_iter / kb_count;
                input_tid[current_buffer] = fill_buffer(
                    current_buffer, refill_job / nb_count,
                    refill_job % nb_count, refill_iter % kb_count);
            }
        }
        // gemm_barrier(active_cores);
        if (has_next)
            current_buffer = next_buffer;
    }
        if (cid < post_cores) {
        verify_errors[cid] = local_errors;
        verify_max_abs[cid] = local_max_abs;
    }
    // snrt_cluster_hw_barrier();


    if (cid < active_cores)
        core_cycles[cid] = kernel_cycles;

    if (cid == 0)
        snrt_dma_wait_all();
    // snrt_cluster_hw_barrier();

    if (cid < post_cores) {
        const uint32_t output_elements = gemm_l.M * gemm_l.N;
        for (uint32_t idx = cid; idx < output_elements; idx += post_cores) {
            const uint32_t row = idx / gemm_l.N;
            const uint32_t col = idx % gemm_l.N;
            const uint32_t mb = row / M_CHUNK;
            const uint32_t nb = col / N_CHUNK;
            const uint32_t job = mb * nb_count + nb;
            const uint32_t local_row = row % M_CHUNK;
            const uint32_t local_col = col % N_CHUNK;
            float sum = 0.0f;
            for (uint32_t kb = 0; kb < kb_count; ++kb) {
                const float *partial = partials +
                    (job * kb_count + kb) * PARTIAL_FLOATS;
                sum += partial_at(partial, local_row, local_col);
            }
            c_out[row * gemm_l.Np + col] = sum;
        }
    }
    // snrt_cluster_hw_barrier();

    uint32_t cycles = core_cycles[0];
    for (uint32_t core = 1; core < active_cores; ++core) {
        if (core_cycles[core] > cycles)
            cycles = core_cycles[core];
    }

    enum { CE = 8 };
    const uint64_t useful_fmas =
        (uint64_t)gemm_l.M * gemm_l.N * gemm_l.K;
    const uint64_t peak_fmas =
        (uint64_t)cycles * CE * CE * active_cores;
    const uint32_t utilization_bp =
        peak_fmas != 0 ? (uint32_t)((useful_fmas * 10000u) / peak_fmas) : 0;

    uint32_t local_errors = 0;
    uint32_t printed_partial_debug = 0;
    float local_max_abs = 0.0f;
    // FP16 inputs are accumulated in FP32, but panel reduction changes the
    // FP32 addition order relative to the host reference.
    const float tolerance = 5.0e-2f;

    if (cid != 0)
        return 0;

    uint32_t errors = 0;
    float max_abs = 0.0f;
    for (uint32_t core = 0; core < post_cores; ++core) {
        errors += verify_errors[core];
        if (verify_max_abs[core] > max_abs)
            max_abs = verify_max_abs[core];
    }

    printf("\n=== General FP16 GEMM %ux%ux%u "
           "(M64xN64xK128 panels, 2 buffers, %u core%s) ===\n",
           gemm_l.M, gemm_l.N, gemm_l.K, active_cores,
           active_cores == 1 ? "" : "s");
    printf("Cycles:    %u\n", cycles);
    printf("Utilization: %u.%02u%%\n", utilization_bp / 100,
           utilization_bp % 100);
    printf("MaxAbsErr: 0x%08x\n", *(uint32_t *)&max_abs);
    printf("Errors:    %u\n", errors);
    printf("Status:    %s\n", errors ? "FAIL" : "PASS");
    return errors ? 1 : 0;
}
