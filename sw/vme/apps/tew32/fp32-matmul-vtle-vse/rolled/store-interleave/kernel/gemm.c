
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
    float *C, const float *Apack, const float *Bpack,
    uint32_t ti_lo, uint32_t ti_hi)
{
    const int ti = (int)ti_lo;
    const int row_blocks = (int)gemm_l.M / 64;
    const int col_blocks = (int)gemm_l.N / 32;
    const int num_blocks = row_blocks * col_blocks;
    const int col_tiles = (int)gemm_l.N / 16;       // TILE_DIM
    const int tile_stride = 16 * (int)gemm_l.K;

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
    uintptr_t p00 = (uintptr_t)(C + ti * col_tiles * 16 * 16);
    uintptr_t p01 = p00 + 16 * 16 * sizeof(float);
    uintptr_t p10 = (uintptr_t)(C + (ti + 1) * col_tiles * 16 * 16);
    uintptr_t p11 = p10 + 16 * 16 * sizeof(float);
    const uintptr_t tile_dim = 16;
    const uintptr_t TK = 8;
    uintptr_t block_counter = (uintptr_t)num_blocks;
    uintptr_t loop_counter;
    uintptr_t vtle_counter;

    asm volatile(

        // First Block
        // K-Group 0
        // vle A0
        // vle B0
        "msetmtypei 1, 2\n"
        "msettn x0, %[tile_dim]\n"
        "vsetvli %[loop], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n"
        "vle32.v v16,  (%[b0p])\n"
        "msettm x0, %[tile_dim]\n"
        "msettk x0, %[tk]\n"
        "li %[vtle_loop], 16\n"
        ".Lvtle_first_mt0_%=:\n"
        "vtle32 %[tss0], (%[p00])\n"
        "addi %[p00], %[p00], 64\n"
        "addi %[tss0], %[tss0], 1\n"
        "addi %[vtle_loop], %[vtle_loop], -1\n"
        "bnez %[vtle_loop], .Lvtle_first_mt0_%=\n"
        "addi %[p00], %[p00], -1024\n"
        "addi %[tss0], %[tss0], -16\n"

        // vle B1
        // vtfmm mt0, A0, B0
        "vle32.v v24,  (%[b1p])\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "addi %[a0p], %[a0p], 512\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "addi %[b1p], %[b1p], 512\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "li %[vtle_loop], 16\n"
        ".Lvtle_first_mt4_%=:\n"
        "vtle32 %[tss4], (%[p01])\n"
        "addi %[p01], %[p01], 64\n"
        "addi %[tss4], %[tss4], 1\n"
        "addi %[vtle_loop], %[vtle_loop], -1\n"
        "bnez %[vtle_loop], .Lvtle_first_mt4_%=\n"
        "addi %[p01], %[p01], -1024\n"
        "addi %[tss4], %[tss4], -16\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vtfmm.tvv mt0, v7, v23\n"

        // vle A1
        // vtfmm mt4, A0, B1
        "vle32.v v8,  (%[a1p])\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "addi %[a1p], %[a1p], 512\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "li %[vtle_loop], 16\n"
        ".Lvtle_first_mt8_%=:\n"
        "vtle32 %[tss8], (%[p10])\n"
        "addi %[p10], %[p10], 64\n"
        "addi %[tss8], %[tss8], 1\n"
        "addi %[vtle_loop], %[vtle_loop], -1\n"
        "bnez %[vtle_loop], .Lvtle_first_mt8_%=\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "addi %[p10], %[p10], -1024\n"
        "addi %[tss8], %[tss8], -16\n"
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
        "li %[vtle_loop], 16\n"
        ".Lvtle_first_mt12_%=:\n"
        "vtle32 %[tss12], (%[p11])\n"
        "addi %[p11], %[p11], 64\n"
        "addi %[tss12], %[tss12], 1\n"
        "addi %[vtle_loop], %[vtle_loop], -1\n"
        "bnez %[vtle_loop], .Lvtle_first_mt12_%=\n"
        "addi %[p11], %[p11], -1024\n"
        "addi %[tss12], %[tss12], -16\n"
        "vtfmm.tvv mt8, v14,  v22\n"
        "vtfmm.tvv mt8, v15,  v23\n"

        // preload vle B0
        // vtfmm mt12, A1, B1
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
        // vtfmm mt4, A0', B1' || vtmv mt0 (row0 - row7) + vse
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

        "vse32.v v0, (%[p00])\n" "addi %[p00], %[p00], 512\n"
        "vtfmm.tvv mt4, v12, v28\n"

        "vtfmm.tvv mt4, v13, v29\n"

        "vtfmm.tvv mt4, v14, v30\n"

        "vle32.v v0,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "vtfmm.tvv mt4, v15, v31\n"

        // vle A1'
        // vtfmm mt12, A1, B1 || vtmv mt0 (row8 - row15) + vse
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

        "vse32.v v8, (%[p00])\n" "addi %[p00], %[p00], 1536\n"
        "vtfmm.tvv mt12, v4,  v20\n"

        "vtfmm.tvv mt12, v5,  v21\n"

        "vtfmm.tvv mt12, v6,  v22\n"

        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "vtfmm.tvv mt12, v7,  v23\n"

        // vle reload B0 (restore B0 pointer)
        // vtfmm mt12, A1', B1' || vtmv mt4 (row0 - row7) + vse
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

        "vse32.v v16, (%[p01])\n" "addi %[p01], %[p01], 512\n"
        "vtfmm.tvv mt12, v12,  v28\n"

        "vtfmm.tvv mt12, v13,  v29\n"
        "addi %[b0p], %[b0p], -1024\n"

        "vtfmm.tvv mt12, v14,  v30\n"

        "vle32.v v16,  (%[b0p])\n"  "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt12, v15,  v31\n"

        // vle reload B0'
        // vtfmm mt8, A1, B0 || vtmv mt4 (row8 - row15) + vse
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

        "vse32.v v24, (%[p01])\n"  "addi %[p01], %[p01], 1536\n"
        "vtfmm.tvv mt8, v4,  v20\n"

        "vtfmm.tvv mt8, v5,  v21\n"

        "vtfmm.tvv mt8, v6,  v22\n"

        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt8, v7,  v23\n"

        // vtfmm mt8, A1', B0' || vtmv mt12 (row0 - row7) + vse
        // vtmv mt12 (row8 - row15) + vse
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

        "vse32.v v0, (%[p11])\n" "addi %[p11], %[p11], 512\n"
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
        "vse32.v v16, (%[p11])\n" "addi %[p11], %[p11], 1536\n"
        "mv %[tss12], %[loop]\n"

        "addi %[blocks], %[blocks], -1\n"
        "beqz %[blocks], 7f\n"

        "addi %[tss0], %[tss0], -16\n"
        "li %[vtle_loop], 16\n"
        ".Lvtle_next_mt0_%=:\n"
        "vtle32 %[tss0], (%[p00])\n"
        "addi %[p00], %[p00], 64\n"
        "addi %[tss0], %[tss0], 1\n"
        "addi %[vtle_loop], %[vtle_loop], -1\n"
        "bnez %[vtle_loop], .Lvtle_next_mt0_%=\n"
        "addi %[p00], %[p00], -1024\n"
        "addi %[tss0], %[tss0], -16\n"
        
        // Next Block
        // K-Group 0
        // vle A0
        // vle B0
        "sub %[loop], %[b1p], %[b0p]\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "vle32.v v0,  (%[a0p])\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "vle32.v v16,  (%[b0p])\n"

        // vtmv mt8 (row0 - row7) + vse || next block vtfmm mt0, A0, B0
        // vle B1
        "vtmv.v.t v8,  %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v9,  %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt0, v0, v16\n"

        "vtmv.v.t v10, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v11, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"

        "vtmv.v.t v12, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v13, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"

        "vtmv.v.t v14, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v15, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"

        "addi %[tss4], %[tss4], -16\n"
        "li %[vtle_loop], 16\n"
        ".Lvtle_next_mt4_%=:\n"
        "vtle32 %[tss4], (%[p01])\n"
        "addi %[p01], %[p01], 64\n"
        "addi %[tss4], %[tss4], 1\n"
        "addi %[vtle_loop], %[vtle_loop], -1\n"
        "bnez %[vtle_loop], .Lvtle_next_mt4_%=\n"
        "addi %[p01], %[p01], -1024\n"
        "addi %[tss4], %[tss4], -16\n"
        "vse32.v v8, (%[p10])\n"  "addi %[p10], %[p10], 512\n"
        "vtfmm.tvv mt0, v4, v20\n"

        "vtfmm.tvv mt0, v5, v21\n"
        "add %[b1p], %[b1p], %[loop]\n"
        "vtfmm.tvv mt0, v6, v22\n"
        
        "vle32.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "vtfmm.tvv mt0, v7, v23\n"

        // vtmv mt8 (row8 - row15) + vse || next block vtfmm mt4, A0, B1
        // vle A1
        "vtmv.v.t v8,  %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v9,  %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt4, v0, v24\n"

        "vtmv.v.t v10, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v11, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        
        "vtmv.v.t v12, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v13, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"

        "vtmv.v.t v14, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v15, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt4, v3, v27\n"

        "vse32.v v8, (%[p10])\n"  "addi %[p10], %[p10], 1536\n"
        "vtfmm.tvv mt4, v4, v28\n"

        "sub %[a1p], %[a1p], %[loop]\n"
        "vtfmm.tvv mt4, v5, v29\n"

        "vtfmm.tvv mt4, v6, v30\n"
        "vle32.v v8, (%[a1p])\n"  "addi %[a1p], %[a1p], 512\n"

        // preload vle A0
        // preload vle B0
        // next block vtfmm mt8, A1, B0
        "vtfmm.tvv mt4, v7, v31\n"     
        "addi %[a0p], %[a0p], 512\n"
        "vle32.v v0,  (%[a0p])\n"
        "addi %[tss8], %[tss8], -16\n"
        "li %[vtle_loop], 16\n"
        ".Lvtle_next_mt8_%=:\n"
        "vtle32 %[tss8], (%[p10])\n"
        "addi %[p10], %[p10], 64\n"
        "addi %[tss8], %[tss8], 1\n"
        "addi %[vtle_loop], %[vtle_loop], -1\n"
        "bnez %[vtle_loop], .Lvtle_next_mt8_%=\n"
        "addi %[p10], %[p10], -1024\n"
        "addi %[tss8], %[tss8], -16\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        "addi %[a0p], %[a0p], 512\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "addi %[tss12], %[tss12], -16\n"
        "li %[vtle_loop], 16\n"
        ".Lvtle_next_mt12_%=:\n"
        "vtle32 %[tss12], (%[p11])\n"
        "addi %[p11], %[p11], 64\n"
        "addi %[tss12], %[tss12], 1\n"
        "addi %[vtle_loop], %[vtle_loop], -1\n"
        "bnez %[vtle_loop], .Lvtle_next_mt12_%=\n"
        "addi %[p11], %[p11], -1024\n"
        "addi %[tss12], %[tss12], -16\n"
        "vtfmm.tvv mt8, v10,  v18\n"
        "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt8, v11,  v19\n"
        "vtfmm.tvv mt8, v12,  v20\n"
        "vtfmm.tvv mt8, v13,  v21\n"
        "vle32.v v16,  (%[b0p])\n"
        "vtfmm.tvv mt8, v14,  v22\n"
        "addi %[b0p], %[b0p], 512\n"
        "vtfmm.tvv mt8, v15,  v23\n"

        // next block vtfmm mt12, A1, B1
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "j 9b\n"

        // Final block: compact 16-row mt8 store loop.
        "7:\n"

        "vtmv.v.t v0, %[tss8]\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v1, %[tss8]\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v2, %[tss8]\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v3, %[tss8]\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v4, %[tss8]\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v5, %[tss8]\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v6, %[tss8]\n"  "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v7, %[tss8]\n"  "addi %[tss8], %[tss8], 1\n"

        "vse32.v v0, (%[p10])\n"
        "addi %[p10], %[p10], 512\n"
        "vtmv.v.t v8,  %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v9,  %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v10, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v11, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v12, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v13, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v14, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v15, %[tss8]\n" "addi %[tss8], %[tss8], 1\n"

        "vse32.v v8, (%[p10])\n"
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
          [blocks] "+r"(block_counter),
          [loop] "=&r"(loop_counter),
          [vtle_loop] "=&r"(vtle_counter)
        :
          [tile_dim] "r"(tile_dim),
          [tk] "r"(TK)
        : "memory");
}
