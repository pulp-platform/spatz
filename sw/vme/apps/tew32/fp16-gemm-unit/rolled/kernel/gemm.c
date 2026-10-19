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
// FP16-to-FP32 GEMM with interleaved bias loads; variants with and without cross-block overlap.

__attribute__((noinline, aligned(64))) void gemm_fp16_fp32_unit_vtle_no_crossblock(const __fp16 *Atpack, const __fp16 *Bpack, float *C, uint32_t M, uint32_t N, uint32_t K)
{
    // Full 32x32 blocks; K is the real FP16 extent, >=24 and divisible by 8.
    for (uint32_t mb = 0; mb < M; mb += BLOCK_DIM) {
      for (uint32_t nb = 0; nb < N; nb += BLOCK_DIM) {
    const uint32_t tile_stride = TE * K;
    uintptr_t a0p = (uintptr_t)(Atpack + mb * K);
    uintptr_t a1p = (uintptr_t)(Atpack + mb * K + tile_stride);
    uintptr_t b0p = (uintptr_t)(Bpack + nb * K);
    uintptr_t b1p = (uintptr_t)(Bpack + nb * K + tile_stride);
    uintptr_t tss;
    const uintptr_t c_tile_bytes = sizeof(float) * TE;
    uintptr_t c00 = (uintptr_t)(C + mb * N + nb);
    uintptr_t c01 = c00 + c_tile_bytes;
    uintptr_t c10 = (uintptr_t)(C + (mb + TE) * N + nb);
    uintptr_t c11 = (uintptr_t)(C + (mb + TE) * N + nb + TE);
    uintptr_t loop_counter;
    uintptr_t vl_counter;
    const uintptr_t c_stride = N * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (2u << 5) | 2u;
    const uintptr_t matrix_vtype = 0xC8;  // e16, m1, ta, ma
    const uintptr_t operand_group_bytes = 4 * TE * sizeof(uint32_t);
    const uintptr_t rewind_b0_bytes = 2 * operand_group_bytes;
    const uintptr_t middle_k_groups = (uintptr_t)K / 8 - 3;

    asm volatile(
        "vsetvli %[vl], x0, e32, m4, ta, ma\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        // Bootstrap mt0; preload the other tiles during preceding-tile MACs.
        "li %[tss], 0\n"
        "li %[loop], 16\n"
        ".Lbias_mt0_%=:\n"
        "vtle32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "addi %[tss], %[tss], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], .Lbias_mt0_%=\n"
        "slli %[vl], %[c_stride], 4\n"
        "sub %[c00], %[c00], %[vl]\n"

        // K group 0: interleave four bias rows after each preceding-tile MAC.
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n"
        "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Load mt4 bias while computing mt0.
        "li %[tss], 0x20000000\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "slli %[loop], %[c_stride], 4\n"
        "sub %[c01], %[c01], %[loop]\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Load mt8 bias while computing mt4.
        "li %[tss], 0x40000000\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "slli %[loop], %[c_stride], 4\n"
        "sub %[c10], %[c10], %[loop]\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Load mt12 bias while computing mt8.
        "li %[tss], 0x60000000\n"
        "vtfmm.tvv mt8, v8, v16\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9, v17\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "slli %[loop], %[c_stride], 4\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n"
        "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        "vtfmm.tvv mt12, v8, v24\n"
        "vtfmm.tvv mt12, v9, v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "9:\n"
        "mv %[loop], %[middle_k_groups]\n"
        "beqz %[loop], 3f\n"
        "1:\n"
        // Middle K groups: four consecutive VTFMMs per load/compute phase.
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v8, v16\n"
        "vtfmm.tvv mt8, v9, v17\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n"
        "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v8, v24\n"
        "vtfmm.tvv mt12, v9, v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 1b\n"
        "3:\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // K-Group N-1 & K-Group N
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b0p])\n" "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v16, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // vtfmm mt4
        // vtse mt0
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n" // mt4
        "vtfmm.tvv mt12, v0, v16\n"
        "vtfmm.tvv mt12, v1, v17\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2, v18\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3, v19\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8, v24\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9, v25\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "sub %[b0p], %[b0p], %[rewind_b0_bytes]\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v16, (%[b0p])\n" "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b0p])\n" "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0, v16\n"
        "vtfmm.tvv mt8, v1, v17\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2, v18\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3, v19\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8, v24\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9, v25\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10, v26\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11, v27\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        "lui %[tss], 0x40000\n"
        // Finish this block: compact 16-row mt8 store loop.
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
          [a0p] "+&r"(a0p),
          [a1p] "+&r"(a1p),
          [b0p] "+&r"(b0p),
          [b1p] "+&r"(b1p),
          [tss] "=&r"(tss),
          [c00] "+&r"(c00),
          [c01] "+&r"(c01),
          [c10] "+&r"(c10),
          [c11] "+&r"(c11),
          [loop] "=&r"(loop_counter),
          [vl] "=&r"(vl_counter)
        :
          [c_stride] "r"(c_stride),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [rewind_b0_bytes] "r"(rewind_b0_bytes),
          [operand_group_bytes] "r"(operand_group_bytes),
          [middle_k_groups] "r"(middle_k_groups)
        : "memory");
      }
    }
}

