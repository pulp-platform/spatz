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

#include "matmul.h"

// Packing pads each FP16 K row to one 64-byte vector register.
// Group loads map K rows consecutively to v0..v7 (and similarly for B).
// tk=2 pairs (0,4), (1,5), (2,6), (3,7), covering all eight K rows.
// Restore e16/tk=2/twiden=2 after each raw e16,m8 load. Independent
// loads/compute/stores provide >=12 intervening instructions before use,
// including implicit sources, without overwriting live rows or adding NOPs.

__attribute__((noinline, aligned(64))) void matmul_fp16_fp32(
    const __fp16 *Apack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K)
{
    const uint32_t col_blocks = (N + BLOCK_DIM - 1) / BLOCK_DIM;
    const uint32_t row_blocks = (M + BLOCK_DIM - 1) / BLOCK_DIM;
    const uint32_t tile_stride = INPUT_ROW_ELEMENTS * K;
    uintptr_t a0p = (uintptr_t)Apack;
    uintptr_t a1p = (uintptr_t)(Apack + tile_stride);
    uintptr_t b0p = (uintptr_t)Bpack;
    uintptr_t b1p = (uintptr_t)(Bpack + tile_stride);
    uintptr_t tss;
    const uintptr_t c_tile_bytes = sizeof(float) * TE;
    uintptr_t c00 = (uintptr_t)(C);
    uintptr_t c01 = c00 + c_tile_bytes;
    uintptr_t c10 = (uintptr_t)(C + TE * CHUNK_SIZE);
    uintptr_t c11 = (uintptr_t)(C + TE * CHUNK_SIZE + TE);
    uintptr_t block_counter = (uintptr_t)row_blocks * col_blocks;
    uintptr_t col_counter = (uintptr_t)col_blocks;
    uintptr_t loop_counter;
    uintptr_t vl_counter;
    const uintptr_t c_stride = CHUNK_SIZE * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (2u << 5) | 2u;
    const uintptr_t matrix_vtype = 0xC8;  // e16, m1, ta, ma
    const uintptr_t rewind_b0_bytes = INPUT_ROW_BYTES * TE;
    const uintptr_t middle_k_groups = (uintptr_t)K / 8 - 3;

    asm volatile(
        ".Lmatmul_block_%=:\n"
        // K-Group 1: prefetch all four input groups before computing.
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        
        // vtfmm mt0
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"  // includes v4 * v20
        "vtfmm.tvv mt0, v1, v17\n"  // includes v5 * v21
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // includes v6 * v22
        "vtfmm.tvv mt0, v3, v19\n"  // includes v7 * v23
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // includes v4 * v28
        "vtfmm.tvv mt4, v1, v25\n"  // includes v5 * v29
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v8, (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // includes v6 * v30
        "vtfmm.tvv mt4, v3, v27\n"  // includes v7 * v31
        "vtzero mt8\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8, v16\n"  // includes v12 * v20
        "vtfmm.tvv mt8, v9, v17\n"  // includes v13 * v21
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v10, v18\n"  // includes v14 * v22
        "vtfmm.tvv mt8, v11, v19\n"  // includes v15 * v23
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8, v24\n"  // includes v12 * v28
        "vtfmm.tvv mt12, v9, v25\n"  // includes v13 * v29
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v10, v26\n"  // includes v14 * v30
        "vtfmm.tvv mt12, v11, v27\n"  // includes v15 * v31

        "9:\n"
        "mv %[loop], %[middle_k_groups]\n"
        "beqz %[loop], 3f\n"
        "1:\n"
        "vtfmm.tvv mt0, v0, v16\n"  // includes v4 * v20
        "vtfmm.tvv mt0, v1, v17\n"  // includes v5 * v21
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // includes v6 * v22
        "vtfmm.tvv mt0, v3, v19\n"  // includes v7 * v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // includes v4 * v28
        "vtfmm.tvv mt4, v1, v25\n"  // includes v5 * v29
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v8, (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // includes v6 * v30
        "vtfmm.tvv mt4, v3, v27\n"  // includes v7 * v31
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8, v16\n"  // includes v12 * v20
        "vtfmm.tvv mt8, v9, v17\n"  // includes v13 * v21
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v10, v18\n"  // includes v14 * v22
        "vtfmm.tvv mt8, v11, v19\n"  // includes v15 * v23

        // vtfmm mt12
        "vtfmm.tvv mt12, v8, v24\n"  // includes v12 * v28
        "vtfmm.tvv mt12, v9, v25\n"  // includes v13 * v29
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v10, v26\n"  // includes v14 * v30
        "vtfmm.tvv mt12, v11, v27\n"  // includes v15 * v31
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 1b\n"
        "3:\n"
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v8, (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // K-Group N-1 & K-Group N
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // includes v4 * v20
        "vtfmm.tvv mt0, v1, v17\n"  // includes v5 * v21
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v24, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // includes v6 * v22
        "vtfmm.tvv mt0, v3, v19\n"  // includes v7 * v23
        "vtfmm.tvv mt0, v8, v24\n"  // includes v12 * v28
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v16, (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v9, v25\n"  // includes v13 * v29
        "vtfmm.tvv mt0, v10, v26\n"  // includes v14 * v30
        "vtfmm.tvv mt0, v11, v27\n"  // includes v15 * v31
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"  // includes v4 * v20
        "vtfmm.tvv mt4, v1, v17\n"  // includes v5 * v21
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"  // includes v6 * v22
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"  // includes v7 * v23
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v0, (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"  // includes v12 * v28
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v9, v25\n"  // includes v13 * v29
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"  // includes v14 * v30
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"  // includes v15 * v31
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v8, (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n" // mt4
        "vtfmm.tvv mt12, v0, v16\n"  // includes v4 * v20
        "vtfmm.tvv mt12, v1, v17\n"  // includes v5 * v21
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2, v18\n"  // includes v6 * v22
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3, v19\n"  // includes v7 * v23
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8, v24\n"  // includes v12 * v28
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9, v25\n"  // includes v13 * v29
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "sub %[b0p], %[b0p], %[rewind_b0_bytes]\n"
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v10, v26\n"  // includes v14 * v30
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11, v27\n"  // includes v15 * v31
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v24, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0, v16\n"  // includes v4 * v20
        "vtfmm.tvv mt8, v1, v17\n"  // includes v5 * v21
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2, v18\n"  // includes v6 * v22
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3, v19\n"  // includes v7 * v23
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8, v24\n"  // includes v12 * v28
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9, v25\n"  // includes v13 * v29
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10, v26\n"  // includes v14 * v30
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11, v27\n"  // includes v15 * v31
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // Defer mt8 to the next block's compute/store overlap.  The final
        // block branches to the compact drain loop below.
        "lui %[tss], 0x40000\n"
        "addi %[blocks], %[blocks], -1\n"
        "beqz %[blocks], 7f\n"

        // Advance horizontally until the N panel is exhausted. At the row
        // boundary, advance A by two tiles and rewind B to its first pair.
        "addi %[cols], %[cols], -1\n"
        "beqz %[cols], 4f\n"
        "sub %[loop], %[b1p], %[b0p]\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "sub %[a1p], %[a1p], %[loop]\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "add %[b1p], %[b1p], %[loop]\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "j 5f\n"
        "4:\n"
        "sub %[cols], %[b1p], %[b0p]\n"
        "add %[a0p], %[a0p], %[cols]\n"
        "add %[a1p], %[a1p], %[cols]\n"
        "slli %[loop], %[col_blocks], 1\n"
        "addi %[loop], %[loop], -1\n"
        "6:\n"
        "sub %[b0p], %[b0p], %[cols]\n"
        "sub %[b1p], %[b1p], %[cols]\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 6b\n"
        "mv %[cols], %[col_blocks]\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "sub %[loop], x0, %[loop]\n"

        "5:\n"
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // Cross-Block Compute-Store overlapped (no control-flow path)
        // vtfmm mt0
        "lui %[tss], 0x40000\n"  // mt8
        "vtzero mt0\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v0, v16\n"  // includes v4 * v20
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"  // includes v5 * v21
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // includes v6 * v22
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"  // includes v7 * v23
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt4
        "vtzero mt4\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v0, v24\n"  // includes v4 * v28
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"  // includes v5 * v29
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v8, (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"  // includes v6 * v30
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v27\n"  // includes v7 * v31
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtzero mt12\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8, v24\n"  // includes v12 * v28
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9, v25\n"  // includes v13 * v29
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10, v26\n"  // includes v14 * v30
        "vtfmm.tvv mt12, v11, v27\n"  // includes v15 * v31

        // vtfmm mt8
        "vtzero mt8\n"
        "vtfmm.tvv mt8, v8, v16\n"  // includes v12 * v20
        "sub %[c00], %[c00], %[loop]\n"
        "sub %[c01], %[c01], %[loop]\n"
        "vtfmm.tvv mt8, v9, v17\n"  // includes v13 * v21
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vtfmm.tvv mt8, v10, v18\n"  // includes v14 * v22
        "vtfmm.tvv mt8, v11, v19\n"  // includes v15 * v23
        "vsetvli %[vl], x0, e16, m8, ta, ma\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "j 9b\n"

        // Final block: compact 16-row mt8 store loop.
        "7:\n"
        "li %[loop], 16\n"
        "2:\n"
        "vtse32 %[tss], (%[c10])\n"
        "add %[c10], %[c10], %[c_stride]\n"
        "addi %[tss], %[tss], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 2b\n"
        "8:\n"
        :
          [a0p] "+r"(a0p),
          [a1p] "+r"(a1p),
          [b0p] "+r"(b0p),
          [b1p] "+r"(b1p),
          [tss] "=&r"(tss),
          [c00] "+r"(c00),
          [c01] "+r"(c01),
          [c10] "+r"(c10),
          [c11] "+r"(c11),
          [blocks] "+r"(block_counter),
          [cols] "+&r"(col_counter),
          [loop] "=&r"(loop_counter),
          [vl] "=&r"(vl_counter)
        :
          [c_stride] "r"(c_stride),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [rewind_b0_bytes] "r"(rewind_b0_bytes),
          [middle_k_groups] "r"(middle_k_groups),
          [col_blocks] "r"((uintptr_t)col_blocks)
        : "memory");
}
