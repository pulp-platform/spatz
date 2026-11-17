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

#pragma once

#include <stdint.h>

enum {
    TE = 16,
    CHUNK_SIZE = 4 * TE,
    BLOCK_DIM = 2 * TE,
    ROW_BLOCKS = (M_CHUNK + BLOCK_DIM - 1) / BLOCK_DIM,
    COL_BLOCKS = (N_CHUNK + BLOCK_DIM - 1) / BLOCK_DIM,
    TILE_STRIDE = TE * K_CHUNK,
};

enum {
    TILE_INPUTS = TE * (K_CHUNK / 2),
    TILE_BYTES = TILE_INPUTS * sizeof(uint32_t),
    A_TILES = M_CHUNK / TE,
    B_TILES = N_CHUNK / TE,
    A_BYTES = A_TILES * TILE_BYTES,
    B_BYTES = B_TILES * TILE_BYTES,
    C_ELEMENTS = M_CHUNK * N_CHUNK,
    C_BYTES = C_ELEMENTS * sizeof(float),
    A_OFFSET = 0,
    B_OFFSET = A_OFFSET + A_BYTES,
    C_OFFSET = B_OFFSET + B_BYTES,
    BUFFER_BYTES = C_OFFSET + C_BYTES,
    L1_BUFFER_COUNT = MATMUL_BUFFER_COUNT,
    L1_BUFFER_STRIDE = SNRT_TCDM_SIZE / L1_BUFFER_COUNT,
    L1_BYTES = (L1_BUFFER_COUNT - 1) * L1_BUFFER_STRIDE + BUFFER_BYTES,
};

// Left/right operands are packed B^T/A^T. K counts FP16 elements.
// C[vocab][token] is FP32 softmax(B^T * A^T) along vocab rows.
// Fixed M=N=64, TE=16, 1x4 accumulator layout, complete K panel.
void matmul_fp16_fp32(const __fp16 *Atpack, const __fp16 *Bpack, float *C);

#include "transformer.h"

// Softmax inputs are x-max <= 0, so the positive upper clamp is redundant.
// Keep the same degree-5 Taylor seed and six squarings as optimized O-POPE;
// fused kernels may change scheduling, but never the numerical operation set.
#define SOFTMAX_EXP_ASM(x, t) \
    "vfmax.vf " x ", " x ", %[transformer_lo]\n" \
    "vfmul.vf " x ", " x ", %[transformer_scale]\n" \
    "vfmv.v.f " t ", %[transformer_c5]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[transformer_c4]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[transformer_c3]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[transformer_c2]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[transformer_one]\n" \
    "vfmul.vv " t ", " t ", " x "\n" \
    "vfadd.vf " t ", " t ", %[transformer_one]\n" \
    "vfmul.vv " x ", " t ", " t "\n" \
    "vfmul.vv " x ", " x ", " x "\n" \
    "vfmul.vv " x ", " x ", " x "\n" \
    "vfmul.vv " x ", " x ", " x "\n" \
    "vfmul.vv " x ", " x ", " x "\n" \
    "vfmul.vv " x ", " x ", " x "\n"

#define SOFTMAX_EXP_PAIR_ASM(x0, x1, t0, t1) \
    "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
    "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
    "vfmv.v.f " t0 ", %[transformer_c5]\n" \
    "vfmv.v.f " t1 ", %[transformer_c5]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfmul.vv " x0 ", " t0 ", " t0 "\n" \
    "vfmul.vv " x1 ", " t1 ", " t1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n"

// Same pairwise exp operation order, with four independent memory operations
// placed in dependency gaps. The caller uses these slots to store the
// previous ping-pong pair and load the following pair.
#define SOFTMAX_EXP_PAIR_IO_ASM(x0, x1, t0, t1, op0, op1, op2, op3) \
    "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
    op0 \
    "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
    op1 \
    "vfmv.v.f " t0 ", %[transformer_c5]\n" \
    "vfmv.v.f " t1 ", %[transformer_c5]\n" \
    op2 \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    op3 \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfmul.vv " x0 ", " t0 ", " t0 "\n" \
    "vfmul.vv " x1 ", " t1 ", " t1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n"

#define SOFTMAX_EXP_TRIPLE_ASM(x0, x1, x2, t0, t1, t2) \
    "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
    "vfmax.vf " x2 ", " x2 ", %[transformer_lo]\n" \
    "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
    "vfmul.vf " x2 ", " x2 ", %[transformer_scale]\n" \
    "vfmv.v.f " t0 ", %[transformer_c5]\n" \
    "vfmv.v.f " t1 ", %[transformer_c5]\n" \
    "vfmv.v.f " t2 ", %[transformer_c5]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c4]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c3]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c2]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_one]\n" \
    "vfmul.vv " x0 ", " t0 ", " t0 "\n" \
    "vfmul.vv " x1 ", " t1 ", " t1 "\n" \
    "vfmul.vv " x2 ", " t2 ", " t2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n"

