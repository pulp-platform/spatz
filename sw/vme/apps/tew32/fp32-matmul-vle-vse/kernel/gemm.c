
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

    asm volatile(

        // Every block enters the same bias-load and first-K-group schedule.
        "10:\n"
        // K-Group 0
        "msetmtypei 1, 2\n"
        "msettn x0, %[tile_dim]\n"
        "msettm x0, %[tile_dim]\n"
        "msettk x0, %[tk]\n"
        "vsetvli %[loop], x0, e32, m4, ta, ma\n"
        // Load all 16 rows of mt0 before its first MAC. Each LMUL=4 load
        // supplies four physical vector registers, hence four loads per tile.
        "vle32.v v24, (%[p00])\n" "addi %[p00], %[p00], 256\n"
        "vle32.v v28, (%[p00])\n" "addi %[p00], %[p00], 256\n"
        "vtmv.t.v %[tss0], v24\n" "addi %[tss0], %[tss0], 4\n"
        "vtmv.t.v %[tss0], v28\n" "addi %[tss0], %[tss0], 4\n"
        "vle32.v v8, (%[p00])\n"  "addi %[p00], %[p00], 256\n"
        "vle32.v v12, (%[p00])\n" "addi %[p00], %[p00], -768\n"
        "vtmv.t.v %[tss0], v8\n"  "addi %[tss0], %[tss0], 4\n"
        "vtmv.t.v %[tss0], v12\n" "addi %[tss0], %[tss0], -12\n"

        // A0 is reused by mt0 and mt4. B0 remains at its current address
        // because v16..v23 must be restored after loading the mt8 bias.
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 256\n"
        "vle32.v v8,  (%[a0p])\n" "addi %[a0p], %[a0p], 256\n"
        "vle32.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "vle32.v v20, (%[b0p])\n"

        // mt0 compute || load and install mt4 bias.
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v24, (%[p01])\n" "addi %[p01], %[p01], 256\n"
        "vle32.v v28, (%[p01])\n" "addi %[p01], %[p01], 256\n"
        "vtmv.t.v %[tss4], v24\n" "addi %[tss4], %[tss4], 4\n"
        "vtmv.t.v %[tss4], v28\n" "addi %[tss4], %[tss4], 4\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vle32.v v8, (%[p01])\n" "addi %[p01], %[p01], 256\n"
        "vle32.v v12, (%[p01])\n" "addi %[p01], %[p01], -768\n"
        "vtmv.t.v %[tss4], v8\n"  "addi %[tss4], %[tss4], 4\n"
        "vtmv.t.v %[tss4], v12\n" "addi %[tss4], %[tss4], -12\n"

        // Replace the consumed mt4-bias group with B1 exactly once. B1 is
        // then reused by mt4 and mt12.
        "vle32.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 256\n"
        "vle32.v v28, (%[b1p])\n" "addi %[b1p], %[b1p], 256\n"

        // mt4 compute || seed mt12 in v8..v15 and v16..v23.
        "vle32.v v8, (%[p11])\n" "addi %[p11], %[p11], 256\n"
        "vle32.v v12, (%[p11])\n" "addi %[p11], %[p11], 256\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtmv.t.v %[tss12], v8\n"  "addi %[tss12], %[tss12], 4\n"
        "vle32.v v16, (%[p11])\n" "addi %[p11], %[p11], 256\n"
        "vtmv.t.v %[tss12], v12\n" "addi %[tss12], %[tss12], 4\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vtmv.t.v %[tss12], v16\n" "addi %[tss12], %[tss12], 4\n"
        "vle32.v v20, (%[p11])\n" "addi %[p11], %[p11], -768\n"
        "vle32.v v8, (%[a1p])\n" "addi %[a1p], %[a1p], 256\n"
        "vle32.v v12, (%[a1p])\n" "addi %[a1p], %[a1p], 256\n"
        "vtmv.t.v %[tss12], v20\n" "addi %[tss12], %[tss12], -12\n"

        // mt12 compute || seed mt8 in v16..v23 and v0..v7.
        "vle32.v v16, (%[p10])\n" "addi %[p10], %[p10], 256\n"
        "vle32.v v20, (%[p10])\n" "addi %[p10], %[p10], 256\n"
        "vtfmm.tvv mt12, v8, v24\n"
        "vtmv.t.v %[tss8], v16\n" "addi %[tss8], %[tss8], 4\n"
        "vle32.v v0, (%[p10])\n" "addi %[p10], %[p10], 256\n"
        "vtmv.t.v %[tss8], v20\n" "addi %[tss8], %[tss8], 4\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtmv.t.v %[tss8], v0\n" "addi %[tss8], %[tss8], 4\n"
        "vle32.v v4, (%[p10])\n" "addi %[p10], %[p10], -768\n"
        "vle32.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "vle32.v v20, (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "vtmv.t.v %[tss8], v4\n" "addi %[tss8], %[tss8], -12\n"

        // mt8 compute. A0 may be preloaded because mt8 reads A1 from v8..v15.
        // Keep B0 intact until the final v15/v23 consumer has issued.
        "vtfmm.tvv mt8, v8,  v16\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 256\n"
        "vle32.v v4,  (%[a0p])\n" "addi %[a0p], %[a0p], 256\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "vle32.v v20,  (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"

        // K-Group 1 to N-3
        // vle B1
        // vtfmm mt0, A0, B0
        "9:\n"
        "li %[loop], 5\n"
        "1:\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 256\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vle32.v v28,  (%[b1p])\n" "addi %[b1p], %[b1p], 256\n"

        // vle A1
        // vtfmm mt4, A0, B1
        "vtfmm.tvv mt4, v0, v24\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 256\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vle32.v v12,  (%[a1p])\n" "addi %[a1p], %[a1p], 256\n"

        // preload vle A0
        // vtfmm mt8, A1, B0
        "vtfmm.tvv mt8, v8,  v16\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 256\n"
        "vtfmm.tvv mt8, v12,  v20\n"
        "vle32.v v4,  (%[a0p])\n" "addi %[a0p], %[a0p], 256\n"

        // preload vle B0
        // vtfmm mt12, A1, B1
        "addi %[loop], %[loop], -1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v20,  (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "bnez %[loop], 1b\n"
        // "3:\n"

        // K-Group K-2 & K-Group K-1
        // vle B0'
        // vle A0'
        // vtfmm mt0, A0, B0
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "vle32.v v28,  (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vle32.v v8,  (%[a0p])\n" "addi %[a0p], %[a0p], 256\n"
        "vle32.v v12,  (%[a0p])\n" "addi %[a0p], %[a0p], 256\n"

        // vle B1
        // vtfmm mt0, A0', B0'
        "vtfmm.tvv mt0, v8, v24\n"
        "vle32.v v16,  (%[b1p])\n" "addi %[b1p], %[b1p], 256\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vle32.v v20,  (%[b1p])\n" "addi %[b1p], %[b1p], 256\n"

        // vle B1'
        // vtfmm mt4, A0, B1
        "vtfmm.tvv mt4, v0, v16\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 256\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vle32.v v28,  (%[b1p])\n" "addi %[b1p], %[b1p], 256\n"

        // vle A1
        // vtfmm mt4, A0', B1' || vtmv mt0 (row0 - row7) + vse
        "vtmv.v.t v0, %[tss0]\n"  "addi %[tss0], %[tss0], 4\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtmv.v.t v4, %[tss0]\n"  "addi %[tss0], %[tss0], 4\n"

        "vse32.v v0, (%[p00])\n" "addi %[p00], %[p00], 256\n"
        "vse32.v v4, (%[p00])\n" "addi %[p00], %[p00], 256\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vle32.v v0,  (%[a1p])\n" "addi %[a1p], %[a1p], 256\n"
        "vle32.v v4,  (%[a1p])\n" "addi %[a1p], %[a1p], 256\n"

        // vle A1'
        // vtfmm mt12, A1, B1 || vtmv mt0 (row8 - row15) + vse
        "vtmv.v.t v8,  %[tss0]\n" "addi %[tss0], %[tss0], 4\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtmv.v.t v12, %[tss0]\n" "addi %[tss0], %[tss0], 4\n"

        "vse32.v v8, (%[p00])\n" "addi %[p00], %[p00], 256\n"
        "vse32.v v12, (%[p00])\n" "addi %[p00], %[p00], 1280\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 256\n"
        "vle32.v v12,  (%[a1p])\n" "addi %[a1p], %[a1p], 256\n"

        // vle reload B0 (restore B0 pointer)
        // vtfmm mt12, A1', B1' || vtmv mt4 (row0 - row7) + vse
        "vtmv.v.t v16, %[tss4]\n" "addi %[tss4], %[tss4], 4\n"
        "vse32.v v16, (%[p01])\n" "addi %[p01], %[p01], 256\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtmv.v.t v20, %[tss4]\n" "addi %[tss4], %[tss4], 4\n"
        "vse32.v v20, (%[p01])\n" "addi %[p01], %[p01], 256\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vle32.v v16,  (%[b0p])\n"  "addi %[b0p], %[b0p], 256\n"
        "vle32.v v20,  (%[b0p])\n"  "addi %[b0p], %[b0p], 256\n"

        // vle reload B0'
        // vtfmm mt8, A1, B0 || vtmv mt4 (row8 - row15) + vse
        "vtmv.v.t v24, %[tss4]\n" "addi %[tss4], %[tss4], 4\n"
        "vtfmm.tvv mt8, v0,  v16\n"
        "vtmv.v.t v28, %[tss4]\n" "addi %[tss4], %[tss4], 4\n"
        "vse32.v v24, (%[p01])\n"  "addi %[p01], %[p01], 256\n"
        "vse32.v v28, (%[p01])\n"  "addi %[p01], %[p01], 1280\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"
        "vle32.v v28,  (%[b0p])\n" "addi %[b0p], %[b0p], 256\n"

        // vtfmm mt8, A1', B0' || vtmv mt12 (row0 - row7) + vse
        // vtmv mt12 (row8 - row15) + vse
        "addi %[loop], %[tss12], 8\n"

        "vtmv.v.t v0, %[tss12]\n" "addi %[tss12], %[tss12], 4\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vtmv.v.t v4, %[tss12]\n" "addi %[tss12], %[tss12], 4\n"

        "vse32.v v0, (%[p11])\n" "addi %[p11], %[p11], 256\n"
        "vse32.v v4, (%[p11])\n" "addi %[p11], %[p11], 256\n"
        "vtmv.v.t v16, %[loop]\n"  "addi %[loop], %[loop], 4\n"
        "vtmv.v.t v20, %[loop]\n"  "addi %[loop], %[loop], 4\n"
        "vse32.v v16, (%[p11])\n" "addi %[p11], %[p11], 256\n"
        "vse32.v v20, (%[p11])\n" "addi %[p11], %[p11], 1280\n"
        "mv %[tss12], %[loop]\n"
        "addi %[blocks], %[blocks], -1\n"
        "j 7f\n"

        // Store mt8 for every block. Non-final blocks then return to the
        // shared bias-load prologue above.
        "7:\n"

        "vtmv.v.t v0, %[tss8]\n"  "addi %[tss8], %[tss8], 4\n"
        "vtmv.v.t v4, %[tss8]\n"  "addi %[tss8], %[tss8], 4\n"

        "vse32.v v0, (%[p10])\n" "addi %[p10], %[p10], 256\n"
        "vse32.v v4, (%[p10])\n" "addi %[p10], %[p10], 256\n"
        "vtmv.v.t v8,  %[tss8]\n" "addi %[tss8], %[tss8], 4\n"
        "vtmv.v.t v12, %[tss8]\n" "addi %[tss8], %[tss8], 4\n"
        "vse32.v v8, (%[p10])\n" "addi %[p10], %[p10], 256\n"
        "vse32.v v12, (%[p10])\n" "addi %[p10], %[p10], 1280\n"
        "beqz %[blocks], 8f\n"

        // Retarget the scalar streams only after the old mt8 store has
        // consumed its p10/tss8 state.
        "addi %[tss0],  %[tss0],  -16\n"
        "addi %[tss4],  %[tss4],  -16\n"
        "addi %[tss8],  %[tss8],  -16\n"
        "addi %[tss12], %[tss12], -16\n"
        "sub %[loop], %[b1p], %[b0p]\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "sub %[a1p], %[a1p], %[loop]\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "add %[b1p], %[b1p], %[loop]\n"
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
          [blocks] "+r"(block_counter),
          [loop] "=&r"(loop_counter)
        :
          [tile_dim] "r"(tile_dim),
          [tk] "r"(TK)
        : "memory");
}
