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
    uintptr_t row_counter;
    uintptr_t col_counter;
    uintptr_t loop_counter;
    const uintptr_t c_stride = CHUNK_SIZE * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (1u << 5) | 1u;
    const uintptr_t matrix_vtype = 0xD1;  // e32, m2, ta, ma
    const uintptr_t tile_bytes = operand_bytes * K;
    const uintptr_t store_rows_bytes = TE * c_stride;
    const uintptr_t col_c_back = store_rows_bytes - 2 * operand_bytes;
    const uintptr_t row_c_advance =
        store_rows_bytes - (col_blocks - 1) * 2 * operand_bytes;
    const uintptr_t row_b_rewind = (2 * col_blocks - 1) * tile_bytes;
    const uintptr_t middle_k_groups = (uintptr_t)K / 4 - 3;

    asm volatile(
        "mv %[rows], %[row_blocks]\n"
        ".Lrow_%=:\n"
        "mv %[cols], %[col_blocks]\n"
        ".Lcol_%=:\n"
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
        
        // vtfmm mt4
        "vle32.v v18,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v0, v16\n"
        "vle32.v v20,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vle32.v v22,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vle32.v v0,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        
        // vtfmm mt12
        "vle32.v v2,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v0,  v16\n"
        "vle32.v v4,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v2,  v18\n"
        "vle32.v v6,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v4,  v20\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v6,  v22\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v10,  v26\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"   
        "vtfmm.tvv mt12, v12,  v28\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt12, v14,  v30\n"

        // vtfmm mt8
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v0,  v16\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v2,  v18\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v4,  v20\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v6,  v22\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v8,  v24\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v10,  v26\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n" 
        "vtfmm.tvv mt8, v12,  v28\n"
        "vtfmm.tvv mt8, v14,  v30\n"

        // Store each completed 32x32 accumulator after all K-groups finish.
        // LMUL=2 vector instructions use even register-group bases only.
        "mv %[tss], x0\n"  // mt0 -> c00
        "li %[loop], 32\n"
        ".Lstore_mt0_%=:\n"
        "vtmv.v.t v0, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v0, (%[c00])\n"
        "add %[c00], %[c00], %[c_stride]\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], .Lstore_mt0_%=\n"

        "lui %[tss], 0x20000\n"  // mt4 -> c01
        "li %[loop], 32\n"
        ".Lstore_mt4_%=:\n"
        "vtmv.v.t v0, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v0, (%[c01])\n"
        "add %[c01], %[c01], %[c_stride]\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], .Lstore_mt4_%=\n"

        "lui %[tss], 0x40000\n"  // mt8 -> c10
        "li %[loop], 32\n"
        ".Lstore_mt8_%=:\n"
        "vtmv.v.t v0, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v0, (%[c10])\n"
        "add %[c10], %[c10], %[c_stride]\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], .Lstore_mt8_%=\n"

        "lui %[tss], 0x60000\n"  // mt12 -> c11
        "li %[loop], 32\n"
        ".Lstore_mt12_%=:\n"
        "vtmv.v.t v0, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v0, (%[c11])\n"
        "add %[c11], %[c11], %[c_stride]\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], .Lstore_mt12_%=\n"

        // Advance to the next 32-column block, or to the next 32-row block.
        "addi %[cols], %[cols], -1\n"
        "beqz %[cols], .Lnext_row_%=\n"

        // Horizontal step: A is reused, B advances by one tile, and C moves
        // from the completed 32-column block to the next one.
        "sub %[a0p], %[a0p], %[tile_bytes]\n"
        "sub %[a1p], %[a1p], %[tile_bytes]\n"
        "add %[b0p], %[b0p], %[tile_bytes]\n"
        "add %[b1p], %[b1p], %[tile_bytes]\n"
        "sub %[c00], %[c00], %[col_c_back]\n"
        "sub %[c01], %[c01], %[col_c_back]\n"
        "sub %[c10], %[c10], %[col_c_back]\n"
        "sub %[c11], %[c11], %[col_c_back]\n"
        "j .Lcol_%=\n"

        ".Lnext_row_%=:\n"
        "addi %[rows], %[rows], -1\n"
        "beqz %[rows], .Ldone_%=\n"

        // Vertical step: A advances to the next row pair, B returns to the
        // first column block, and C advances to the next row pair.
        "add %[a0p], %[a0p], %[tile_bytes]\n"
        "add %[a1p], %[a1p], %[tile_bytes]\n"
        "sub %[b0p], %[b0p], %[row_b_rewind]\n"
        "sub %[b1p], %[b1p], %[row_b_rewind]\n"
        "add %[c00], %[c00], %[row_c_advance]\n"
        "add %[c01], %[c01], %[row_c_advance]\n"
        "add %[c10], %[c10], %[row_c_advance]\n"
        "add %[c11], %[c11], %[row_c_advance]\n"
        "j .Lrow_%=\n"

        ".Ldone_%=:\n"
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
          [rows] "=&r"(row_counter),
          [cols] "=&r"(col_counter),
          [loop] "=&r"(loop_counter)
        :
          [row_blocks] "r"((uintptr_t)row_blocks),
          [col_blocks] "r"((uintptr_t)col_blocks),
          [c_stride] "r"(c_stride),
          [operand_bytes] "r"(operand_bytes),
          [tile_bytes] "r"(tile_bytes),
          [col_c_back] "r"(col_c_back),
          [row_c_advance] "r"(row_c_advance),
          [row_b_rewind] "r"(row_b_rewind),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [middle_k_groups] "r"(middle_k_groups)
        : "memory");
}
