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

// Specialized FP16 VME matmul kernel for M=N=64, K=64 or K=128.
// TE=16, CE=8, and eight logical tiles.

#include "matmul.h"

__attribute__((noinline, aligned(64))) void matmul_fp16(
    const __fp16 *Apack, const __fp16 *Bpack, __fp16 *C,
    uint32_t M, uint32_t N, uint32_t K)
{
    (void)M;
    (void)N;
    const uint32_t tile_stride = TE * K;

    uintptr_t a0p = (uintptr_t)Apack;
    uintptr_t a1p = (uintptr_t)(Apack + tile_stride);
    uintptr_t a2p = (uintptr_t)(Apack + 2 * tile_stride);
    uintptr_t a3p = (uintptr_t)(Apack + 3 * tile_stride);
    uintptr_t b0p = (uintptr_t)Bpack;
    uintptr_t b1p = (uintptr_t)(Bpack + tile_stride);

    uintptr_t tss;
    uintptr_t cp = (uintptr_t)C;
    uintptr_t block_counter = 2;
    uintptr_t loop_counter;
    const uintptr_t matrix_mtype = (TE << 10) | (2u << 5) | 1u;
    const uintptr_t matrix_vtype = 0xC8;  // e16, m1, ta, ma
    const uintptr_t middle_k_groups = (uintptr_t)K / 4 - 5;

    asm volatile(
        // K-Group 1
        "msetmtype %[mtype], %[vtype]\n"  // set sew=16, set tm=TE, tk=2, twiden=1
        "li %[loop], 16\n"
        "msettn x0, %[loop]\n"            // msetmtype resets tn, so set full tn=TE

        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v4, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v20, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v8, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v17, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v12, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v21, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v1, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtzero mt0\n"

        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16 + v4*v20
        "vle16.v v5, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt0, v8, v17\n"  // mt0 += v8*v17 + v12*v21
        "vle16.v v9, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v13, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v2, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v1, v16\n"  // mt4 += v1*v16 + v5*v20
        "vle16.v v6, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v10, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v14, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt4, v9, v17\n"  // mt4 += v9*v17 + v13*v21
        "vle16.v v3, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v7, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtzero mt8\n"

        // vtfmm mt8
        "vtfmm.tvv mt8, v2,  v16\n"  // mt8 += v2*v16 + v6*v20
        "vle16.v v11, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v15, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtfmm.tvv mt8, v10,  v17\n"  // mt8 += v10*v17 + v14*v21
        "vle16.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v28, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v3,  v16\n"  // mt12 += v3*v16 + v7*v20
        "vle16.v v25, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v29, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
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
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v4, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt10, v10,  v25\n"  // mt10 += v10*v25 + v14*v29
        "vle16.v v20, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v8, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v17, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vtzero mt14\n"

        // vtfmm mt14
        "vtfmm.tvv mt14, v3,  v24\n"  // mt14 += v3*v24 + v7*v28
        "vle16.v v12, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v21, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v1, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt14, v11,  v25\n"  // mt14 += v11*v25 + v15*v29

        "9:\n"
        // K-Group 1 to N-2
        // vtfmm mt0
        "mv %[loop], %[middle_k_groups]\n"
        "beqz %[loop], 3f\n"
        "1:\n"
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16 + v4*v20
        "vle16.v v5, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt0, v8, v17\n"  // mt0 += v8*v17 + v12*v21
        "vle16.v v9, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v13, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v2, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v1, v16\n"  // mt4 += v1*v16 + v5*v20
        "vle16.v v6, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v10, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v14, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt4, v9, v17\n"  // mt4 += v9*v17 + v13*v21
        "vle16.v v3, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v7, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"

        // vtfmm mt8
        "vtfmm.tvv mt8, v2,  v16\n"  // mt8 += v2*v16 + v6*v20
        "vle16.v v11, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v15, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtfmm.tvv mt8, v10,  v17\n"  // mt8 += v10*v17 + v14*v21
        "vle16.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v28, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v3,  v16\n"  // mt12 += v3*v16 + v7*v20
        "vle16.v v25, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v29, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vtfmm.tvv mt12, v11,  v17\n"  // mt12 += v11*v17 + v15*v21

        // vtfmm mt2
        "vtfmm.tvv mt2, v0, v24\n"  // mt2 += v0*v24 + v4*v28
        "vtfmm.tvv mt2, v8, v25\n"  // mt2 += v8*v25 + v12*v29

        // vtfmm mt6
        "vtfmm.tvv mt6, v1, v24\n"  // mt6 += v1*v24 + v5*v28
        "vtfmm.tvv mt6, v9, v25\n"  // mt6 += v9*v25 + v13*v29

        // vtfmm mt10
        "vtfmm.tvv mt10, v2,  v24\n"  // mt10 += v2*v24 + v6*v28
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v4, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt10, v10,  v25\n"  // mt10 += v10*v25 + v14*v29
        "vle16.v v20, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v8, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v17, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"

        // vtfmm mt14
        "vtfmm.tvv mt14, v3,  v24\n"  // mt14 += v3*v24 + v7*v28
        "vle16.v v12, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v21, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v1, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt14, v11,  v25\n"  // mt14 += v11*v25 + v15*v29

        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 1b\n"

        "3:\n"
        "vle16.v v1, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "addi %[a1p], %[a1p], -32\n"

        // K-Group N-1 & K-Group N
        // vtfmm mt0
        "vle16.v v18, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v5, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16 + v4*v20
        "vle16.v v22, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v9, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v19, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v13, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt0, v8, v17\n"  // mt0 += v8*v17 + v12*v21
        "vle16.v v23, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v2, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v24, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vtfmm.tvv mt0, v1, v18\n"  // mt0 += v1*v18 + v5*v22
        "vle16.v v6, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v28, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v10, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v25, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vtfmm.tvv mt0, v9, v19\n"  // mt0 += v9*v19 + v13*v23
        "vle16.v v14, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v29, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v3, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt0, v2,  v24\n"  // mt0 += v2*v24 + v6*v28
        "vle16.v v26, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v7, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v30, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v11, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt0, v10,  v25\n"  // mt0 += v10*v25 + v14*v29
        "vle16.v v27, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v15, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v31, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vtfmm.tvv mt0, v3,  v26\n"  // mt0 += v3*v26 + v7*v30
        "vle16.v v0, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v4, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v8, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt0, v11,  v27\n"  // mt0 += v11*v27 + v15*v31

        // vtfmm mt4 || vtse mt0
        "mv %[tss], x0\n"
        "vle16.v v12, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v1, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v5, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt4, v0, v16\n"  // mt4 += v0*v16 + v4*v20
        "vle16.v v9, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v13, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v2, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt4, v8, v17\n"  // mt4 += v8*v17 + v12*v21
        "vle16.v v6, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v10, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v14, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v3, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt4, v1, v18\n"  // mt4 += v1*v18 + v5*v22
        "vle16.v v7, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v11, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v15, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt4, v9, v19\n"  // mt4 += v9*v19 + v13*v23
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2,  v24\n"  // mt4 += v2*v24 + v6*v28
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10,  v25\n"  // mt4 += v10*v25 + v14*v29
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3,  v26\n"  // mt4 += v3*v26 + v7*v30
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11,  v27\n"  // mt4 += v11*v27 + v15*v31
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt8 || vtse mt4
        "lui %[tss], 0x20000\n"
        "vle16.v v0, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v4, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v8, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt8, v0, v16\n"  // mt8 += v0*v16 + v4*v20
        "vle16.v v12, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v1, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v5, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v9, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt8, v8, v17\n"  // mt8 += v8*v17 + v12*v21
        "vle16.v v13, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v2, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v6, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt8, v1, v18\n"  // mt8 += v1*v18 + v5*v22
        "vle16.v v10, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v14, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v3, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v7, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt8, v9, v19\n"  // mt8 += v9*v19 + v13*v23
        "vle16.v v11, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v15, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v24\n"  // mt8 += v2*v24 + v6*v28
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v25\n"  // mt8 += v10*v25 + v14*v29
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v26\n"  // mt8 += v3*v26 + v7*v30
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"  // mt8 += v11*v27 + v15*v31
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12 || vtse mt8
        "lui %[tss], 0x40000\n"
        "vle16.v v0, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v4, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v8, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v12, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtfmm.tvv mt12, v0, v16\n"  // mt12 += v0*v16 + v4*v20
        "vle16.v v1, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v5, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v9, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtfmm.tvv mt12, v8, v17\n"  // mt12 += v8*v17 + v12*v21
        "vle16.v v13, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v2, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v6, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v10, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtfmm.tvv mt12, v1, v18\n"  // mt12 += v1*v18 + v5*v22
        "vle16.v v14, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v3, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v7, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtfmm.tvv mt12, v9, v19\n"  // mt12 += v9*v19 + v13*v23
        "vle16.v v11, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v15, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v24\n"  // mt12 += v2*v24 + v6*v28
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v25\n"  // mt12 += v10*v25 + v14*v29
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v26\n"  // mt12 += v3*v26 + v7*v30
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"  // mt12 += v11*v27 + v15*v31
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt14 || vtse mt12
        "lui %[tss], 0x60000\n"
        "vle16.v v16, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v20, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v17, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v21, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vtfmm.tvv mt14, v0, v16\n"  // mt14 += v0*v16 + v4*v20
        "vle16.v v18, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v22, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v19, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vtfmm.tvv mt14, v8, v17\n"  // mt14 += v8*v17 + v12*v21
        "vle16.v v23, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v28, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v25, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vtfmm.tvv mt14, v1, v18\n"  // mt14 += v1*v18 + v5*v22
        "vle16.v v29, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v26, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v30, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vtfmm.tvv mt14, v9, v19\n"  // mt14 += v9*v19 + v13*v23
        "vle16.v v27, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v31, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt14, v2,  v24\n"  // mt14 += v2*v24 + v6*v28
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt14, v10,  v25\n"  // mt14 += v10*v25 + v14*v29
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt14, v3,  v26\n"  // mt14 += v3*v26 + v7*v30
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt14, v11,  v27\n"  // mt14 += v11*v27 + v15*v31
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt10 || vtse mt14
        // C: (row 64, col 0) -> (row 48, col 16).
        "addi %[cp], %[cp], -2016\n"
        "addi %[a2p], %[a2p], -512\n"
        "lui %[tss], 0x70000\n"
        "vle16.v v0, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v4, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v8, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v12, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt10, v0, v16\n"  // mt10 += v0*v16 + v4*v20
        "vle16.v v1, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v5, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v9, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt10, v8, v17\n"  // mt10 += v8*v17 + v12*v21
        "vle16.v v13, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v2, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v6, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v10, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt10, v1, v18\n"  // mt10 += v1*v18 + v5*v22
        "vle16.v v14, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v3, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v7, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt10, v9, v19\n"  // mt10 += v9*v19 + v13*v23
        "vle16.v v11, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v15, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt10, v2,  v24\n"  // mt10 += v2*v24 + v6*v28
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt10, v10,  v25\n"  // mt10 += v10*v25 + v14*v29
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt10, v3,  v26\n"  // mt10 += v3*v26 + v7*v30
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt10, v11,  v27\n"  // mt10 += v11*v27 + v15*v31
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt6 || vtse mt10
        // C: (row 64, col 16) -> (row 32, col 16).
        "lui %[loop], 1\n"
        "sub %[cp], %[cp], %[loop]\n"
        "lui %[tss], 0x50000\n"
        "addi %[a1p], %[a1p], -512\n"
        "vle16.v v0, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v4, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v8, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v12, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt6, v0, v16\n"  // mt6 += v0*v16 + v4*v20
        "vle16.v v1, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v5, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v9, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt6, v8, v17\n"  // mt6 += v8*v17 + v12*v21
        "vle16.v v13, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v2, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v6, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v10, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt6, v1, v18\n"  // mt6 += v1*v18 + v5*v22
        "vle16.v v14, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v3, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v7, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtfmm.tvv mt6, v9, v19\n"  // mt6 += v9*v19 + v13*v23
        "vle16.v v11, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v15, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt6, v2,  v24\n"  // mt6 += v2*v24 + v6*v28
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt6, v10,  v25\n"  // mt6 += v10*v25 + v14*v29
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt6, v3,  v26\n"  // mt6 += v3*v26 + v7*v30
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt6, v11,  v27\n"  // mt6 += v11*v27 + v15*v31
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt2 || vtse mt6
        // C: (row 48, col 16) -> (row 16, col 16).
        "lui %[loop], 1\n"
        "sub %[cp], %[cp], %[loop]\n"
        "lui %[tss], 0x30000\n"
        "addi %[a0p], %[a0p], -512\n"
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v4, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v8, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v12, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt2, v0, v16\n"  // mt2 += v0*v16 + v4*v20
        "vle16.v v1, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v5, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v9, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt2, v8, v17\n"  // mt2 += v8*v17 + v12*v21
        "vle16.v v13, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v2, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v6, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v10, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt2, v1, v18\n"  // mt2 += v1*v18 + v5*v22
        "vle16.v v14, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v3, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v7, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtfmm.tvv mt2, v9, v19\n"  // mt2 += v9*v19 + v13*v23
        "vle16.v v11, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v15, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt2, v2,  v24\n"  // mt2 += v2*v24 + v6*v28
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt2, v10,  v25\n"  // mt2 += v10*v25 + v14*v29
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt2, v3,  v26\n"  // mt2 += v3*v26 + v7*v30
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt2, v11,  v27\n"  // mt2 += v11*v27 + v15*v31
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        // C: (row 32, col 16) -> (row 0, col 16) for mt2.
        "lui %[loop], 1\n"
        "sub %[cp], %[cp], %[loop]\n"
        "lui %[tss], 0x10000\n"
        // block transition
        "addi %[blocks], %[blocks], -1\n"
        "beqz %[blocks], 7f\n"
        "addi %[loop], %[middle_k_groups], 5\n"
        "slli %[loop], %[loop], 7\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "sub %[a1p], %[a1p], %[loop]\n"
        "sub %[a2p], %[a2p], %[loop]\n"
        "sub %[a3p], %[a3p], %[loop]\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "add %[b1p], %[b1p], %[loop]\n"

        // vtfmm mt0
        "vtzero mt0\n"
        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v4, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v20, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16 + v4*v20
        "vle16.v v8, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v17, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v12, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v21, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vtfmm.tvv mt0, v8, v17\n"  // mt0 += v8*v17 + v12*v21
        "vle16.v v1, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v5, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v9, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"

        // vtfmm mt4
        "vtzero mt4\n"
        "vtfmm.tvv mt4, v1, v16\n"  // mt4 += v1*v16 + v5*v20
        "vle16.v v13, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vle16.v v2, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v6, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vtfmm.tvv mt4, v9, v17\n"  // mt4 += v9*v17 + v13*v21
        "vle16.v v10, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v14, (%[a2p])\n" "addi %[a2p], %[a2p], 32\n"
        "vle16.v v3, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v7, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"

        // vtfmm mt8
        "vtzero mt8\n"
        "vtfmm.tvv mt8, v2,  v16\n"  // mt8 += v2*v16 + v6*v20
        "vle16.v v11, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vle16.v v15, (%[a3p])\n" "addi %[a3p], %[a3p], 32\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v17\n"  // mt8 += v10*v17 + v14*v21
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        "vtzero mt12\n"
        "vtfmm.tvv mt12, v3,  v16\n"  // mt12 += v3*v16 + v7*v20
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v17\n"  // mt12 += v11*v17 + v15*v21
        "vle16.v v24, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v28, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v25, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"
        "vle16.v v29, (%[b1p])\n" "addi %[b1p], %[b1p], 32\n"

        // vtfmm mt6
        "vtzero mt6\n"
        "vtfmm.tvv mt6, v1, v24\n"  // mt6 += v1*v24 + v5*v28
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt6, v9, v25\n"  // mt6 += v9*v25 + v13*v29
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"

        "vtzero mt2\n"
        "vtfmm.tvv mt2, v0, v24\n"  // mt2 += v0*v24 + v4*v28
        "vtfmm.tvv mt2, v8, v25\n"  // mt2 += v8*v25 + v12*v29

        "vle16.v v0, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v16, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v4, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"

        // vtfmm mt10
        "vtzero mt10\n"
        "vtfmm.tvv mt10, v2,  v24\n"  // mt10 += v2*v24 + v6*v28
        "vle16.v v20, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v8, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v17, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vtfmm.tvv mt10, v10,  v25\n"  // mt10 += v10*v25 + v14*v29
        "vle16.v v12, (%[a0p])\n" "addi %[a0p], %[a0p], 32\n"
        "vle16.v v21, (%[b0p])\n" "addi %[b0p], %[b0p], 32\n"
        "vle16.v v1, (%[a1p])\n" "addi %[a1p], %[a1p], 32\n"
        "vtzero mt14\n"

        // vtfmm mt14
        "vtfmm.tvv mt14, v3,  v24\n"  // mt14 += v3*v24 + v7*v28
        "vtfmm.tvv mt14, v11,  v25\n"  // mt14 += v11*v25 + v15*v29
        // Old mt2 store ended at (row 16, col 16); next block starts at col 32.
        "addi %[cp], %[cp], -2016\n"

        "j 9b\n"

        // Final block: compact TM-row mt2 store loop.
        "7:\n"
        "li %[loop], 16\n"
        "2:\n"
        "vtse16 %[tss], (%[cp])\n" "addi %[cp], %[cp], 128\n" "addi %[tss], %[tss], 1\n"
        "addi %[loop], %[loop], -1\n"
        "bnez %[loop], 2b\n"
        "8:\n"

        :
          [a0p] "+r"(a0p),
          [a1p] "+r"(a1p),
          [a2p] "+r"(a2p),
          [a3p] "+r"(a3p),
          [b0p] "+r"(b0p),
          [b1p] "+r"(b1p),
          [tss] "=&r"(tss),
          [cp] "+r"(cp),
          [blocks] "+r"(block_counter),
          [loop] "=&r"(loop_counter)
        :
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype),
          [middle_k_groups] "r"(middle_k_groups)
        : "memory");
}
