// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

#include "gemm.h"

/*
 * Keep the hand-scheduled bias-load M64xN64xK64 implementation as the fast
 * path without changing its source. It only reads gemm_l.M/N/K to construct
 * panel-local pointers, so bind those fields to the fixed panel shape.
 */
static const struct {
    uint32_t M;
    uint32_t N;
    uint32_t K;
} gemm_tuned_shape = {
    .M = 64,
    .N = 64,
    .K = 64,
};

#define gemm_l gemm_tuned_shape
#include "../../fp32-matmul-vle-vse/kernel/gemm.c"
#undef gemm_l

enum {
    GEMM_TE = 16,
    GEMM_MAX_CORE_M = 32,
    GEMM_PANEL_N = 64,
    GEMM_PANEL_K = 64,
    GEMM_TILE_BYTES = GEMM_TE * GEMM_TE * sizeof(float),
    GEMM_TILE_K_BYTES = GEMM_TE * GEMM_PANEL_K * sizeof(float),
    GEMM_C_TILE_ROW_BYTES = (GEMM_PANEL_N / GEMM_TE) * GEMM_TILE_BYTES,
};

/*
 * Runtime edge kernel for one per-core physical M32xN64xK64 panel.
 *
 * All runtime scheduling is inside this assembly block:
 *   - N is traversed in one or two 32-column blocks.
 *   - K / 8 full groups use LMUL=8 grouped loads and eight VTFMMs.
 *   - K % 8 tail positions use LMUL=1 loads and one VTFMM each.
 *   - quadrant activation, loops, stores, and block pointer updates use
 *     scalar branch instructions rather than C control flow.
 *
 * A/B retain a physical K stride of 64.  C uses packed 16x16 tile-major
 * layout, matching partial_at() in main.c.
 */
