
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

#include "gemm.h"

enum {
    GEMM_TSS_MT0_ROW0 = 0u << 27,
    GEMM_TSS_MT4_ROW0 = 4u << 27,
    GEMM_TSS_MT8_ROW0 = 8u << 27,
    GEMM_TSS_MT12_ROW0 = 12u << 27,
};

__attribute__((noinline, aligned(64))) void gemm_fp32(
    float *C, const float *Bias, const float *Apack, const float *Bpack,
    uint32_t ti_lo, uint32_t ti_hi)
{
    const int ti = (int)ti_lo;
    const int col_blocks = (int)gemm_l.N / 32;
    const int col_tiles = (int)gemm_l.N / 16;       // TILE_DIM
    const int tile_stride = 16 * (int)gemm_l.K;
    const int output_stride = gemm_l.N < 64 ? 64 : (int)gemm_l.N;

    // The M64 two-core partition assigns one row-tile pair to each core.
    (void)ti_hi;

    if (ti_lo != 0)
        asm volatile("nop" ::: "memory");

    uintptr_t a0p = (uintptr_t)(Apack + ti * tile_stride);
    uintptr_t a1p = (uintptr_t)(Apack + (ti + 1) * tile_stride);
    uintptr_t b0p = (uintptr_t)Bpack;
    uintptr_t b1p = (uintptr_t)(Bpack + tile_stride);
    uintptr_t tss0 = GEMM_TSS_MT0_ROW0;
    uintptr_t tss4 = GEMM_TSS_MT4_ROW0;
    uintptr_t tss8 = GEMM_TSS_MT8_ROW0;
    uintptr_t tss12 = GEMM_TSS_MT12_ROW0;
    // Bias remains tile-packed for two LMUL=8 loads per tile. VTSE writes C
    // through independent row-major pointers.
    uintptr_t q0 = (uintptr_t)(Bias + ti * col_tiles * 16 * 16);
    uintptr_t q1 = (uintptr_t)(Bias + (ti + 1) * col_tiles * 16 * 16);
    uintptr_t p00 = (uintptr_t)(C + ti * 16 * output_stride);
    uintptr_t p01 = p00 + 16 * sizeof(float);
    uintptr_t p10 = (uintptr_t)(C + (ti + 1) * 16 * output_stride);
    uintptr_t p11 = p10 + 16 * sizeof(float);
    const uintptr_t c_stride = (uintptr_t)(output_stride * sizeof(float));
    const uintptr_t tile_dim = 16;
    const uintptr_t TK = 8;
    uintptr_t block_counter = (uintptr_t)col_blocks;
    uintptr_t loop_counter;

    asm volatile(

        // Every block enters the same bias-load and first-K-group schedule.
        "10:\n"
        // K-Group 0
        "msetmtypei 1, 2\n"
        "msettn x0, %[tile_dim]\n"
        "vsetvli %[loop], x0, e32, m8, ta, ma\n"
        "msettm x0, %[tile_dim]\n"
        "msettk x0, %[tk]\n"
        // Load all 16 rows of mt0 before its first MAC. Each LMUL=8 load
        // supplies eight physical vector registers, hence two loads per tile.
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

        // A0 is reused by mt0 and mt4. B0 remains at its current address
        // because v16..v23 must be restored after loading the mt8 bias.
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "vle32.v v16, (%[b0p])\n"

        // mt0 compute || load and install mt4 bias.
        "vtfmm.tvv mt0, v0, v16\n"
        "addi %[q0], %[q0], 1024\n"
        "vle32.v v24, (%[q0])\n" "addi %[q0], %[q0], 512\n"
        "vtmv.t.v %[tss4], v24\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v25\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtmv.t.v %[tss4], v26\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v27\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtmv.t.v %[tss4], v28\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v29\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtmv.t.v %[tss4], v30\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v31\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vle32.v v8, (%[q0])\n" "addi %[q0], %[q0], -512\n"
        "vtmv.t.v %[tss4], v8\n"  "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v9\n"  "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vtmv.t.v %[tss4], v10\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v11\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vtmv.t.v %[tss4], v12\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v13\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt0, v7, v23\n"
        "vtmv.t.v %[tss4], v14\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.t.v %[tss4], v15\n" "addi %[tss4], %[tss4], -15\n"

        // Replace the consumed mt4-bias group with B1 exactly once. B1 is
        // then reused by mt4 and mt12.
        "vle32.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"

        // mt4 compute || seed mt12 in v8..v15 and v16..v23.
        "addi %[q1], %[q1], 1024\n"
        "vle32.v v8, (%[q1])\n" "addi %[q1], %[q1], 512\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtmv.t.v %[tss12], v8\n"  "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v9\n"  "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtmv.t.v %[tss12], v10\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v11\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vle32.v v16, (%[q1])\n" "addi %[q1], %[q1], -512\n"
        "vtmv.t.v %[tss12], v12\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v13\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtmv.t.v %[tss12], v14\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v15\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vtmv.t.v %[tss12], v16\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v17\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vtmv.t.v %[tss12], v18\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v19\n" "addi %[tss12], %[tss12], 1\n"
        "vle32.v v8, (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vtmv.t.v %[tss12], v20\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v21\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt4, v7, v31\n"
        "vtmv.t.v %[tss12], v22\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.t.v %[tss12], v23\n" "addi %[tss12], %[tss12], -15\n"

        // mt12 compute || seed mt8 in v16..v23 and v0..v7.
        "addi %[q1], %[q1], -1024\n"
        "vle32.v v16, (%[q1])\n" "addi %[q1], %[q1], 512\n"
        "vtfmm.tvv mt12, v8, v24\n"
        "vtmv.t.v %[tss8], v16\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v17\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v9, v25\n"
        "vtmv.t.v %[tss8], v18\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v19\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v0, (%[q1])\n" "addi %[q1], %[q1], -512\n"
        "vtmv.t.v %[tss8], v20\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v21\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtmv.t.v %[tss8], v22\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v23\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtmv.t.v %[tss8], v0\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v1\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtmv.t.v %[tss8], v2\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v3\n" "addi %[tss8], %[tss8], 1\n"
        "vle32.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtmv.t.v %[tss8], v4\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v5\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vtmv.t.v %[tss8], v6\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.t.v %[tss8], v7\n" "addi %[tss8], %[tss8], -15\n"

        // mt8 compute. A0 may be preloaded because mt8 reads A1 from v8..v15.
        // Keep B0 intact until the final v15/v23 consumer has issued.
        "vtfmm.tvv mt8, v8,  v16\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "vtfmm.tvv mt8, v13, v21\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vtfmm.tvv mt8, v15, v23\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"

        // K-Group 1 to N-3
        // vle B1
        // vtfmm mt0, A0, B0
        "9:\n"
        "li %[loop], 5\n"
        "1:\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vle32.v v24,  (%[b1p])\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "addi %[b1p], %[b1p], 512\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vtfmm.tvv mt0, v7, v23\n"

        // vle A1
        // vtfmm mt4, A0, B1
        "vle32.v v8,  (%[a1p])\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "addi %[a1p], %[a1p], 512\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vtfmm.tvv mt4, v7, v31\n"

        // preload vle A0
        // vtfmm mt8, A1, B0
        "vle32.v v0,  (%[a0p])\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        "addi %[a0p], %[a0p], 512\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vtfmm.tvv mt8, v10,  v18\n"
        "vtfmm.tvv mt8, v11,  v19\n"
        "vtfmm.tvv mt8, v12,  v20\n"
        "vtfmm.tvv mt8, v13,  v21\n"
        "vtfmm.tvv mt8, v14,  v22\n"
        "vtfmm.tvv mt8, v15,  v23\n"

        // preload vle B0
        // vtfmm mt12, A1, B1
        "addi %[loop], %[loop], -1\n"
        "vle32.v v16,  (%[b0p])\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "bnez %[loop], 1b\n"
        // "3:\n"

        // K-Group K-2 & K-Group K-1
        // vle B0'
        // vle A0'
        // vtfmm mt0, A0, B0
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vle32.v v24,  (%[b0p])\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vle32.v v8,  (%[a0p])\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vtfmm.tvv mt0, v7, v23\n"
        "addi %[a0p], %[a0p], 512\n"
        "addi %[b0p], %[b0p], 512\n"

        // vle B1
        // vtfmm mt0, A0', B0'
        "vtfmm.tvv mt0, v8, v24\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vle32.v v16,  (%[b1p])\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vtfmm.tvv mt0, v15, v31\n"
        "addi %[b1p], %[b1p], 512\n"

        // vle B1'
        // vtfmm mt4, A0, B1
        "vtfmm.tvv mt4, v0, v16\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vle32.v v24,  (%[b1p])\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "addi %[b1p], %[b1p], 512\n"

        // vle A1
        // vtfmm mt4, A0', B1' || stage mt0 rows 0..7 in the VRF
        "vtmv.v.t v0, %[tss0]\n"  "addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v1, %[tss0]\n"  "addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"

        "vtmv.v.t v2, %[tss0]\n"  "addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v3, %[tss0]\n"  "addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v9, v25\n"

        "vtmv.v.t v4, %[tss0]\n"  "addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v5, %[tss0]\n"  "addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"

        "vtmv.v.t v6, %[tss0]\n"  "addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v7, %[tss0]\n"  "addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"

        "vtfmm.tvv mt4, v12, v28\n"

        "vtfmm.tvv mt4, v13, v29\n"

        "vtfmm.tvv mt4, v14, v30\n"

        "vle32.v v0,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "vtfmm.tvv mt4, v15, v31\n"

        // vle A1'
        // vtfmm mt12, A1, B1 || stage mt0 rows 8..15 in the VRF
        "vtmv.v.t v8,  %[tss0]\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v9,  %[tss0]\n" "addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt12, v0,  v16\n"

        "vtmv.v.t v10, %[tss0]\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v11, %[tss0]\n" "addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt12, v1,  v17\n"

        "vtmv.v.t v12, %[tss0]\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v13, %[tss0]\n" "addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"

        "vtmv.v.t v14, %[tss0]\n" "addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v15, %[tss0]\n" "addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"

        "vtfmm.tvv mt12, v4,  v20\n"

        "vtfmm.tvv mt12, v5,  v21\n"

        "vtfmm.tvv mt12, v6,  v22\n"

        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "vtfmm.tvv mt12, v7,  v23\n"

        // vle reload B0 (restore B0 pointer)
        // vtfmm mt12, A1', B1' || stage mt4 rows 0..7 in the VRF
        "vtmv.v.t v16, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v17, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"

        "vtmv.v.t v18, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v19, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"

        "vtmv.v.t v20, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v21, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"

        "vtmv.v.t v22, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v23, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"

        "vtfmm.tvv mt12, v12,  v28\n"

        "vtfmm.tvv mt12, v13,  v29\n"
        "addi %[b0p], %[b0p], -1024\n"

        "vtfmm.tvv mt12, v14,  v30\n"

        "vle32.v v16,  (%[b0p])\n"  "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt12, v15,  v31\n"

        // vle reload B0'
        // vtfmm mt8, A1, B0 || stage mt4 rows 8..15 in the VRF
        "vtmv.v.t v24, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v25, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v0,  v16\n"

        "vtmv.v.t v26, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v27, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v1,  v17\n"

        "vtmv.v.t v28, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v29, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"

        "vtmv.v.t v30, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v31, %[tss4]\n" "addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"

        "vtfmm.tvv mt8, v4,  v20\n"

        "vtfmm.tvv mt8, v5,  v21\n"

        "vtfmm.tvv mt8, v6,  v22\n"

        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt8, v7,  v23\n"

        // vtfmm mt8, A1', B0' || stage all mt12 rows in the VRF
        "addi %[loop], %[tss12], 8\n"

        "vtmv.v.t v0, %[tss12]\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v1, %[tss12]\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"

        "vtmv.v.t v2, %[tss12]\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v3, %[tss12]\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt8, v9,  v25\n"

        "vtmv.v.t v4, %[tss12]\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v5, %[tss12]\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"

        "vtmv.v.t v6, %[tss12]\n" "addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v7, %[tss12]\n" "addi %[tss12], %[tss12], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"

        "vtmv.v.t v16, %[loop]\n"  "addi %[loop], %[loop], 1\n"
        "vtmv.v.t v17, %[loop]\n"  "addi %[loop], %[loop], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"

        "vtmv.v.t v18, %[loop]\n"  "addi %[loop], %[loop], 1\n"
        "vtmv.v.t v19, %[loop]\n"  "addi %[loop], %[loop], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"

        "vtmv.v.t v20, %[loop]\n"  "addi %[loop], %[loop], 1\n"
        "vtmv.v.t v21, %[loop]\n"  "addi %[loop], %[loop], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"

        "vtmv.v.t v22, %[loop]\n"  "addi %[loop], %[loop], 1\n"
        "vtmv.v.t v23, %[loop]\n"  "addi %[loop], %[loop], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "mv %[tss12], %[loop]\n"

        // The VTMV instructions above preserve the existing MAC schedule,
        // but all four result tiles are committed through row-major VTSE.
        "addi %[tss0], %[tss0], -16\n"
        "li %[loop], 16\n"
        "3:\n"
        "vtse32 %[tss0], (%[p00])\n"
        "add %[p00], %[p00], %[c_stride]\n"
        "addi %[tss0], %[tss0], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 3b\n"

        "addi %[tss4], %[tss4], -16\n"
        "li %[loop], 16\n"
        "4:\n"
        "vtse32 %[tss4], (%[p01])\n"
        "add %[p01], %[p01], %[c_stride]\n"
        "addi %[tss4], %[tss4], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 4b\n"

        "addi %[tss12], %[tss12], -16\n"
        "li %[loop], 16\n"
        "5:\n"
        "vtse32 %[tss12], (%[p11])\n"
        "add %[p11], %[p11], %[c_stride]\n"
        "addi %[tss12], %[tss12], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 5b\n"

        "addi %[blocks], %[blocks], -1\n"
        "j 7f\n"

        // Complete the fourth row-major tile store for this block.
        "7:\n"
        "li %[loop], 16\n"
        "2:\n"
        "vtse32 %[tss8], (%[p10])\n"
        "add %[p10], %[p10], %[c_stride]\n"
        "addi %[tss8], %[tss8], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 2b\n"
        "beqz %[blocks], 8f\n"

        // Retarget the scalar streams only after the old mt8 store has
        // consumed its p10/tss8 state.
        "addi %[tss0],  %[tss0],  -16\n"
        "addi %[tss4],  %[tss4],  -16\n"
        "addi %[tss8],  %[tss8],  -16\n"
        "addi %[tss12], %[tss12], -16\n"

        // Each VTSE stream advanced by 16 rows. Retarget it to the same row
        // pair and the next 32-column output block.
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "sub %[p00], %[p00], %[loop]\n"
        "sub %[p01], %[p01], %[loop]\n"
        "sub %[p10], %[p10], %[loop]\n"
        "sub %[p11], %[p11], %[loop]\n"

        "sub %[loop], %[b1p], %[b0p]\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "sub %[a1p], %[a1p], %[loop]\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "add %[b1p], %[b1p], %[loop]\n"

        // q0 currently addresses this block's mt4 bias and q1 its mt8 bias.
        // Advance both packed-bias row streams to the next 2x2 block.
        "addi %[q0], %[q0], 1024\n"
        "srli %[loop], %[loop], 1\n"
        "add %[q1], %[q1], %[loop]\n"
        "j 10b\n"
        "8:\n"

        :
          [a0p] "+r"(a0p),
          [a1p] "+r"(a1p),
          [b0p] "+r"(b0p),
          [b1p] "+r"(b1p),
          [tss0] "+r"(tss0),
          [tss4] "+r"(tss4),
          [tss8] "+r"(tss8),
          [tss12] "+r"(tss12),
          [p00] "+r"(p00),
          [p01] "+r"(p01),
          [p10] "+r"(p10),
          [p11] "+r"(p11),
          [q0] "+r"(q0),
          [q1] "+r"(q1),
          [blocks] "+r"(block_counter),
          [loop] "=&r"(loop_counter)
        :
          [c_stride] "r"(c_stride),
          [tile_dim] "r"(tile_dim),
          [tk] "r"(TK)
        : "memory");
}
