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

// fp32 optimal GEMM kernel for TE=16, CE=8, and four accumulator tiles.


#include "matmul.h"

__attribute__((noinline, aligned(64))) void matmul_fp32(
    const float *Apack, const float *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K)
{
    // const uint32_t col_blocks = (N + BLOCK_DIM - 1) / BLOCK_DIM;
    // const uint32_t row_blocks = (M + BLOCK_DIM - 1) / BLOCK_DIM;
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
    // uintptr_t block_counter = (uintptr_t)row_blocks * col_blocks;
    // uintptr_t col_counter = (uintptr_t)col_blocks;
    uintptr_t loop_counter;
    uintptr_t vl_counter;
    const uintptr_t c_stride = CHUNK_SIZE * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (2u << 5) | 1u;
    const uintptr_t matrix_vtype = 0xD0;  // e32, m1, ta, ma

    asm volatile(
        
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        // Block 1, K-Group 1
        "vtzero mt0\n"
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtzero mt8\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 1, K-Group 2
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 1, K-Group 3
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 1, K-Group 4
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 1, K-Group 5
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 1, K-Group 6
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        
        // Block 1, K-Group 7+8
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v15, v31\n"
        
        // vtfmm mt4  
        // vtse mt0   
        "mv %[tss], x0\n"   
        "vtfmm.tvv mt4, v0, v16\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v15, v31\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v5,  v21\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v7,  v23\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "addi %[b0p], %[b0p], -1024\n"

        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n"
        "vtfmm.tvv mt8, v0,  v16\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v5,  v21\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v7,  v23\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "sub %[loop], %[b1p], %[b0p]\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "sub %[a1p], %[a1p], %[loop]\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "add %[b1p], %[b1p], %[loop]\n"
        
        // Block 2, K-Group 1
        // vtfmm mt0
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "lui %[tss], 0x40000\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v7, v23\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v31\n"
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vtzero mt8\n"

        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "sub %[c00], %[c00], %[loop]\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "sub %[c01], %[c01], %[loop]\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vtfmm.tvv mt8, v13, v21\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vtfmm.tvv mt8, v15, v23\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 2, K-Group 2
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 2, K-Group 3
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 2, K-Group 4
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 2, K-Group 5
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 2, K-Group 6
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        
        // Block 2, K-Group 7+8
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vtfmm.tvv mt0, v15, v31\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vtfmm.tvv mt0, v14, v30\n"
        
        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n"
        "vtfmm.tvv mt4, v0, v16\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v15, v31\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v5,  v21\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v7,  v23\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n"
        "vtfmm.tvv mt8, v0,  v16\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v5,  v21\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v7,  v23\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // Drain Block 2 mt8 alongside Block 3's other tiles.

        "sub %[loop], %[b1p], %[b0p]\n"
        "add %[a0p], %[a0p], %[loop]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "add %[a1p], %[a1p], %[loop]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "sub %[b0p], %[b0p], %[loop]\n"
        "sub %[b1p], %[b1p], %[loop]\n"
        "sub %[b0p], %[b0p], %[loop]\n"
        "sub %[b1p], %[b1p], %[loop]\n"
        "sub %[b0p], %[b0p], %[loop]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "sub %[b1p], %[b1p], %[loop]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "sub %[loop], x0, %[loop]\n"
        "sub %[c00], %[c00], %[loop]\n"
        "sub %[c01], %[c01], %[loop]\n"
        // c10 still addresses Block 2 mt8 until its deferred store finishes.
        "sub %[c11], %[c11], %[loop]\n"

        "lui %[tss], 0x40000\n"
        // Block 3, K-Group 1
        "vtzero mt0\n"
        
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtzero mt12\n"
        
        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "sub %[c10], %[c10], %[loop]\n"
        "vtzero mt8\n"
        // Block 3 mt8 can begin only after the old mt8 is fully stored.
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10, v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11, v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12, v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13, v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14, v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15, v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 3, K-Group 2
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 3, K-Group 3
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 3, K-Group 4
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 3, K-Group 5
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 3, K-Group 6
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        
        // Block 3, K-Group 7+8
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "nop\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v15, v31\n"
        
        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "vtfmm.tvv mt4, v0, v16\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v15, v31\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v5,  v21\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v7,  v23\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n"
        "vtfmm.tvv mt8, v0,  v16\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v5,  v21\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v7,  v23\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // Defer the old block's mt8 store and interleave its 16 beats with
        // Block 4 K-group 1, matching the Block 1 -> Block 2 schedule.

        // // block transition
        "vtzero mt0\n"
        // "addi %[blocks], %[blocks], -1\n"
        // "beqz %[blocks], 8f\n"

        // // Advance horizontally until the N panel is exhausted. At the row
        // // boundary, advance A by two tiles and rewind B to its first pair.
        // "addi %[cols], %[cols], -1\n"
        // "beqz %[cols], 4f\n"
        "sub %[loop], %[b1p], %[b0p]\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "sub %[a1p], %[a1p], %[loop]\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "add %[b1p], %[b1p], %[loop]\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"

        "mv %[tss], x0\n"
        // Block 4, K-Group 1: mirror the Block 2 handoff schedule.
        // The prior K-group has already prepared v1..v7 and v17..v23.
        // vtfmm mt0
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtzero mt0\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "lui %[tss], 0x40000\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v7, v23\n"
        "vtzero mt4\n"

        // vtfmm mt4
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v31\n"
        "vtzero mt12\n"

        // vtfmm mt12; the last four deferred mt8 stores happen here.
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "sub %[c00], %[c00], %[loop]\n"
        "sub %[c01], %[c01], %[loop]\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vtzero mt8\n"

        // vtfmm mt8: eight FMAs follow the final deferred store.
        "vtfmm.tvv mt8, v8,  v16\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vtfmm.tvv mt8, v13, v21\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vtfmm.tvv mt8, v15, v23\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 4, K-Group 2
        // vtfmm mt0
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 4, K-Group 3
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 4, K-Group 4
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 4, K-Group 5
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Block 4, K-Group 6
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 4, K-Group 7+8
        "mv %[tss], x0\n"
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a0p])\n" "addi %[a0p], %[a0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "nop\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v15, v31\n"
        
        // vtfmm mt4  
        // vtse mt0      
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "vtfmm.tvv mt4, v0, v16\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "addi %[b1p], %[b1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v15, v31\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "addi %[a1p], %[a1p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v5,  v21\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v7,  v23\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n"
        "vtfmm.tvv mt8, v0,  v16\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "addi %[b0p], %[b0p], 512\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v5,  v21\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v7,  v23\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // Final block: compact 16-row mt8 store loop.
        // "7:\n"
        // "li %[loop], 16\n"
        // "2:\n"
        "lui %[tss], 0x40000\n"
        "nop\n"
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
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // "addi %[loop], %[loop], -1\n"
        // "bnez %[loop], 2b\n"
        // "8:\n"

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
          // [blocks] "+r"(block_counter),
          // [cols] "+&r"(col_counter),
          [loop] "=&r"(loop_counter),
          [vl] "=&r"(vl_counter)
        :
          [c_stride] "r"(c_stride),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype)
          // [rewind_b0_bytes] "r"(rewind_b0_bytes),
          // [middle_k_groups] "r"(middle_k_groups),
          // [col_blocks] "r"((uintptr_t)col_blocks)
        : "memory");
}