__attribute__((noinline, aligned(64))) void gemm_block_fp32(
    void *addrA, void *addrB, void *addrC,
    int K, int N, int M, int alt_fmt)
{
    const uintptr_t rows = (uintptr_t)M;
    const uintptr_t depth = (uintptr_t)K;
    uintptr_t n_remaining = (uintptr_t)N;
    uintptr_t block_count = ((uintptr_t)N + 31u) >> 5;
    uintptr_t a0p = (uintptr_t)addrA;
    uintptr_t a1p = a0p + GEMM_TILE_K_BYTES;
    uintptr_t b0p = (uintptr_t)addrB;
    uintptr_t b1p = b0p + GEMM_TILE_K_BYTES;
    uintptr_t p0 = (uintptr_t)addrC;
    uintptr_t p1 = p0 + GEMM_C_TILE_ROW_BYTES;
    uintptr_t q0 = p0;
    uintptr_t q1 = p1;
    uintptr_t tss0 = GEMM_TSS_MT0_ROW0;
    uintptr_t tss4 = GEMM_TSS_MT4_ROW0;
    uintptr_t tss8 = GEMM_TSS_MT8_ROW0;
    uintptr_t tss12 = GEMM_TSS_MT12_ROW0;
    uintptr_t full_groups;
    uintptr_t tail_count;
    uintptr_t tm;
    uintptr_t tn;
    uintptr_t scratch;

    (void)alt_fmt;

    asm volatile(
        "beqz %[rows], .Lgemm_done_%=\n"
        "beqz %[n_remaining], .Lgemm_done_%=\n"
        "beqz %[depth], .Lgemm_done_%=\n"

        /*
         * M is fixed for this per-core call.  A second row tile is active
         * exactly when M > 16; otherwise tm carries the true edge size.
         */
        "li %[scratch], 16\n"
        "mv %[tm], %[scratch]\n"
        "bltu %[scratch], %[rows], .Lgemm_tm_ready_%=\n"
        "mv %[tm], %[rows]\n"
        ".Lgemm_tm_ready_%=:\n"

        /*
         * One assembly iteration covers up to two 16-column tiles.
         * n_remaining > 16 activates mt4/mt12.
         */
        ".Lgemm_n_block_%=:\n"
        "li %[scratch], 16\n"
        "mv %[tn], %[scratch]\n"
        "bltu %[scratch], %[n_remaining], .Lgemm_tn_ready_%=\n"
        "mv %[tn], %[n_remaining]\n"
        ".Lgemm_tn_ready_%=:\n"

        "msetmtypei 1, 2\n"
        "msettm x0, %[tm]\n"
        "li %[scratch], 8\n"
        "msettk x0, %[scratch]\n"
        "msettn x0, %[tn]\n"
        "vsetvli %[scratch], x0, e32, m8, ta, ma\n"

        // Load mt0
        "vle32.v v24, (%[q0])\n" "addi %[q0], %[q0], 512\n"
        "vtmv.t.v %[tss0], v24\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v25\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v26\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v27\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v28\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v29\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v30\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v31\n" "addi %[tss0], %[tss0], 1\n"
        "vle32.v v8, (%[q0])\n"  "addi %[q0], %[q0], -512\n"
        "vtmv.t.v %[tss0], v8\n"  "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v9\n"  "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v10\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v11\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v12\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v13\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v14\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.t.v %[tss0], v15\n" "addi %[tss0], %[tss0], -15\n"
        "bgeu %[tn], %[n_remaining], .Lgemm_zero_m_done_%=\n"

        // Load mt4
        "addi %[q0], %[q0], 1024\n"
        "vle32.v v24, (%[q0])\n" "addi %[q0], %[q0], 512\n"
        "vtmv.t.v %[tss4], v24\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v25\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v26\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v27\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v28\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v29\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v30\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v31\n" "addi %[tss4], %[tss4], 1\n"
        "vle32.v v8, (%[q0])\n"  "addi %[q0], %[q0], -512\n"
        "vtmv.t.v %[tss4], v8\n"  "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v9\n"  "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v10\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v11\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v12\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v13\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v14\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v15\n" "addi %[tss4], %[tss4], -15\n"
        ".Lgemm_zero_m_done_%=:\n"
        "bgeu %[tm], %[rows], .Lgemm_zero_done_%=\n"

        // Load mt8
        "vle32.v v16, (%[q1])\n"  "addi %[q1], %[q1], 512\n"
        "vtmv.t.v %[tss8], v16\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v17\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v18\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v19\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v20\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v21\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v22\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v23\n" "addi %[tss8], %[tss8], 1\n"
        "vle32.v v8, (%[q1])\n"  "addi %[q1], %[q1], -512\n"
        "vtmv.t.v %[tss8], v8\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v9\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v10\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v11\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v12\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v13\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v14\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v15\n" "addi %[tss8], %[tss8], -15\n"
        "bgeu %[tn], %[n_remaining], .Lgemm_zero_done_%=\n"

        // Load mt12
        "addi %[q1], %[q1], 1024\n"
        "vle32.v v24, (%[q1])\n" "addi %[q1], %[q1], 512\n"
        "vtmv.t.v %[tss12], v24\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v25\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v26\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v27\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v28\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v29\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v30\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v31\n" "addi %[tss12], %[tss12], 1\n"
        "vle32.v v8, (%[q1])\n"  "addi %[q1], %[q1], -512\n"
        "vtmv.t.v %[tss12], v8\n"  "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v9\n"  "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v10\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v11\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v12\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v13\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v14\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v15\n" "addi %[tss12], %[tss12], -15\n"
        ".Lgemm_zero_done_%=:\n"

        /*
         * Full software K groups.  One m8 VLE fills eight architectural
         * registers; the eight VTFMMs consume those registers as eight
         * independent outer products.
         */
        "srli %[full_groups], %[depth], 3\n"
        "andi %[tail_count], %[depth], 7\n"
        "beqz %[full_groups], .Lgemm_tail_setup_%=\n"

        ".Lgemm_full_loop_%=:\n"
        "vle32.v v0,  (%[a0p])\n"
        "vle32.v v16, (%[b0p])\n"

        "vtfmm.tvv mt0, v0, v16\n"
        "bgeu %[tn], %[n_remaining], .Lgemm_full_b1_loaded_%=\n"
        "vle32.v v24, (%[b1p])\n"
        ".Lgemm_full_b1_loaded_%=:\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtfmm.tvv mt0, v3, v19\n"

        "bgeu %[tm], %[rows], .Lgemm_full_a1_loaded_%=\n"
        "vle32.v v8, (%[a1p])\n"
        ".Lgemm_full_a1_loaded_%=:\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vtfmm.tvv mt0, v7, v23\n"

        "bgeu %[tn], %[n_remaining], .Lgemm_full_mt4_done_%=\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vtfmm.tvv mt4, v7, v31\n"
        ".Lgemm_full_mt4_done_%=:\n"

        "bgeu %[tm], %[rows], .Lgemm_full_a_done_%=\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vtfmm.tvv mt8, v13, v21\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vtfmm.tvv mt8, v15, v23\n"

        "bgeu %[tn], %[n_remaining], .Lgemm_full_a_done_%=\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        ".Lgemm_full_a_done_%=:\n"

        /*
         * Keep four independent scalar instructions after the final source
         * use before the next grouped VLE can overwrite v0/v16.
         */
        "addi %[a0p], %[a0p], 512\n"
        "addi %[b0p], %[b0p], 512\n"
        "bgeu %[tn], %[n_remaining], .Lgemm_full_b1_advanced_%=\n"
        "addi %[b1p], %[b1p], 512\n"
        ".Lgemm_full_b1_advanced_%=:\n"
        "bgeu %[tm], %[rows], .Lgemm_full_a1_advanced_%=\n"
        "addi %[a1p], %[a1p], 512\n"
        ".Lgemm_full_a1_advanced_%=:\n"
        "addi %[full_groups], %[full_groups], -1\n"
        "bnez %[full_groups], .Lgemm_full_loop_%=\n"

        /*
         * K remainder.  LMUL=1 makes each VLE produce one source vector and
         * each loop iteration contributes exactly one outer product.
         */
        ".Lgemm_tail_setup_%=:\n"
        "beqz %[tail_count], .Lgemm_store_setup_%=\n"
        "vsetivli x0, 16, e32, m1, ta, ma\n"

        ".Lgemm_tail_loop_%=:\n"
        "vle32.v v0,  (%[a0p])\n"
        "vle32.v v16, (%[b0p])\n"

        "vtfmm.tvv mt0, v0, v16\n"
        "bgeu %[tn], %[n_remaining], .Lgemm_tail_b1_loaded_%=\n"
        "vle32.v v24, (%[b1p])\n"
        ".Lgemm_tail_b1_loaded_%=:\n"

        "bgeu %[tm], %[rows], .Lgemm_tail_a1_loaded_%=\n"
        "vle32.v v8, (%[a1p])\n"
        ".Lgemm_tail_a1_loaded_%=:\n"

        "bgeu %[tn], %[n_remaining], .Lgemm_tail_mt4_done_%=\n"
        "vtfmm.tvv mt4, v0, v24\n"
        ".Lgemm_tail_mt4_done_%=:\n"

        "bgeu %[tm], %[rows], .Lgemm_tail_a_done_%=\n"
        "vtfmm.tvv mt8, v8, v16\n"
        "bgeu %[tn], %[n_remaining], .Lgemm_tail_a_done_%=\n"
        "vtfmm.tvv mt12, v8, v24\n"
        ".Lgemm_tail_a_done_%=:\n"

        "addi %[a0p], %[a0p], 64\n"
        "addi %[b0p], %[b0p], 64\n"
        "bgeu %[tn], %[n_remaining], .Lgemm_tail_b1_advanced_%=\n"
        "addi %[b1p], %[b1p], 64\n"
        ".Lgemm_tail_b1_advanced_%=:\n"
        "bgeu %[tm], %[rows], .Lgemm_tail_a1_advanced_%=\n"
        "addi %[a1p], %[a1p], 64\n"
        ".Lgemm_tail_a1_advanced_%=:\n"
        "addi %[tail_count], %[tail_count], -1\n"
        "bnez %[tail_count], .Lgemm_tail_loop_%=\n"

        /*
         * Packed tile store.  VTMV collects 16 rows into two eight-register
         * groups; each VSE writes one 512-byte half tile.
         */
        ".Lgemm_store_setup_%=:\n"
        "vsetvli %[scratch], x0, e32, m8, ta, ma\n"

        // mt0 -> p0
        "li %[scratch], 0\n"
        "vtmv.v.t v0,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v1,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v2,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v3,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v4,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v5,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v6,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v7,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vse32.v v0, (%[p0])\n" "addi %[p0], %[p0], 512\n"
        "vtmv.v.t v8,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v9,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v10, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v11, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v12, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v13, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v14, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v15, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vse32.v v8, (%[p0])\n" "addi %[p0], %[p0], 512\n"

        // mt4 -> p0, when the second column tile is active
        "bgeu %[tn], %[n_remaining], .Lgemm_store_top_done_%=\n"
        "li %[scratch], 0x20000000\n"
        "vtmv.v.t v0,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v1,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v2,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v3,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v4,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v5,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v6,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v7,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vse32.v v0, (%[p0])\n" "addi %[p0], %[p0], 512\n"
        "vtmv.v.t v8,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v9,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v10, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v11, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v12, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v13, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v14, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v15, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vse32.v v8, (%[p0])\n" "addi %[p0], %[p0], 512\n"
        ".Lgemm_store_top_done_%=:\n"

        // No lower row tiles means neither mt8 nor mt12 is stored.
        "bgeu %[tm], %[rows], .Lgemm_store_done_%=\n"

        // mt8 -> p1
        "li %[scratch], 0x40000000\n"
        "vtmv.v.t v0,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v1,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v2,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v3,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v4,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v5,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v6,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v7,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vse32.v v0, (%[p1])\n" "addi %[p1], %[p1], 512\n"
        "vtmv.v.t v8,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v9,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v10, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v11, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v12, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v13, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v14, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v15, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vse32.v v8, (%[p1])\n" "addi %[p1], %[p1], 512\n"

        // mt12 -> p1, when the second column tile is active
        "bgeu %[tn], %[n_remaining], .Lgemm_store_done_%=\n"
        "li %[scratch], 0x60000000\n"
        "vtmv.v.t v0,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v1,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v2,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v3,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v4,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v5,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v6,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v7,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vse32.v v0, (%[p1])\n" "addi %[p1], %[p1], 512\n"
        "vtmv.v.t v8,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v9,  %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v10, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v11, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v12, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v13, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v14, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vtmv.v.t v15, %[scratch]\n" "addi %[scratch], %[scratch], 1\n"
        "vse32.v v8, (%[p1])\n" "addi %[p1], %[p1], 512\n"

        ".Lgemm_store_done_%=:\n"
        "addi %[block_count], %[block_count], -1\n"
        "beqz %[block_count], .Lgemm_done_%=\n"

        /*
         * Rewind A to K=0 and advance B to the next two column tiles.
         * Reaching this path implies the current block had two active N tiles,
         * so b0p/b1p and p0/p1 have both advanced symmetrically.
         */
        "slli %[scratch], %[depth], 6\n"
        "sub %[a0p], %[a0p], %[scratch]\n"
        "sub %[b0p], %[b0p], %[scratch]\n"
        "sub %[b1p], %[b1p], %[scratch]\n"
        "bgeu %[tm], %[rows], .Lgemm_a1_rewound_%=\n"
        "sub %[a1p], %[a1p], %[scratch]\n"
        ".Lgemm_a1_rewound_%=:\n"
        "li %[scratch], 8192\n"
        "add %[b0p], %[b0p], %[scratch]\n"
        "add %[b1p], %[b1p], %[scratch]\n"

        /*
         * q0 ended on mt4, so another tile reaches the next block's mt0.
         * q1 ended on mt12 only when the lower row tile was active; otherwise
         * it needs both tile advances here.
         */
        "addi %[q0], %[q0], 1024\n"
        "addi %[q1], %[q1], 1024\n"
        "bltu %[tm], %[rows], .Lgemm_q1_advanced_%=\n"
        "addi %[q1], %[q1], 1024\n"
        ".Lgemm_q1_advanced_%=:\n"
        "addi %[n_remaining], %[n_remaining], -32\n"
        "j .Lgemm_n_block_%=\n"

        ".Lgemm_done_%=:\n"
        : [a0p] "+&r"(a0p),
          [a1p] "+&r"(a1p),
          [b0p] "+&r"(b0p),
          [b1p] "+&r"(b1p),
          [p0] "+&r"(p0),
          [p1] "+&r"(p1),
          [q0] "+&r"(q0),
          [q1] "+&r"(q1),
          [tss0] "+&r"(tss0),
          [tss4] "+&r"(tss4),
          [tss8] "+&r"(tss8),
          [tss12] "+&r"(tss12),
          [n_remaining] "+&r"(n_remaining),
          [block_count] "+&r"(block_count),
          [full_groups] "=&r"(full_groups),
          [tail_count] "=&r"(tail_count),
          [tm] "=&r"(tm),
          [tn] "=&r"(tn),
          [scratch] "=&r"(scratch)
        : [rows] "r"(rows),
          [depth] "r"(depth)
        : "memory");
}