// Four independent Horner/squaring chains.  This consumes all eight LMUL=4
// register groups, so callers must finish using their max/sum vectors before
// entering the macro.  It is intended for fixed-size kernels that can keep
// per-quartet sums in scratch memory and combine them after packing.
#if defined(TRANSFORMER_EXP_SCALE32) && TRANSFORMER_EXP_SCALE32
#define SOFTMAX_EXP_QUAD_LAST_SQUARE(x0, x1, x2, x3)
#else
#define SOFTMAX_EXP_QUAD_LAST_SQUARE(x0, x1, x2, x3) \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x3 ", " x3 ", " x3 "\n"
#endif
#define SOFTMAX_EXP_UNROLL4(x0, x1, x2, x3, t0, t1, t2, t3) \
    "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
    "vfmax.vf " x2 ", " x2 ", %[transformer_lo]\n" \
    "vfmax.vf " x3 ", " x3 ", %[transformer_lo]\n" \
    "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
    "vfmul.vf " x2 ", " x2 ", %[transformer_scale]\n" \
    "vfmul.vf " x3 ", " x3 ", %[transformer_scale]\n" \
    "vfmv.v.f " t0 ", %[transformer_c5]\n" \
    "vfmv.v.f " t1 ", %[transformer_c5]\n" \
    "vfmv.v.f " t2 ", %[transformer_c5]\n" \
    "vfmv.v.f " t3 ", %[transformer_c5]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c4]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c4]\n" \
    "vfadd.vf " t3 ", " t3 ", %[transformer_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c3]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c3]\n" \
    "vfadd.vf " t3 ", " t3 ", %[transformer_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c2]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c2]\n" \
    "vfadd.vf " t3 ", " t3 ", %[transformer_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_one]\n" \
    "vfadd.vf " t3 ", " t3 ", %[transformer_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_one]\n" \
    "vfadd.vf " t3 ", " t3 ", %[transformer_one]\n" \
    "vfmul.vv " x0 ", " t0 ", " t0 "\n" \
    "vfmul.vv " x1 ", " t1 ", " t1 "\n" \
    "vfmul.vv " x2 ", " t2 ", " t2 "\n" \
    "vfmul.vv " x3 ", " t3 ", " t3 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x3 ", " x3 ", " x3 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x3 ", " x3 ", " x3 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x3 ", " x3 ", " x3 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x3 ", " x3 ", " x3 "\n" \
    SOFTMAX_EXP_QUAD_LAST_SQUARE(x0, x1, x2, x3)

