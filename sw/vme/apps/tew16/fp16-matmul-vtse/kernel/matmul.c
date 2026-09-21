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

// fp16 VME GEMM kernel for TE=16, CE=8, and four accumulator tiles.

#include "matmul.h"

__attribute__((noinline, aligned(64))) void matmul_fp16(
    const __fp16 *Apack, const __fp16 *Bpack, __fp16 *C,
    uint32_t M, uint32_t N, uint32_t K)
{
    const uint32_t col_blocks = (N + N_BLOCK - 1) / N_BLOCK;
    // The unchanged assembly below still consumes A0/A1 per block. Switch
    // this to M_BLOCK only when the schedule starts consuming A2/A3.
    const uint32_t scheduled_m_block = 2 * TE;
    const uint32_t row_blocks =
        (M + scheduled_m_block - 1) / scheduled_m_block;
    const uint32_t tile_stride = TE * K;
    const uintptr_t operand_bytes = sizeof(__fp16) * TE;
    const uintptr_t tile_bytes = sizeof(__fp16) * tile_stride;
    const uintptr_t a_block_bytes = A_TILES_PER_BLOCK * tile_bytes;
    const uintptr_t b_block_bytes = B_TILES_PER_BLOCK * tile_bytes;
    const uintptr_t c_stride = CHUNK_SIZE * sizeof(__fp16);
    const uintptr_t c_col_block_bytes = N_BLOCK * sizeof(__fp16);
    const uintptr_t c_row_block_bytes = M_BLOCK * c_stride;

    uintptr_t a0p = (uintptr_t)Apack;
    uintptr_t a1p = (uintptr_t)(Apack + tile_stride);
    uintptr_t a2p = (uintptr_t)(Apack + 2 * tile_stride);
    uintptr_t a3p = (uintptr_t)(Apack + 3 * tile_stride);
    uintptr_t b0p = (uintptr_t)Bpack;
    uintptr_t b1p = (uintptr_t)(Bpack + tile_stride);

    uintptr_t tss;
    uintptr_t c00 = (uintptr_t)(C);
    uintptr_t c01 = c00 + operand_bytes;
    uintptr_t c10 = (uintptr_t)(C + TE * CHUNK_SIZE);
    uintptr_t c11 = (uintptr_t)(C + TE * CHUNK_SIZE + TE);
    uintptr_t c20 = (uintptr_t)(C + 2 * TE * CHUNK_SIZE);
    uintptr_t c21 = (uintptr_t)(C + 2 * TE * CHUNK_SIZE + TE);
    uintptr_t c30 = (uintptr_t)(C + 3 * TE * CHUNK_SIZE);
    uintptr_t c31 = (uintptr_t)(C + 3 * TE * CHUNK_SIZE + TE);

    uintptr_t block_counter = (uintptr_t)row_blocks * col_blocks;
    uintptr_t col_counter = (uintptr_t)col_blocks;
    uintptr_t loop_counter;
    const uintptr_t matrix_mtype = (TE << 10) | (2u << 5) | 1u;
    const uintptr_t matrix_vtype = 0xC8;  // e16, m1, ta, ma
    const uintptr_t middle_k_groups = (uintptr_t)K / 8 - 3;

    asm volatile(
        // K-Group 1
        "msetmtype %[mtype], %[vtype]\n"  // set sew=16, set tm=TE, tk=2, twiden=1
        "li %[loop], 16\n"
        "msettn x0, %[loop]\n"            // msetmtype resets tn, so set full tn=TE
        // we dont need preload
        "vtzero mt0\n"
        
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16 + v4*v20
        "vtfmm.tvv mt0, v8, v17\n"  // mt0 += v8*v17 + v12*v21
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v1, v16\n"  // mt4 += v1*v16 + v5*v20
        "vtfmm.tvv mt4, v9, v17\n"  // mt4 += v9*v17 + v13*v21
        "vtzero mt8\n"

        // vtfmm mt8
        "vtfmm.tvv mt8, v2,  v16\n"  // mt8 += v2*v16 + v6*v20
        "vtfmm.tvv mt8, v10,  v17\n"  // mt8 += v10*v17 + v14*v21
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v3,  v16\n"  // mt12 += v3*v16 + v7*v20
        "vtfmm.tvv mt12, v11,  v17\n"  // mt12 += v11*v17 + v15*v21
        "vtzero mt2\n"
        
        // vtfmm mt2
        "vtfmm.tvv mt2, v0, v24\n"  // mt2 += v0*v24 + v4*v28
        "vtfmm.tvv mt2, v8, v25\n"  // mt2 += v8*v25 + v12*v29
        "vtzero mt6\n"

        // vtfmm mt6
        "vtfmm.tvv mt6, v1, v24\n"  // mt6 += v1*v24 + v5*v28
        "vtfmm.tvv mt6, v9, v25\n"  // mt6 += v9*v25 + v13*v29
        "vtzero mt10\n"

        // vtfmm mt10
        "vtfmm.tvv mt10, v2,  v24\n"  // mt10 += v2*v24 + v6*v28
        "vtfmm.tvv mt10, v10,  v25\n"  // mt10 += v10*v25 + v14*v29
        "vtzero mt14\n"

        // vtfmm mt14
        "vtfmm.tvv mt14, v3,  v24\n"  // mt14 += v3*v24 + v7*v28
        "vtfmm.tvv mt14, v11,  v25\n"  // mt14 += v11*v25 + v15*v29

        "9:\n"
        // K-Group 1 to N-2
        // vtfmm mt0
        "mv %[loop], %[middle_k_groups]\n"
        "beqz %[loop], 3f\n"
        "1:\n"

        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 1b\n"
        "3:\n"

        // K-Group N-1 & K-Group N
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16 + v4*v20
        "vtfmm.tvv mt0, v8, v17\n"  // mt0 += v8*v17 + v12*v21
        "vtfmm.tvv mt0, v1, v18\n"  // mt0 += v1*v18 + v5*v22
        "vtfmm.tvv mt0, v9, v19\n"  // mt0 += v9*v19 + v13*v23
        "vtfmm.tvv mt0, v2,  v24\n"  // mt0 += v2*v24 + v6*v28
        "vtfmm.tvv mt0, v10,  v25\n"  // mt0 += v10*v25 + v14*v29
        "vtfmm.tvv mt0, v3,  v26\n"  // mt0 += v3*v26 + v7*v30
        "vtfmm.tvv mt0, v11,  v27\n"  // mt0 += v11*v27 + v15*v31

        // vtfmm mt4 || vtse mt0
        "mv %[tss], x0\n"
        "vtfmm.tvv mt4, v0, v16\n"  // mt4 += v0*v16 + v4*v20
        "vtfmm.tvv mt4, v8, v17\n"  // mt4 += v8*v17 + v12*v21
        "vtfmm.tvv mt4, v1, v18\n"  // mt4 += v1*v18 + v5*v22
        "vtfmm.tvv mt4, v9, v19\n"  // mt4 += v9*v19 + v13*v23
        "vtfmm.tvv mt4, v2,  v24\n"  // mt4 += v2*v24 + v6*v28
        "vtfmm.tvv mt4, v10,  v25\n"  // mt4 += v10*v25 + v14*v29
        "vtfmm.tvv mt4, v3,  v26\n"  // mt4 += v3*v26 + v7*v30
        "vtfmm.tvv mt4, v11,  v27\n"  // mt4 += v11*v27 + v15*v31

        // vtfmm mt8 || vtse mt4
        "lui %[tss], 0x20000\n"
        "vtfmm.tvv mt8, v0, v16\n"  // mt8 += v0*v16 + v4*v20
        "vtfmm.tvv mt8, v8, v17\n"  // mt8 += v8*v17 + v12*v21
        "vtfmm.tvv mt8, v1, v18\n"  // mt8 += v1*v18 + v5*v22
        "vtfmm.tvv mt8, v9, v19\n"  // mt8 += v9*v19 + v13*v23
        "vtfmm.tvv mt8, v2,  v24\n"  // mt8 += v2*v24 + v6*v28
        "vtfmm.tvv mt8, v10,  v25\n"  // mt8 += v10*v25 + v14*v29
        "vtfmm.tvv mt8, v3,  v26\n"  // mt8 += v3*v26 + v7*v30
        "vtfmm.tvv mt8, v11,  v27\n"  // mt8 += v11*v27 + v15*v31

        // vtfmm mt12 || vtse mt8
        "lui %[tss], 0x40000\n"
        "vtfmm.tvv mt12, v0, v16\n"  // mt12 += v0*v16 + v4*v20
        "vtfmm.tvv mt12, v8, v17\n"  // mt12 += v8*v17 + v12*v21
        "vtfmm.tvv mt12, v1, v18\n"  // mt12 += v1*v18 + v5*v22
        "vtfmm.tvv mt12, v9, v19\n"  // mt12 += v9*v19 + v13*v23
        "vtfmm.tvv mt12, v2,  v24\n"  // mt12 += v2*v24 + v6*v28
        "vtfmm.tvv mt12, v10,  v25\n"  // mt12 += v10*v25 + v14*v29
        "vtfmm.tvv mt12, v3,  v26\n"  // mt12 += v3*v26 + v7*v30
        "vtfmm.tvv mt12, v11,  v27\n"  // mt12 += v11*v27 + v15*v31

        // vtfmm mt14 || vtse mt12
        "lui %[tss], 0x60000\n"
        "vtfmm.tvv mt14, v0, v16\n"  // mt14 += v0*v16 + v4*v20
        "vtfmm.tvv mt14, v8, v17\n"  // mt14 += v8*v17 + v12*v21
        "vtfmm.tvv mt14, v1, v18\n"  // mt14 += v1*v18 + v5*v22
        "vtfmm.tvv mt14, v9, v19\n"  // mt14 += v9*v19 + v13*v23
        "vtfmm.tvv mt14, v2,  v24\n"  // mt14 += v2*v24 + v6*v28
        "vtfmm.tvv mt14, v10,  v25\n"  // mt14 += v10*v25 + v14*v29
        "vtfmm.tvv mt14, v3,  v26\n"  // mt14 += v3*v26 + v7*v30
        "vtfmm.tvv mt14, v11,  v27\n"  // mt14 += v11*v27 + v15*v31

        // vtfmm mt10 || vtse mt14
        "lui %[tss], 0x70000\n"
        "vtfmm.tvv mt10, v0, v16\n"  // mt10 += v0*v16 + v4*v20
        "vtfmm.tvv mt10, v8, v17\n"  // mt10 += v8*v17 + v12*v21
        "vtfmm.tvv mt10, v1, v18\n"  // mt10 += v1*v18 + v5*v22
        "vtfmm.tvv mt10, v9, v19\n"  // mt10 += v9*v19 + v13*v23
        "vtfmm.tvv mt10, v2,  v24\n"  // mt10 += v2*v24 + v6*v28
        "vtfmm.tvv mt10, v10,  v25\n"  // mt10 += v10*v25 + v14*v29
        "vtfmm.tvv mt10, v3,  v26\n"  // mt10 += v3*v26 + v7*v30
        "vtfmm.tvv mt10, v11,  v27\n"  // mt10 += v11*v27 + v15*v31

        // vtfmm mt6 || vtse mt10
        "lui %[tss], 0x50000\n"
        "vtfmm.tvv mt6, v0, v16\n"  // mt6 += v0*v16 + v4*v20
        "vtfmm.tvv mt6, v8, v17\n"  // mt6 += v8*v17 + v12*v21
        "vtfmm.tvv mt6, v1, v18\n"  // mt6 += v1*v18 + v5*v22
        "vtfmm.tvv mt6, v9, v19\n"  // mt6 += v9*v19 + v13*v23
        "vtfmm.tvv mt6, v2,  v24\n"  // mt6 += v2*v24 + v6*v28
        "vtfmm.tvv mt6, v10,  v25\n"  // mt6 += v10*v25 + v14*v29
        "vtfmm.tvv mt6, v3,  v26\n"  // mt6 += v3*v26 + v7*v30
        "vtfmm.tvv mt6, v11,  v27\n"  // mt6 += v11*v27 + v15*v31

        // vtfmm mt2 || vtse mt6
        "lui %[tss], 0x10000\n"
        "vtfmm.tvv mt2, v0, v16\n"  // mt2 += v0*v16 + v4*v20
        "vtfmm.tvv mt2, v8, v17\n"  // mt2 += v8*v17 + v12*v21
        "vtfmm.tvv mt2, v1, v18\n"  // mt2 += v1*v18 + v5*v22
        "vtfmm.tvv mt2, v9, v19\n"  // mt2 += v9*v19 + v13*v23
        "vtfmm.tvv mt2, v2,  v24\n"  // mt2 += v2*v24 + v6*v28
        "vtfmm.tvv mt2, v10,  v25\n"  // mt2 += v10*v25 + v14*v29
        "vtfmm.tvv mt2, v3,  v26\n"  // mt2 += v3*v26 + v7*v30
        "vtfmm.tvv mt2, v11,  v27\n"  // mt2 += v11*v27 + v15*v31

        "lui %[tss], 0x20000\n"
        // block transition
        "vtzero mt0\n"
        "addi %[blocks], %[blocks], -1\n"
        "beqz %[blocks], 7f\n"

        // Keep the block-control skeleton while rebuilding the optimized
        // schedule. The M/N block pointers intentionally remain unchanged.
        "addi %[cols], %[cols], -1\n"
        "beqz %[cols], 4f\n"
        "j 5f\n"
        "4:\n"
        "mv %[cols], %[col_blocks]\n"
        "5:\n"

        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16 + v4*v20
        "vtfmm.tvv mt0, v8, v17\n"  // mt0 += v8*v17 + v12*v21
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v1, v16\n"  // mt4 += v1*v16 + v5*v20
        "vtfmm.tvv mt4, v9, v17\n"  // mt4 += v9*v17 + v13*v21
        "vtzero mt8\n"

        // vtfmm mt8
        "vtfmm.tvv mt8, v2,  v16\n"  // mt8 += v2*v16 + v6*v20
        "vtfmm.tvv mt8, v10,  v17\n"  // mt8 += v10*v17 + v14*v21
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v3,  v16\n"  // mt12 += v3*v16 + v7*v20
        "vtfmm.tvv mt12, v11,  v17\n"  // mt12 += v11*v17 + v15*v21
        "vtzero mt2\n"
        
        // vtfmm mt2
        "vtfmm.tvv mt2, v0, v24\n"  // mt2 += v0*v24 + v4*v28
        "vtfmm.tvv mt2, v8, v25\n"  // mt2 += v8*v25 + v12*v29
        "vtzero mt6\n"

        // vtfmm mt6
        "vtfmm.tvv mt6, v1, v24\n"  // mt6 += v1*v24 + v5*v28
        "vtfmm.tvv mt6, v9, v25\n"  // mt6 += v9*v25 + v13*v29
        "vtzero mt10\n"

        // vtfmm mt10
        "vtfmm.tvv mt10, v2,  v24\n"  // mt10 += v2*v24 + v6*v28
        "vtfmm.tvv mt10, v10,  v25\n"  // mt10 += v10*v25 + v14*v29
        "vtzero mt14\n"

        // vtfmm mt14
        "vtfmm.tvv mt14, v3,  v24\n"  // mt14 += v3*v24 + v7*v28
        "vtfmm.tvv mt14, v11,  v25\n"  // mt14 += v11*v25 + v15*v29
        "mv %[tss], x0\n"
        "j 9b\n"

        // Final block: compact TM-row mt8 store loop.
        "7:\n"
        "li %[loop], 16\n"
        "2:\n"
        
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
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [middle_k_groups] "r"(middle_k_groups),
          [col_blocks] "r"((uintptr_t)col_blocks)
        : "memory");
}
