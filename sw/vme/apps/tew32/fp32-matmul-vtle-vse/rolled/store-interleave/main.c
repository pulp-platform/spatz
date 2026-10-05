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

// main.c — fp32×fp32=fp32 VME GEMM functional check

#include <snrt.h>
#include <string.h>
#include <printf.h>

#include DATAHEADER
#include "kernel/gemm.c"

// Shared L1 buffers: core 0 allocates and DMAs; the pointers live in shared
// TCDM (.data) so every core sees the same addresses. Declared at file scope
// (not on main's stack) precisely so the assignment by core 0 is visible to the
// other cores after the barrier -- the sp-fmatmul multi-core pattern.
static const float *Apack, *Bpack, *Cref;
static float *C;
static uint32_t allocation_failed;
static volatile uint32_t core_cycles[16];

int main(void)
{
    const uint32_t cid = snrt_cluster_core_idx();
    const uint32_t ncores = snrt_cluster_core_num();
#ifdef SPATZ_GEMM_SINGLE_CORE
    const uint32_t active_cores = 1;
#else
    const uint32_t active_cores = ncores;
#endif

    const uint32_t M  = gemm_l.M;
    const uint32_t N  = gemm_l.N;
    const uint32_t K  = gemm_l.K;
    const uint32_t TM = 16;
    const uint32_t TN = 16;
    const uint32_t output_ld = N;

    // Core 0 owns the DMA engine and publishes all shared L1 pointers.
    const uint32_t c_bytes = M * output_ld * sizeof(float);
    const uint32_t c_ref_bytes = M * N * sizeof(float);
    const uint32_t a_bytes = K * M * sizeof(float);
    const uint32_t b_bytes = K * N * sizeof(float);

    if (cid == 0) {
        C = (float *)snrt_l1alloc(c_bytes);
        Cref = (float *)snrt_l1alloc(c_ref_bytes);
        Bpack = (float *)snrt_l1alloc(b_bytes);
        Apack = (float *)snrt_l1alloc(a_bytes);
        allocation_failed = C == NULL || Cref == NULL || Apack == NULL || Bpack == NULL;
        if (!allocation_failed) {
            snrt_dma_start_1d((void *)Cref, gemm_C_dram, c_ref_bytes);
            snrt_dma_start_1d((void *)Apack, gemm_Apack_dram, a_bytes);
            snrt_dma_start_1d((void *)Bpack, gemm_Bpack_dram, b_bytes);
            snrt_dma_wait_all();
            memset(C, 0, c_bytes);
        }
    }

    // Partition complete 2x2 tile blocks. This keeps the range pair-aligned and
    // leaves excess cores idle when M=32 has only one row-tile pair.
    const uint32_t npairs = (M / TM) / 2;
    const uint32_t pairs_per_core =
        (npairs + active_cores - 1) / active_cores;
    uint32_t pair_lo = cid < active_cores ? cid * pairs_per_core : npairs;
    uint32_t pair_hi = cid < active_cores ? pair_lo + pairs_per_core : npairs;
    if (pair_hi > npairs) pair_hi = npairs;
    if (pair_lo > npairs) pair_lo = npairs;
    const uint32_t ti_lo = 2 * pair_lo;
    const uint32_t ti_hi = 2 * pair_hi;

    // Barrier: all cores wait until the buffers are DMA'd in and pointers set.
    snrt_cluster_hw_barrier();

    if (allocation_failed) {
        if (cid == 0) printf("GEMM allocation failed\n");
        return 2;
    }

    if (K % 8 != 0)
        return 2;

    // Synchronize the launch, but keep both barriers outside the measured
    // interval. Each core measures only its own GEMM slice.
    snrt_cluster_hw_barrier();
    if (cid < active_cores) {
        uint32_t t0 = get_cycle();
        gemm_fp32(C, Apack, Bpack, ti_lo, ti_hi);
        uint32_t t1 = get_cycle();
        core_cycles[cid] = t1 - t0;
    } else {
        core_cycles[cid] = 0;
    }

    // Publish all per-core durations before core 0 selects the parallel
    // wall-clock time, defined as the slowest core's kernel duration.
    snrt_cluster_hw_barrier();

    // Only core 0 verifies and reports.
    if (cid != 0)
        return 0;

    uint32_t cycles = 0;
    for (uint32_t i = 0; i < active_cores; ++i) {
        if (core_cycles[i] > cycles)
            cycles = core_cycles[i];
    }

    enum { CE = 8 };
    const uint64_t useful_fmas = (uint64_t)M * N * K;
    const uint64_t peak_fmas =
        (uint64_t)cycles * CE * CE * active_cores;
    const uint32_t utilization_bp =
        peak_fmas != 0 ? (uint32_t)((useful_fmas * 10000u) / peak_fmas) : 0;

    // Element-wise error check.
    // fdiv.s is not supported by the Spatz FPU; use fmul.s to avoid it.
    // Instead of rel_err = abs_err / den > tol, check abs_err > tol * den.
    int   n_err   = 0;
    float max_abs = 0.0f;
    const float tol = 1.0e-3f;

    uint32_t row_errs[M];
    memset(row_errs, 0, sizeof(row_errs));
    for (uint32_t idx = 0; idx < M * N; idx++) {
        const uint32_t row = idx / N;
        const uint32_t col = idx % N;
        const uint32_t tile_row = row / TM;
        const uint32_t tile_col = col / TN;
        const uint32_t row_in_tile = row % TM;
        const uint32_t col_in_tile = col % TN;
        const uint32_t packed_idx =
            (tile_row * (N / TN) + tile_col) * (TM * TN) +
            row_in_tile * TN + col_in_tile;
        float diff    = C[packed_idx] - Cref[idx];
        float abs_err = diff      < 0.0f ? -diff      : diff;
        float den     = Cref[idx] < 0.0f ? -Cref[idx] : Cref[idx];
        if (den < 1.0e-9f) den = 1.0e-9f;
        if (abs_err > max_abs) max_abs = abs_err;
        if (abs_err > tol && abs_err > tol * den) {
            row_errs[idx / N]++;
            n_err++;
        }
    }
    for (uint32_t r = 0; r < M; r++) {
        if (row_errs[r])
            printf("row %u: %u errors\n", (unsigned)r, (unsigned)row_errs[r]);
    }

    printf("\n=== VME fp32 GEMM %ux%ux%u (TM=%u TN=%u, %u core%s) ===\n",
           (unsigned)M, (unsigned)N, (unsigned)K,
           (unsigned)TM, (unsigned)TN,
           (unsigned)active_cores, active_cores == 1 ? "" : "s");
    printf("Cycles:    %u\n", (unsigned)cycles);
    printf("Utilization: %u.%02u%%\n", (unsigned)(utilization_bp / 100),
           (unsigned)(utilization_bp % 100));
    printf("MaxAbsErr: 0x%08x\n", *(unsigned *)&max_abs);
    printf("Errors:    %d\n",   n_err);
    printf("Status:    %s\n",   n_err == 0 ? "PASS" : "FAIL");

    // No trailing barrier: the non-zero cores have already returned, so an extra
    // barrier here would have no partner and deadlock.
    return n_err == 0 ? 0 : 1;
}