// buf[vocab][token=64], FP32. Each lane owns a token.
// Three passes: column max; exp + column sum; normalize before final store.
// m4 groups use v8/v12 as ping and v24/v28 as pong.
static inline __attribute__((always_inline)) void softmax_cols_rows(
    float *buf, uint32_t rows) {
  const uintptr_t pairs = rows / 2;
  const uintptr_t quartets = rows / 4;
  asm volatile(
      "vsetvli x0, %[pair_vl], e32, m8, ta, ma\n"
      "mv t0, %[buf]\n"
      "mv t1, %[pairs]\n"
      "vfmv.v.f v0, %[init]\n"
      "1:\n"
      "vle32.v v8, (t0)\n"
      "vfmax.vv v0, v0, v8\n"
      "addi t0, t0, 512\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      "2:\n"
      "li t1, 64\n"
      "vslidedown.vx v8, v0, t1\n"
      "vsetvli x0, %[vl], e32, m4, ta, ma\n"
      "vfmax.vv v0, v0, v8\n"
      "vmv.v.i v4, 0\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "3:\n"
      LOAD_ADVANCE_ASM("v24", "t0", "256")
      LOAD_ADVANCE_ASM("v28", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v4, v4, v12\n"
      STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "addi t1, t1, -1\n"
      "beqz t1, 4f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n"
      "vfadd.vv v4, v4, v28\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      "j 3b\n"
      "4:\n"
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n"
      "vfadd.vv v4, v4, v28\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      RECIP_ASM("v4", "v0", "v16")
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      "5:\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0")
      LOAD_ADVANCE_ASM("v16", "t0", "256")
      STORE_ADVANCE_ASM("v8", "t2", "256")
      NORMALIZE_ASM("v12", "v0")
      LOAD_ADVANCE_ASM("v20", "t0", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      NORMALIZE_ASM("v16", "v0")
      STORE_ADVANCE_ASM("v16", "t2", "256")
      NORMALIZE_ASM("v20", "v0")
      STORE_ADVANCE_ASM("v20", "t2", "256")
      "addi t1, t1, -1\n"
      "bnez t1, 5b\n"
      :
      : [buf] "r"(buf), [vl] "r"((uintptr_t)64),
        [pair_vl] "r"((uintptr_t)128),
        [init] "f"(-__builtin_inff()), [pairs] "r"(pairs),
        [quartets] "r"(quartets), EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

// Fused GEMM-softmax entry: column maxima were accumulated while matrix
// accumulators were drained, so the first read-only pass over buf is omitted.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax(
    float *buf, uint32_t rows, const float *colmax) {
  const uintptr_t quartets = rows / 4;
  asm volatile(
      "vsetvli x0, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v4, 0\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "3:\n"
      LOAD_ADVANCE_ASM("v24", "t0", "256")
      LOAD_ADVANCE_ASM("v28", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n"
      STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "vfadd.vv v4, v4, v12\n"
      "addi t1, t1, -1\n"
      "beqz t1, 4f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      "vfadd.vv v4, v4, v28\n"
      "j 3b\n"
      "4:\n"
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      "vfadd.vv v4, v4, v28\n"
      RECIP_ASM("v4", "v0", "v16")
      // Duplicate the per-token reciprocal into adjacent m4 groups, then
      // normalize/store two contiguous vocabulary rows as one e32,m8 group.
      // This preserves the exact value used by each row while halving the
      // normalization loop's vector instructions and pointer updates.
      "vmv.v.v v4, v0\n"
      "vsetvli zero, %[pair_vl], e32, m8, ta, ma\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "512")
      LOAD_ADVANCE_ASM("v16", "t0", "512")
      "5:\n"
      NORMALIZE_ASM("v8", "v0")
      STORE_ADVANCE_ASM("v8", "t2", "512")
      "addi t1, t1, -1\n"
      "beqz t1, 6f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "512")
      NORMALIZE_ASM("v16", "v0")
      STORE_ADVANCE_ASM("v16", "t2", "512")
      LOAD_ADVANCE_ASM("v16", "t0", "512")
      "j 5b\n"
      "6:\n"
      NORMALIZE_ASM("v16", "v0")
      "vse32.v v16, (t2)\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [vl] "r"((uintptr_t)64),
        [pair_vl] "r"((uintptr_t)128), [quartets] "r"(quartets),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

// P7 form: identical operation order, additionally preserves the pre-reciprocal
// column sum for the log-softmax cross-entropy identity.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_stats(
    float *buf, uint32_t rows, const float *colmax, float *colsum) {
  const uintptr_t pairs = rows / 2, quartets = rows / 4;
  asm volatile(
      "vsetvli x0, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\nvmv.v.i v4, 0\n"
      "mv t0, %[buf]\nmv t2, %[buf]\nmv t1, %[quartets]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "3:\n" LOAD_ADVANCE_ASM("v24", "t0", "256")
      LOAD_ADVANCE_ASM("v28", "t0", "256")
      "vfsub.vv v8, v8, v0\nvfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n" STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256") "vfadd.vv v4, v4, v12\n"
      "addi t1, t1, -1\nbeqz t1, 4f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "vfsub.vv v24, v24, v0\nvfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n" STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256") "vfadd.vv v4, v4, v28\nj 3b\n"
      "4:\nvfsub.vv v24, v24, v0\nvfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n" STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256") "vfadd.vv v4, v4, v28\n"
      "vse32.v v4, (%[sum])\n"
      RECIP_ASM("v4", "v0", "v16")
      "mv t0, %[buf]\nmv t2, %[buf]\nmv t1, %[pairs]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      "5:\n" LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0") STORE_ADVANCE_ASM("v8", "t2", "256")
      "addi t1, t1, -1\nbeqz t1, 6f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      NORMALIZE_ASM("v12", "v0") STORE_ADVANCE_ASM("v12", "t2", "256")
      "j 5b\n6:\n" NORMALIZE_ASM("v12", "v0") "vse32.v v12, (t2)\n"
      :: [buf] "r"(buf), [max] "r"(colmax), [sum] "r"(colsum),
         [vl] "r"((uintptr_t)64), [pairs] "r"(pairs), [quartets] "r"(quartets),
         EXP_INPUTS, RECIP_INPUTS
      : "t0","t1","t2","v0","v1","v2","v3","v4","v5","v6","v7",
        "v8","v9","v10","v11","v12","v13","v14","v15","v16","v17",
        "v18","v19","v20","v21","v22","v23","v24","v25","v26","v27",
        "v28","v29","v30","v31","memory");
}

// Same operations and row-order accumulation as optimized O-POPE, kept in
// one asm block so the loop carries only one pointer and one counter.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_single(
    float *buf, uint32_t rows, const float *colmax) {
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v12, 0\n"
      "mv t0, %[buf]\n"
      "mv t1, %[rows]\n"
      "1:\n"
      "vle32.v v4, (t0)\n"
      "vfsub.vv v4, v4, v0\n"
      SOFTMAX_EXP_ASM("v4", "v8")
      "vfadd.vv v12, v12, v4\n"
      "vse32.v v4, (t0)\n"
      "addi t0, t0, 256\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      RECIP_ASM("v12", "v16", "v20")
      "mv t0, %[buf]\n"
      "mv t1, %[rows]\n"
      "2:\n"
      "vle32.v v4, (t0)\n"
      "vfmul.vv v4, v4, v16\n"
      "vse32.v v4, (t0)\n"
      "addi t0, t0, 256\n"
      "addi t1, t1, -1\n"
      "bnez t1, 2b\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [rows] "r"((uintptr_t)rows),
        [vl] "r"((uintptr_t)64), EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
        "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
        "memory");
}

