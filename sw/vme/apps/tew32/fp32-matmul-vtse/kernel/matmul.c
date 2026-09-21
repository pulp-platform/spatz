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

// fp32 VME GEMM kernel for TE=16, CE=8, and four accumulator tiles.


#include "matmul.h"

__attribute__((noinline, aligned(64))) void matmul_fp32(
    const float *Apack, const float *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K)
{
    const uint32_t col_blocks = (N + BLOCK_DIM - 1) / BLOCK_DIM;
    const uint32_t row_blocks = (M + BLOCK_DIM - 1) / BLOCK_DIM;
    const uint32_t tile_stride = TE * K;
    uintptr_t a0p = (uintptr_t)Apack;
    uintptr_t a1p = (uintptr_t)(Apack + tile_stride);
    uintptr_t b0p = (uintptr_t)Bpack;
    uintptr_t b1p = (uintptr_t)(Bpack + tile_stride);
    uintptr_t tss;
    const uintptr_t operand_bytes = sizeof(float) * TE;
    uintptr_t c00 = (uintptr_t)(C);
    uintptr_t c01 = c00 + operand_bytes;
    uintptr_t c10 = (uintptr_t)(C + TE * CHUNK_SIZE);
    uintptr_t c11 = (uintptr_t)(C + TE * CHUNK_SIZE + TE);
    uintptr_t block_counter = (uintptr_t)row_blocks * col_blocks;
    uintptr_t col_counter = (uintptr_t)col_blocks;
    uintptr_t loop_counter;
    const uintptr_t c_stride = CHUNK_SIZE * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (2u << 5) | 1u;
    const uintptr_t matrix_vtype = 0xD0;  // e32, m1, ta, ma
    const uintptr_t rewind_b0_bytes = operand_bytes * TE;
    const uintptr_t middle_k_groups = (uintptr_t)K / 8 - 3;

    asm volatile(
        ".Lmatmul_block_%=:\n"
        "mv %[tss], x0\n"
        // K-Group 1
        "msetmtype %[mtype], %[vtype]\n"  // set sew=16, set tm=TE, tk=2, twiden=1
        "li %[loop], 16\n"
        "msettn x0, %[loop]\n"            // msetmtype resets tn, so set full tn=TE
        // we dont need preload
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtzero mt0\n"
        
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtzero mt8\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        "9:\n"
        "mv %[loop], %[middle_k_groups]\n"
        "beqz %[loop], 3f\n"
        "1:\n"
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 1b\n"
        "3:\n"

        // K-Group N-1 & K-Group N
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v8,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v9,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v10,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v11,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vle32.v v13,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vle32.v v14,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v15,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vle32.v v16,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v15, v31\n"
        
        // vtfmm mt4  
        // vtse mt0      
        "vle32.v v18,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v0, v16\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v19,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v20,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v21,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v22,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v23,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v0,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v1,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"      
        "vtfmm.tvv mt4, v15, v31\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n"
        "vle32.v v2,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v3,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v4,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v2,  v18\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v5,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v3,  v19\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v6,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v4,  v20\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v7,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v5,  v21\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v6,  v22\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v7,  v23\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v11,  v27\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v13,  v29\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt12, v14,  v30\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt12, v15,  v31\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v0,  v16\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v2,  v18\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v3,  v19\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v4,  v20\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v5,  v21\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v6,  v22\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v7,  v23\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v8,  v24\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v10,  v26\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v11,  v27\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v12,  v28\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v13,  v29\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // Complete the block before changing any A/B/C panel pointer.  The
        // former cross-block path deferred this mt8 store while beginning the
        // next block, but its FP32 register preload was incomplete.
        "lui %[tss], 0x40000\n"
        "li %[loop], 16\n"
        ".Lstore_mt8_%=:\n"
        "vtse32 %[tss], (%[c10])\n"
        "add %[c10], %[c10], %[c_stride]\n"
        "addi %[tss], %[tss], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], .Lstore_mt8_%=\n"

        // block transition
        "vtzero mt0\n"
        "addi %[blocks], %[blocks], -1\n"
        "beqz %[blocks], 8f\n"

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
        "sub %[c00], %[c00], %[loop]\n"
        "sub %[c01], %[c01], %[loop]\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "j .Lmatmul_block_%=\n"

        // Legacy overlapped cross-block schedule retained below for assembly
        // comparison, but no control-flow path enters it.
        // vtfmm mt0
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"
        "vtzero mt4\n"

        // vtfmm mt4
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v16,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vle32.v v17,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"
        "vtzero mt8\n"

        // vtfmm mt8
        "vle32.v v18,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v19,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        "vle32.v v20,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vle32.v v21,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v22,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vle32.v v23,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13, v21\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15, v23\n"
        "vtzero mt12\n"

        // vtfmm mt12
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "mv %[tss], x0\n"
        "sub %[c00], %[c00], %[loop]\n"
        "sub %[c01], %[c01], %[loop]\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
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
          [loop] "=&r"(loop_counter)
        :
          [c_stride] "r"(c_stride),
          [operand_bytes] "r"(operand_bytes),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [rewind_b0_bytes] "r"(rewind_b0_bytes),
          [middle_k_groups] "r"(middle_k_groups),
          [col_blocks] "r"((uintptr_t)col_blocks)
        : "memory");
}
