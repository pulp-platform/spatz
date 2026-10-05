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
    const uintptr_t tile_stride = (uintptr_t)TE * K;
    uintptr_t a0p = (uintptr_t)Apack;
    uintptr_t a1p = (uintptr_t)(Apack + tile_stride);
    uintptr_t b0p = (uintptr_t)Bpack;
    uintptr_t b1p = (uintptr_t)(Bpack + tile_stride);
    uintptr_t tss;
    const uintptr_t operand_bytes = sizeof(float) * TE;
    const uintptr_t operand_bytes_m2 = operand_bytes;
    const uintptr_t operand_bytes_m8 = 4 * operand_bytes;
    const uintptr_t c_block_bytes = 2 * operand_bytes;
    uintptr_t c00 = (uintptr_t)(C);
    uintptr_t c01 = c00 + operand_bytes;
    uintptr_t c10 = (uintptr_t)(C + TE * CHUNK_SIZE);
    uintptr_t c11 = (uintptr_t)(C + TE * CHUNK_SIZE + TE);
    uintptr_t block_counter = (uintptr_t)row_blocks * col_blocks;
    uintptr_t col_counter = (uintptr_t)col_blocks;
    uintptr_t loop_counter;
    uintptr_t store_counter;
    uintptr_t vl_counter;
    const uintptr_t c_stride = CHUNK_SIZE * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (1u << 5) | 1u;
    const uintptr_t matrix_vtype = 0xD1;  // e32, m2, ta, ma
    const uintptr_t rewind_b0_bytes = 2 * operand_bytes_m8;
    const uintptr_t middle_k_groups = (uintptr_t)K / 4 - 3;

    asm volatile(
        ".Lmatmul_block_%=:\n"
        // K-Group 1
        // LMUL=2: VLE/VTFMM use even registers only. The vtmv/vse32 store
        // destinations below intentionally preserve the te16 schedule.
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes_m8]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n"  // set sew=32, set tm=TE, tk=1, twiden=1
        "msettn x0, %[vl]\n"              // msetmtype resets tn, so set full tn=TE
        
        // vtfmm mt0
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m2]\n"
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m2]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m2]\n"
        "vtzero mt8\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v14, v30\n"

        "9:\n"
        "mv %[loop], %[middle_k_groups]\n"
        "beqz %[loop], 3f\n"
        "1:\n"
        // A0 v0..v7 and B0 v16..v23 are already loaded; do not advance again.
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m2]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m2]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m2]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 1b\n"
        "3:\n"

        // K-Group N-1 & K-Group N
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vtfmm.tvv mt0, v8, v24\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vtfmm.tvv mt0, v14, v30\n"

        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vtfmm.tvv mt4, v14, v30\n"  

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n" // mt4
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "sub %[b0p], %[b0p], %[rewind_b0_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0,  v16\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vtfmm.tvv mt8, v14,  v30\n"

        // Store the three non-deferred tiles. LMUL=2 requires an even
        // vector-register group, and each TE32 tile has 32 complete rows.
        "mv %[tss], x0\n"  // mt0 -> c00
        "li %[store], 32\n"
        ".Lstore_mt0_%=:\n"
        "vtmv.v.t v0, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v0, (%[c00])\n"
        "add %[c00], %[c00], %[c_stride]\n"
        "addi %[store], %[store], -1\n"
        "bnez %[store], .Lstore_mt0_%=\n"

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
        "slli %[loop], %[c_stride], 5\n"
        "sub %[loop], %[loop], %[c_block_bytes]\n"
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
        "slli %[loop], %[c_stride], 5\n"
        "sub %[loop], %[loop], %[c_block_bytes]\n"
        "sub %[loop], x0, %[loop]\n"

        "5:\n"

        // Cross-Block Compute-Store overlapped (no control-flow path)
        // vtfmm mt0
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes_m8]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "lui %[tss], 0x40000\n"  // mt8
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtfmm.tvv mt0, v6, v22\n"

        // Store the preceding block's deferred mt8 while computing the next
        // block. v8 is free until the A1 group is loaded below.
        "li %[store], 32\n"
        ".Lstore_prev_mt8_%=:\n"
        "vtmv.v.t v8, %[tss]\n"
        "addi %[tss], %[tss], 1\n"
        "vse32.v v8, (%[c10])\n"
        "add %[c10], %[c10], %[c_stride]\n"
        "addi %[store], %[store], -1\n"
        "bnez %[store], .Lstore_prev_mt8_%=\n"

        // vtfmm mt4
        "vtzero mt4\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vtfmm.tvv mt4, v6, v30\n"

        // vtfmm mt12
        "vtzero mt12\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v14,  v30\n"

        // vtfmm mt8
        "vtzero mt8\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        // "slli %[loop], %[c_stride], 5\n"
        // "sub %[loop], %[loop], %[c_block_bytes]\n"
        "sub %[c00], %[c00], %[loop]\n"
        "sub %[c01], %[c01], %[loop]\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes_m8]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "j 9b\n"

        // Final block: compact 32-row mt8 store loop.
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
          [store] "=&r"(store_counter),
          [vl] "=&r"(vl_counter)
        :
          [c_stride] "r"(c_stride),
          [operand_bytes_m2] "r"(operand_bytes_m2),
          [operand_bytes_m8] "r"(operand_bytes_m8),
          [c_block_bytes] "r"(c_block_bytes),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [rewind_b0_bytes] "r"(rewind_b0_bytes),
          [middle_k_groups] "r"(middle_k_groups),
          [col_blocks] "r"((uintptr_t)col_blocks)
        : "memory");
}