// Pairwise exp with L1 stores and future loads inserted into the independent
// FP dependency gaps. Row order, accumulation order and approximation are
// identical to softmax_cols_rows_premax.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_io(
    float *buf, uint32_t rows, const float *colmax) {
  const uintptr_t steady = rows / 4 - 1;
  const uintptr_t pairs = rows / 2;
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v4, 0\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      LOAD_ADVANCE_ASM("v24", "t0", "256")
      LOAD_ADVANCE_ASM("v28", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v4, v4, v12\n"
      "mv t1, %[steady]\n"
      "1:\n"
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_IO_ASM(
          "v24", "v28", "v16", "v20",
          STORE_ADVANCE_ASM("v8", "t2", "256"),
          STORE_ADVANCE_ASM("v12", "t2", "256"),
          LOAD_ADVANCE_ASM("v8", "t0", "256"),
          LOAD_ADVANCE_ASM("v12", "t0", "256"))
      "vfadd.vv v4, v4, v24\n"
      "vfadd.vv v4, v4, v28\n"
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_IO_ASM(
          "v8", "v12", "v16", "v20",
          STORE_ADVANCE_ASM("v24", "t2", "256"),
          STORE_ADVANCE_ASM("v28", "t2", "256"),
          LOAD_ADVANCE_ASM("v24", "t0", "256"),
          LOAD_ADVANCE_ASM("v28", "t0", "256"))
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v4, v4, v12\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      // Final odd pair: store the preceding even pair in its dependency gaps.
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_IO_ASM(
          "v24", "v28", "v16", "v20",
          STORE_ADVANCE_ASM("v8", "t2", "256"),
          STORE_ADVANCE_ASM("v12", "t2", "256"), "", "")
      "vfadd.vv v4, v4, v24\n"
      "vfadd.vv v4, v4, v28\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      RECIP_ASM("v4", "v0", "v16")
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[pairs]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      "2:\n"
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0")
      STORE_ADVANCE_ASM("v8", "t2", "256")
      "addi t1, t1, -1\n"
      "beqz t1, 3f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      NORMALIZE_ASM("v12", "v0")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "j 2b\n"
      "3:\n"
      NORMALIZE_ASM("v12", "v0")
      "vse32.v v12, (t2)\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [steady] "r"(steady),
        [pairs] "r"(pairs), [vl] "r"((uintptr_t)64),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

// Two independent column-sum chains remove the loop-carried add dependency.
// The extra final add combines even- and odd-row partial sums before RECIP.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_2sum(
    float *buf, uint32_t rows, const float *colmax) {
  const uintptr_t pairs = rows / 2;
  const uintptr_t quartets = rows / 4;
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v4, 0\n"
      "vmv.v.i v24, 0\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[pairs]\n"
      "1:\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v24, v24, v12\n"
      STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      "vfadd.vv v4, v4, v24\n"
      RECIP_ASM("v4", "v0", "v16")
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      "2:\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0")
      LOAD_ADVANCE_ASM("v16", "t0", "256")
      STORE_ADVANCE_ASM("v8", "t2", "256")
      NORMALIZE_ASM("v12", "v0")
      LOAD_ADVANCE_ASM("v20", "t0", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      NORMALIZE_ASM("v16", "v0")
      STORE_ADVANCE_ASM("v16", "t2", "256")
      NORMALIZE_ASM("v20", "v0")
      STORE_ADVANCE_ASM("v20", "t2", "256")
      "addi t1, t1, -1\n"
      "bnez t1, 2b\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [pairs] "r"(pairs),
        [quartets] "r"(quartets), [vl] "r"((uintptr_t)64),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "memory");
}