__attribute__((noinline, aligned(64))) void gemm_fp16_fp32_unit_crossblock(const __fp16 *Atpack, const __fp16 *Bpack, float *C, uint32_t M, uint32_t N, uint32_t K)
{
    // Full 32x32 blocks; K is the real FP16 extent, >=24 and divisible by 8.
    const uint32_t col_blocks = N / BLOCK_DIM;
    const uint32_t row_blocks = M / BLOCK_DIM;
    const uint32_t tile_stride = TE * K;
    uintptr_t a0p = (uintptr_t)Atpack;
    uintptr_t a1p = (uintptr_t)(Atpack + tile_stride);
    uintptr_t b0p = (uintptr_t)Bpack;
    uintptr_t b1p = (uintptr_t)(Bpack + tile_stride);
    uintptr_t tss;
    uintptr_t bias_tss, bias_ptr;
    const uintptr_t c_tile_bytes = sizeof(float) * TE;
    uintptr_t c00 = (uintptr_t)(C);
    uintptr_t c01 = c00 + c_tile_bytes;
    uintptr_t c10 = (uintptr_t)(C + TE * N);
    uintptr_t c11 = (uintptr_t)(C + TE * N + TE);
    uintptr_t block_counter = (uintptr_t)row_blocks * col_blocks;
    uintptr_t col_counter = (uintptr_t)col_blocks;
    uintptr_t loop_counter;
    uintptr_t vl_counter;
    const uintptr_t c_stride = N * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (2u << 5) | 2u;
    const uintptr_t matrix_vtype = 0xC8;  // e16, m1, ta, ma
    const uintptr_t operand_group_bytes = 4 * TE * sizeof(uint32_t);
    const uintptr_t rewind_b0_bytes = 2 * operand_group_bytes;
    const uintptr_t middle_k_groups = (uintptr_t)K / 8 - 3;

    asm volatile(
        "vsetvli %[vl], x0, e32, m4, ta, ma\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        // Bootstrap mt0; preload the other tiles during preceding-tile MACs.
        "li %[tss], 0\n"
        "li %[loop], 16\n"
        ".Lbias_mt0_%=:\n"
        "vtle32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "addi %[tss], %[tss], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], .Lbias_mt0_%=\n"
        "slli %[vl], %[c_stride], 4\n"
        "sub %[c00], %[c00], %[vl]\n"

        // K group 0: interleave four bias rows after each preceding-tile MAC.
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n"
        "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Load mt4 bias while computing mt0.
        "li %[tss], 0x20000000\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "slli %[loop], %[c_stride], 4\n"
        "sub %[c01], %[c01], %[loop]\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Load mt8 bias while computing mt4.
        "li %[tss], 0x40000000\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "slli %[loop], %[c_stride], 4\n"
        "sub %[c10], %[c10], %[loop]\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        // Load mt12 bias while computing mt8.
        "li %[tss], 0x60000000\n"
        "vtfmm.tvv mt8, v8, v16\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9, v17\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtle32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "slli %[loop], %[c_stride], 4\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n"
        "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"

        "vtfmm.tvv mt12, v8, v24\n"
        "vtfmm.tvv mt12, v9, v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "9:\n"
        "mv %[loop], %[middle_k_groups]\n"
        "beqz %[loop], 3f\n"
        "1:\n"
        // Middle K groups: four consecutive VTFMMs per load/compute phase.
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt8, v8, v16\n"
        "vtfmm.tvv mt8, v9, v17\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n"
        "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n"
        "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v8, v24\n"
        "vtfmm.tvv mt12, v9, v25\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 1b\n"
        "3:\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // K-Group N-1 & K-Group N
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b0p])\n" "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v16, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // vtfmm mt4
        // vtse mt0
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n" // mt4
        "vtfmm.tvv mt12, v0, v16\n"
        "vtfmm.tvv mt12, v1, v17\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2, v18\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3, v19\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8, v24\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9, v25\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "sub %[b0p], %[b0p], %[rewind_b0_bytes]\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v16, (%[b0p])\n" "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b0p])\n" "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0, v16\n"
        "vtfmm.tvv mt8, v1, v17\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2, v18\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3, v19\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8, v24\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v9, v25\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10, v26\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11, v27\n"
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
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n" "add %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // Cross-Block Compute-Store overlapped (no control-flow path)
        // vtfmm mt0
        "lui %[tss], 0x40000\n"  // mt8
        // Next block mt0: its previous result has already been stored.
        "li %[bias_tss], 0x0\n"
        "sub %[bias_ptr], %[c00], %[loop]\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v0, v16\n"
        // Next block mt4: its previous result has already been stored.
        "li %[bias_tss], 0x20000000\n"
        "sub %[bias_ptr], %[c01], %[loop]\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v24, (%[b1p])\n" "add %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt4

        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v0, v24\n"
        // Next block mt12: its previous result has already been stored.
        "li %[bias_tss], 0x60000000\n"
        "sub %[bias_ptr], %[c11], %[loop]\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v8, (%[a1p])\n" "add %[a1p], %[a1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v0, (%[a0p])\n" "add %[a0p], %[a0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[vl]\n"

        // vtfmm mt12

        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8, v24\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9, v25\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        "vtse32 %[tss], (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n" "addi %[tss], %[tss], 1\n"
        // Next block mt8: its previous result has already been stored.
        "li %[bias_tss], 0x40000000\n"
        "sub %[bias_ptr], %[c10], %[loop]\n"
        
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtle32 %[bias_tss], (%[bias_ptr])\n" "add %[bias_ptr], %[bias_ptr], %[c_stride]\n" "addi %[bias_tss], %[bias_tss], 1\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vtfmm.tvv mt12, v11, v27\n"

        // vtfmm mt8

        "vtfmm.tvv mt8, v8, v16\n"
        "sub %[c00], %[c00], %[loop]\n"
        "sub %[c01], %[c01], %[loop]\n"
        "vtfmm.tvv mt8, v9, v17\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vsetvli %[vl], x0, e32, m4, ta, ma\n" "vle32.v v16, (%[b0p])\n" "add %[b0p], %[b0p], %[operand_group_bytes]\n"
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
          [a0p] "+&r"(a0p),
          [a1p] "+&r"(a1p),
          [b0p] "+&r"(b0p),
          [b1p] "+&r"(b1p),
          [tss] "=&r"(tss),
          [bias_tss] "=&r"(bias_tss),
          [bias_ptr] "=&r"(bias_ptr),
          [c00] "+&r"(c00),
          [c01] "+&r"(c01),
          [c10] "+&r"(c10),
          [c11] "+&r"(c11),
          [blocks] "+&r"(block_counter),
          [cols] "+&r"(col_counter),
          [loop] "=&r"(loop_counter),
          [vl] "=&r"(vl_counter)
        :
          [c_stride] "r"(c_stride),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [rewind_b0_bytes] "r"(rewind_b0_bytes),
          [operand_group_bytes] "r"(operand_group_bytes),
          [middle_k_groups] "r"(middle_k_groups),
          [col_blocks] "r"((uintptr_t)col_blocks)
        : "memory");
}

