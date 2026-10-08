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

// fp32 optimal kernel for TE=32, CE=8, and four accumulator tiles.

#include "matmul.h"

__attribute__((noinline, aligned(64))) void matmul_fp32(const float *At, const float *B, float *C, uint32_t M, uint32_t N, uint32_t K)
{
    const uint32_t col_blocks = (N + BLOCK_DIM - 1) / BLOCK_DIM;
    const uint32_t row_blocks = (M + BLOCK_DIM - 1) / BLOCK_DIM;
    const uint32_t tile_stride = TE * K;
    uintptr_t a0p = (uintptr_t)At;
    uintptr_t a1p = (uintptr_t)(At + tile_stride);
    uintptr_t b0p = (uintptr_t)B;
    uintptr_t b1p = (uintptr_t)(B + tile_stride);
    uintptr_t tss;
    const uintptr_t operand_bytes = sizeof(float) * TE;
    uintptr_t c00 = (uintptr_t)(C);
    uintptr_t c01 = c00 + operand_bytes;
    uintptr_t c10 = (uintptr_t)(C + TE * CHUNK_SIZE);
    uintptr_t c11 = (uintptr_t)(C + TE * CHUNK_SIZE + TE);
    uintptr_t block_counter = (uintptr_t)row_blocks * col_blocks;
    uintptr_t col_counter = (uintptr_t)col_blocks;
    uintptr_t loop_counter;
    uintptr_t store_counter;
    const uintptr_t c_stride = CHUNK_SIZE * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (1u << 5) | 1u;
    const uintptr_t matrix_vtype = 0xD1;  // e32, m2, ta, ma
    const uintptr_t rewind_b0_bytes = 8 * operand_bytes;
    const uintptr_t tile_bytes = operand_bytes * K;
    const uintptr_t store_rows_bytes = TE * c_stride;
    const uintptr_t col_c_back = store_rows_bytes - 2 * operand_bytes;
    const uintptr_t row_c_advance =
        store_rows_bytes - (col_blocks - 1) * 2 * operand_bytes;
    const uintptr_t row_b_rewind = (2 * col_blocks - 1) * tile_bytes;
    const uintptr_t middle_k_groups = (uintptr_t)K / 4 - 3;

    asm volatile(
        ".Lmatmul_block_%=:\n"
        // K-Group 1
        "msetmtype %[mtype], %[vtype]\n"  // set sew=32, set tm=TE, tk=2, twiden=1
        "li %[loop], 32\n"
        "msettn x0, %[loop]\n"            // msetmtype resets tn, so set full tn=TE

        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtzero mt0\n"
        
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtzero mt8\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"

        "9:\n"
        "mv %[loop], %[middle_k_groups]\n"
        "beqz %[loop], 3f\n"
        "1:\n"
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 1b\n"
        "3:\n"

        // K-Group N-1 & K-Group N
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v8,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v10,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vle32.v v12,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vle32.v v14,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vle32.v v16,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v0,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v0,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v0,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v0,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v2,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v2,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v2,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v2,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v4,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v4,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v4,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v4,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v6,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v6,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v6,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v6,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v0,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v8,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v8,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v8,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v8,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v10,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v10,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v10,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v10,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"

        // Finish mt0 rows 28-31 with the already released v12 group. Avoid
        // reusing v14 immediately after it is consumed by the final mt4 MAC.
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "sub %[b0p], %[b0p], %[rewind_b0_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0,  v16\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vtfmm.tvv mt8, v14,  v30\n"

        // Delay the right-half stores until mt12/mt8 have consumed every
        // B-side register group. This avoids overwriting live LMUL=2 operands.
        "lui %[tss], 0x20000\n"  // mt4 -> c01
        "li %[store], 32\n"
        ".Lstore_mt4_%=:\n"
        "vtmv.v.t v0, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v0, (%[c01])\n"
        "add %[c01], %[c01], %[c_stride]\n"
        "addi %[store], %[store], -1\n"
        "bnez %[store], .Lstore_mt4_%=\n"

        "lui %[tss], 0x60000\n"  // mt12 -> c11
        "li %[store], 32\n"
        ".Lstore_mt12_%=:\n"
        "vtmv.v.t v0, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v0, (%[c11])\n"
        "add %[c11], %[c11], %[c_stride]\n"
        "addi %[store], %[store], -1\n"
        "bnez %[store], .Lstore_mt12_%=\n"

        // Defer mt8 to the next block's compute/store overlap. The final
        // block branches to the VSE drain loop below.
        "lui %[tss], 0x40000\n"
        "addi %[blocks], %[blocks], -1\n"
        "beqz %[blocks], 7f\n"

        // Advance horizontally until the N panel is exhausted. At the row
        // boundary, advance A by two tiles and rewind B to its first pair.
        "addi %[cols], %[cols], -1\n"
        "beqz %[cols], 4f\n"
        "sub %[a0p], %[a0p], %[tile_bytes]\n"
        "sub %[a1p], %[a1p], %[tile_bytes]\n"
        "add %[b0p], %[b0p], %[tile_bytes]\n"
        "add %[b1p], %[b1p], %[tile_bytes]\n"
        "mv %[loop], %[col_c_back]\n"
        "j 5f\n"
        "4:\n"
        "mv %[cols], %[col_blocks]\n"
        "add %[a0p], %[a0p], %[tile_bytes]\n"
        "add %[a1p], %[a1p], %[tile_bytes]\n"
        "sub %[b0p], %[b0p], %[row_b_rewind]\n"
        "sub %[b1p], %[b1p], %[row_b_rewind]\n"
        "sub %[loop], x0, %[row_c_advance]\n"

        "5:\n"

        // Cross-block compute/store overlap. Compute the first K group of
        // the next block while draining the preceding block's mt8.
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        "li %[store], 32\n"
        ".Lstore_prev_mt8_%=:\n"
        "vtmv.v.t v8, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v8, (%[c10])\n"
        "add %[c10], %[c10], %[c_stride]\n"
        "addi %[store], %[store], -1\n"
        "bnez %[store], .Lstore_prev_mt8_%=\n"

        "vtzero mt4\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"

        "vtzero mt12\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"

        "vtzero mt8\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14, v22\n"

        // All four C pointers have now advanced by exactly 32 rows.
        "sub %[c00], %[c00], %[loop]\n"
        "sub %[c01], %[c01], %[loop]\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "j 9b\n"

        // Final block: drain the deferred mt8 through legal LMUL=2 VSE.
        "7:\n"
        "li %[store], 32\n"
        ".Lstore_final_mt8_%=:\n"
        "vtmv.v.t v0, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v0, (%[c10])\n"
        "add %[c10], %[c10], %[c_stride]\n"
        "addi %[store], %[store], -1\n"
        "bnez %[store], .Lstore_final_mt8_%=\n"
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
          [store] "=&r"(store_counter)
        :
          [c_stride] "r"(c_stride),
          [operand_bytes] "r"(operand_bytes),
          [tile_bytes] "r"(tile_bytes),
          [col_c_back] "r"(col_c_back),
          [row_c_advance] "r"(row_c_advance),
          [row_b_rewind] "r"(row_b_rewind),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [rewind_b0_bytes] "r"(rewind_b0_bytes),
          [middle_k_groups] "r"(middle_k_groups),
          [col_blocks] "r"((uintptr_t)col_blocks)
        : "memory");
}