// Same numerical operations as softmax_cols_rows_premax, scheduled as one
// e32,m8 vector per two contiguous rows. v0 keeps two independent partial
// sums (lower/upper row positions); they are combined once after all pairs.
// This changes only vector-register layout and issue scheduling.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_m8(
    float *buf, uint32_t rows, float *colmax) {
  const uintptr_t pairs = rows / 2;
  asm volatile(
      // Duplicate the 64-lane maximum so an m8 row pair can subtract it.
      "vsetvli zero, %[half_vl], e32, m4, ta, ma\n"
      "vle32.v v24, (%[max])\n"
      "addi t2, %[max], 256\n"
      "vse32.v v24, (%[max])\n"
      "vse32.v v24, (t2)\n"
      "vsetvli zero, %[pair_vl], e32, m8, ta, ma\n"
      "vle32.v v24, (%[max])\n"
      "vmv.v.i v0, 0\n"
      "mv t0, %[buf]\n"
      "mv t1, %[pairs]\n"
      "1:\n"
      "vle32.v v8, (t0)\n"
      "vfsub.vv v8, v8, v24\n"
      SOFTMAX_EXP_ASM("v8", "v16")
      "vfadd.vv v0, v0, v8\n"
      "vse32.v v8, (t0)\n"
      "addi t0, t0, 512\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      // Add the upper 64-lane partial sum into the lower half.
      "li t1, 64\n"
      "vslidedown.vx v8, v0, t1\n"
      "vsetvli zero, %[half_vl], e32, m4, ta, ma\n"
      "vfadd.vv v0, v0, v8\n"
      RECIP_ASM("v0", "v4", "v8")
      // Duplicate reciprocal for the final m8 pairwise normalization pass.
      "vse32.v v4, (%[max])\n"
      "vse32.v v4, (t2)\n"
      "vsetvli zero, %[pair_vl], e32, m8, ta, ma\n"
      "vle32.v v24, (%[max])\n"
      "mv t0, %[buf]\n"
      "mv t1, %[pairs]\n"
      "2:\n"
      "vle32.v v8, (t0)\n"
      "vfmul.vv v8, v8, v24\n"
      "vse32.v v8, (t0)\n"
      "addi t0, t0, 512\n"
      "addi t1, t1, -1\n"
      "bnez t1, 2b\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [pairs] "r"(pairs),
        [half_vl] "r"((uintptr_t)64), [pair_vl] "r"((uintptr_t)128),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

// Degree-5 exp with three independent m4 dependency chains. This preserves
// row-order accumulation exactly while spacing every dependent FP operation
// by two unrelated vector instructions.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_m3(
    float *buf, uint32_t rows, const float *colmax) {
  const uintptr_t triples = rows / 3;
  const uintptr_t pairs = rows / 2;
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v4, 0\n"
      "mv t0, %[buf]\n"
      "mv t1, %[triples]\n"
      "1:\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      LOAD_ADVANCE_ASM("v16", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      "vfsub.vv v16, v16, v0\n"
      SOFTMAX_EXP_TRIPLE_ASM("v8", "v12", "v16", "v20", "v24", "v28")
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v4, v4, v12\n"
      "vfadd.vv v4, v4, v16\n"
      "addi t2, t0, -768\n"
      STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "vse32.v v16, (t2)\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      RECIP_ASM("v4", "v0", "v20")
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[pairs]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      "2:\n"
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0")
      STORE_ADVANCE_ASM("v8", "t2", "256")
      "addi t1, t1, -1\n"
      "beqz t1, 3f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      NORMALIZE_ASM("v12", "v0")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "j 2b\n"
      "3:\n"
      NORMALIZE_ASM("v12", "v0")
      "vse32.v v12, (t2)\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [triples] "r"(triples),
        [pairs] "r"(pairs), [vl] "r"((uintptr_t)64),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

static inline __attribute__((always_inline)) void softmax_cols(float *buf) {
  softmax_cols_rows(buf, 64);
}