__attribute__((noinline, aligned(64))) void gemm_fp16_fp32_unit_vle(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C)
{
  const uintptr_t c_stride = 128 * sizeof(float);
  const uintptr_t mtype = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t vtype = 0xC8; // e16, m1, tk=2 widening SDOTP
  const uintptr_t store_mtype = (TE << 10) | (1u << 5) | 1u;
  const uintptr_t store_vtype = 0xD0; // e32, m1: one vector per tile row
  const uintptr_t tile_vl = TE;
  const uintptr_t group_vl = 4 * TE;
  const uintptr_t bias_vl = 8 * TE; // m8: two contiguous 64-element FP32 rows
  const uintptr_t operand_vector_bytes = TE * sizeof(uint32_t);
  const uintptr_t operand_group_bytes = 4 * TE * sizeof(uint32_t);

  // Process each 16-row output block independently; no state crosses blocks.
  for (uint32_t nb = 0; nb < 4; nb += 1) {
    const uint32_t mb0 = nb * TE;
    {
      // Initialize and compute one complete 16-row output block.
      uintptr_t a0p = (uintptr_t)(Atpack + mb0 * K_CHUNK);
      uintptr_t b0p = (uintptr_t)Bpack;
      uintptr_t b1p = (uintptr_t)(Bpack + TE * K_CHUNK);
      uintptr_t b2p = (uintptr_t)(Bpack + 2 * TE * K_CHUNK);
      uintptr_t b3p = (uintptr_t)(Bpack + 3 * TE * K_CHUNK);
      uintptr_t ptr = (uintptr_t)(C + mb0 * 64);
      uintptr_t bias_ptr = ptr;
      uintptr_t tss0 = 0x00000000u;
      uintptr_t tss4 = 0x20000000u;
      uintptr_t tss8 = 0x40000000u;
      uintptr_t tss12 = 0x60000000u;

      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vle32.v v0, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v0", "memory");
      asm volatile("vle32.v v1, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v1", "memory");
      asm volatile("vle32.v v16, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v16", "memory");
      asm volatile("vle32.v v17, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v17", "memory");
      asm volatile("vtzero mt0" : : : "memory");
      asm volatile("vtzero mt4" : : : "memory");
      asm volatile("vtzero mt8" : : : "memory");
      asm volatile("vtzero mt12" : : : "memory");

      // Use the already-advanced A pointer as the loop induction variable.
      const uintptr_t a_middle_end = a0p + (K_CHUNK / 8 - 1) * operand_group_bytes;
      while (a0p != a_middle_end) {
      asm volatile("vtfmm.tvv mt0, v0, v16" : : : "memory");
      asm volatile("vle32.v v2, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v2", "memory");
      asm volatile("vtfmm.tvv mt0, v1, v17" : : : "memory");
      asm volatile("vle32.v v18, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v18", "memory");
      asm volatile("vle32.v v3, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v3", "memory");
      asm volatile("vtfmm.tvv mt0, v2, v18" : : : "memory");
      asm volatile("vle32.v v19, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v19", "memory");
      asm volatile("vtfmm.tvv mt0, v3, v19" : : : "memory");
      asm volatile("vle32.v v24, (%0)\naddi %0, %0, %1" : "+r"(b1p) : "i"(operand_vector_bytes) : "v24", "memory");
      asm volatile("vle32.v v25, (%0)\naddi %0, %0, %1" : "+r"(b1p) : "i"(operand_vector_bytes) : "v25", "memory");
      asm volatile("vtfmm.tvv mt4, v0, v24" : : : "memory");
      asm volatile("vle32.v v26, (%0)\naddi %0, %0, %1" : "+r"(b1p) : "i"(operand_vector_bytes) : "v26", "memory");
      asm volatile("vtfmm.tvv mt4, v1, v25" : : : "memory");
      asm volatile("vle32.v v27, (%0)\naddi %0, %0, %1" : "+r"(b1p) : "i"(operand_vector_bytes) : "v27", "memory");
      asm volatile("vtfmm.tvv mt4, v2, v26" : : : "memory");
      asm volatile("vtfmm.tvv mt4, v3, v27" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v16, (%0)" : : "r"(b2p) : "v16", "v17", "v18", "v19", "memory"); b2p += operand_group_bytes;
      asm volatile("vle32.v v24, (%0)" : : "r"(b3p) : "v24", "v25", "v26", "v27", "memory"); b3p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt8, v0, v16" : : : "memory");
      asm volatile("vtfmm.tvv mt8, v1, v17" : : : "memory");
      asm volatile("vtfmm.tvv mt8, v2, v18" : : : "memory");
      asm volatile("vle32.v v16, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v16", "memory");
      asm volatile("vtfmm.tvv mt8, v3, v19" : : : "memory");
      asm volatile("vle32.v v17, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v17", "memory");
      asm volatile("vtfmm.tvv mt12, v0, v24" : : : "memory");
      asm volatile("vtfmm.tvv mt12, v1, v25" : : : "memory");
      asm volatile("vle32.v v0, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v0", "memory");
      asm volatile("vtfmm.tvv mt12, v2, v26" : : : "memory");
      asm volatile("vle32.v v1, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v1", "memory");
      asm volatile("vtfmm.tvv mt12, v3, v27" : : : "memory");
      // Keep the pointer comparison; otherwise Clang adds a separate countdown.
      asm volatile("" : "+r"(a0p));
      }

      // Final K group: move each completed tile row to the vector registers.
      asm volatile("vtfmm.tvv mt0, v0, v16" : : : "memory");
      asm volatile("vle32.v v2, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v2", "memory");
      asm volatile("vtfmm.tvv mt0, v1, v17" : : : "memory");
      asm volatile("vle32.v v3, (%0)\naddi %0, %0, %1" : "+r"(a0p) : "i"(operand_vector_bytes) : "v3", "memory");
      asm volatile("vle32.v v18, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v18", "memory");
      asm volatile("vtfmm.tvv mt0, v2, v18" : : : "memory");
      asm volatile("vle32.v v19, (%0)\naddi %0, %0, %1" : "+r"(b0p) : "i"(operand_vector_bytes) : "v19", "memory");
      asm volatile("vtfmm.tvv mt0, v3, v19" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v24, (%0)" : : "r"(b1p) : "v24", "v25", "v26", "v27", "memory"); b1p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt4, v0, v24" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v16, (%0)" : : "r"(b2p) : "v16", "v17", "v18", "v19", "memory"); b2p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt4, v2, v26" : : : "memory");
      asm volatile("vtfmm.tvv mt4, v1, v25" : : : "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(store_mtype), "r"(store_vtype), "r"(tile_vl) : "memory");
      asm volatile("vtmv.v.t v8, %0\naddi %0, %0, 1" : "+r"(tss0) : : "v8", "memory");
      asm volatile("vtmv.v.t v12, %0\naddi %0, %0, 1" : "+r"(tss0) : : "v12", "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt4, v3, v27" : : : "memory");
      asm volatile("vtfmm.tvv mt8, v0, v16" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v24, (%0)" : : "r"(b3p) : "v24", "v25", "v26", "v27", "memory"); b3p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt8, v1, v17" : : : "memory");
      asm volatile("vtfmm.tvv mt8, v2, v18" : : : "memory");
      asm volatile("vtfmm.tvv mt8, v3, v19" : : : "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(store_mtype), "r"(store_vtype), "r"(tile_vl) : "memory");
      asm volatile("vtmv.v.t v16, %0\naddi %0, %0, 1" : "+r"(tss0) : : "v16", "memory");
      asm volatile("vtmv.v.t v20, %0\naddi %0, %0, 1" : "+r"(tss0) : : "v20", "memory");
      asm volatile("vtmv.v.t v9, %0\naddi %0, %0, 1" : "+r"(tss4) : : "v9", "memory");
      asm volatile("vtmv.v.t v13, %0\naddi %0, %0, 1" : "+r"(tss4) : : "v13", "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt12, v0, v24" : : : "memory");
      asm volatile("vtfmm.tvv mt12, v1, v25" : : : "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(store_mtype), "r"(store_vtype), "r"(tile_vl) : "memory");
      asm volatile("vtmv.v.t v17, %0\naddi %0, %0, 1" : "+r"(tss4) : : "v17", "memory");
      asm volatile("vtmv.v.t v21, %0\naddi %0, %0, 1" : "+r"(tss4) : : "v21", "memory");
      asm volatile("vtmv.v.t v10, %0\naddi %0, %0, 1" : "+r"(tss8) : : "v10", "memory");
      asm volatile("vtmv.v.t v14, %0\naddi %0, %0, 1" : "+r"(tss8) : : "v14", "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt12, v2, v26" : : : "memory");
      asm volatile("vtfmm.tvv mt12, v3, v27" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m8, ta, ma" : : "r"(bias_vl) : "memory");
      asm volatile("vle32.v v0, (%0)\n" "addi %0, %0, %1" : "+r"(bias_ptr) : "i"(c_stride) : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
      asm volatile("vle32.v v24, (%0)\n" "addi %0, %0, %1" : "+r"(bias_ptr) : "i"(c_stride) : "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(store_mtype), "r"(store_vtype), "r"(tile_vl) : "memory");
      asm volatile("vtmv.v.t v18, %0\naddi %0, %0, 1" : "+r"(tss8) : : "v18", "memory");
      asm volatile("vtmv.v.t v22, %0\naddi %0, %0, 1" : "+r"(tss8) : : "v22", "memory");
      asm volatile("vtmv.v.t v11, %0\naddi %0, %0, 1" : "+r"(tss12) : : "v11", "memory");
      asm volatile("vtmv.v.t v15, %0\naddi %0, %0, 1" : "+r"(tss12) : : "v15", "memory");
      asm volatile("vtmv.v.t v19, %0\naddi %0, %0, 1" : "+r"(tss12) : : "v19", "memory");
      asm volatile("vtmv.v.t v23, %0\naddi %0, %0, 1" : "+r"(tss12) : : "v23", "memory");
      asm volatile("vsetvli x0, %0, e32, m8, ta, ma" : : "r"(bias_vl) : "memory");
      asm volatile("vfadd.vv v8, v8, v0" : : : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory");
      asm volatile("vfadd.vv v16, v16, v24" : : : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "memory");
      asm volatile("vse32.v v8, (%0)\n" "addi %0, %0, %1" : "+r"(ptr) : "i"(c_stride) : "memory");
      asm volatile("vse32.v v16, (%0)\n" "addi %0, %0, %1" : "+r"(ptr) : "i"(c_stride) : "memory");
      
      // Rows 4-15: two contiguous 64-element rows per batch.
      // Keep the store epilogue compact for the instruction cache.
      for (uint32_t row = 4; row < TE; row += 4) {
      asm volatile("vle32.v v0, (%0)\n" "addi %0, %0, %1" : "+r"(bias_ptr) : "i"(c_stride) : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(store_mtype), "r"(store_vtype), "r"(tile_vl) : "memory");
      asm volatile("vtmv.v.t v8, %0\naddi %0, %0, 1" : "+r"(tss0) : : "v8", "memory");
      asm volatile("vtmv.v.t v12, %0\naddi %0, %0, 1" : "+r"(tss0) : : "v12", "memory");
      asm volatile("vtmv.v.t v9, %0\naddi %0, %0, 1" : "+r"(tss4) : : "v9", "memory");
      asm volatile("vtmv.v.t v13, %0\naddi %0, %0, 1" : "+r"(tss4) : : "v13", "memory");
      asm volatile("vtmv.v.t v10, %0\naddi %0, %0, 1" : "+r"(tss8) : : "v10", "memory");
      asm volatile("vtmv.v.t v14, %0\naddi %0, %0, 1" : "+r"(tss8) : : "v14", "memory");
      asm volatile("vtmv.v.t v11, %0\naddi %0, %0, 1" : "+r"(tss12) : : "v11", "memory");
      asm volatile("vtmv.v.t v15, %0\naddi %0, %0, 1" : "+r"(tss12) : : "v15", "memory");
      asm volatile("vsetvli x0, %0, e32, m8, ta, ma" : : "r"(bias_vl) : "memory");
      asm volatile("vfadd.vv v8, v8, v0" : : : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory");
      asm volatile("vle32.v v24, (%0)\n" "addi %0, %0, %1" : "+r"(bias_ptr) : "i"(c_stride) : "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(store_mtype), "r"(store_vtype), "r"(tile_vl) : "memory");
      asm volatile("vtmv.v.t v16, %0\naddi %0, %0, 1" : "+r"(tss0) : : "v16", "memory");
      asm volatile("vtmv.v.t v20, %0\naddi %0, %0, 1" : "+r"(tss0) : : "v20", "memory");
      asm volatile("vtmv.v.t v17, %0\naddi %0, %0, 1" : "+r"(tss4) : : "v17", "memory");
      asm volatile("vtmv.v.t v21, %0\naddi %0, %0, 1" : "+r"(tss4) : : "v21", "memory");
      asm volatile("vtmv.v.t v18, %0\naddi %0, %0, 1" : "+r"(tss8) : : "v18", "memory");
      asm volatile("vtmv.v.t v22, %0\naddi %0, %0, 1" : "+r"(tss8) : : "v22", "memory");
      asm volatile("vtmv.v.t v19, %0\naddi %0, %0, 1" : "+r"(tss12) : : "v19", "memory");
      asm volatile("vtmv.v.t v23, %0\naddi %0, %0, 1" : "+r"(tss12) : : "v23", "memory");
      asm volatile("vsetvli x0, %0, e32, m8, ta, ma" : : "r"(bias_vl) : "memory");
      asm volatile("vfadd.vv v16, v16, v24" : : : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "memory");
      asm volatile("vse32.v v8, (%0)\n" "addi %0, %0, %1" : "+r"(ptr) : "i"(c_stride) : "memory");
      asm volatile("vse32.v v16, (%0)\n" "addi %0, %0, %1" : "+r"(ptr) : "i"(c_stride) : "memory");
      }
    }
  }
}
