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

// fp32 optimal kernel for TE=16, CE=8, and 1x4 tiles layout.

#include "matmul.h"

__attribute__((noinline, aligned(64))) void matmul_fp32_crossblock(
    const float *At, const float *B, float *C)
{
  const uintptr_t c_stride = 64 * sizeof(float);
  const uintptr_t mtype = (TE << 10) | (2u << 5) | 1u;
  const uintptr_t vtype = 0xD0; // e32, m1: one vector per tile row
  const uintptr_t tile_vl = TE;
  const uintptr_t group_vl = 4 * TE;
  const uintptr_t operand_bytes = TE * sizeof(float);
  const uintptr_t operand_group_bytes = 4 * TE * sizeof(float);

  // Keep pointer updates inside each asm region to avoid scalar spills and tail merging.
  // Pair rows 0/16, then rows 32/48. No generic block-coordinate division.
  #pragma clang loop unroll(disable)
  for (uint32_t nb = 0; nb < 4; nb += 2) {
    const uint32_t mb0 = nb * TE;
    uintptr_t old_c_ptr;
    {
      // First block: complete rows0-11 and retain rows12-15.
      uintptr_t a0p = (uintptr_t)(At + mb0 * K_CHUNK);
      uintptr_t b0p = (uintptr_t)B;
      uintptr_t b1p = (uintptr_t)(B + TE * K_CHUNK);
      uintptr_t b2p = (uintptr_t)(B + 2 * TE * K_CHUNK);
      uintptr_t b3p = (uintptr_t)(B + 3 * TE * K_CHUNK);
      uintptr_t ptr = (uintptr_t)(C + mb0 * 64);
      uintptr_t tss0 = 0x00000000u;
      uintptr_t tss4 = 0x20000000u;
      uintptr_t tss8 = 0x40000000u;
      uintptr_t tss12 = 0x60000000u;

      asm volatile(
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v0, (%[a0p])\n addi %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n addi %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtzero mt0\n"
        "vtzero mt4\n"
        "vtzero mt8\n"
        "vtzero mt12\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v20, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vle32.v v21, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vle32.v v22, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vle32.v v23, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v0, v20\n"
        "vle32.v v24, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v21\n"
        "vle32.v v25, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v22\n"
        "vle32.v v26, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v23\n"
        "vle32.v v27, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v0, v24\n"
        "vle32.v v28, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v1, v25\n"
        "vle32.v v29, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v2, v26\n"
        "vle32.v v30, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v3, v27\n"
        "vle32.v v31, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v0, v28\n"
        "vle32.v v0, (%[a0p])\n addi %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16, (%[b0p])\n addi %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v1, v29\n"
        "vle32.v v1, (%[a0p])\n addi %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17, (%[b0p])\n addi %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v2, v30\n"
        "vle32.v v2, (%[a0p])\n addi %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18, (%[b0p])\n addi %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v3, v31\n"
        "vle32.v v3, (%[a0p])\n addi %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v19, (%[b0p])\n addi %[b0p], %[b0p], %[operand_bytes]\n"
        : [a0p] "+&r"(a0p),
          [b0p] "+&r"(b0p),
          [b1p] "+&r"(b1p),
          [b2p] "+&r"(b2p),
          [b3p] "+&r"(b3p)
        : [group_vl] "r"(group_vl),
          [operand_group_bytes] "i"(operand_group_bytes),
          [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl),
          [operand_bytes] "i"(operand_bytes)
        : "memory", "v0", "v1", "v2", "v3", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");

      #pragma clang loop unroll(disable)
      for (uint32_t remaining = K_CHUNK / 4 - 2;
           remaining != 0; --remaining) {
      asm volatile(
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v20, (%[b1p])\n addi %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtfmm.tvv mt4, v0, v20\n"
        "vtfmm.tvv mt4, v1, v21\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v24, (%[b2p])\n addi %[b2p], %[b2p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtfmm.tvv mt4, v2, v22\n"
        "vtfmm.tvv mt4, v3, v23\n"
        "vtfmm.tvv mt8, v0, v24\n"
        "vtfmm.tvv mt8, v1, v25\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v28, (%[b3p])\n addi %[b3p], %[b3p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n addi %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtfmm.tvv mt8, v2, v26\n"
        "vtfmm.tvv mt8, v3, v27\n"
        "vtfmm.tvv mt12, v0, v28\n"
        "vtfmm.tvv mt12, v1, v29\n"
        "vtfmm.tvv mt12, v2, v30\n"
        "vtfmm.tvv mt12, v3, v31\n"
        : [b1p] "+&r"(b1p),
          [b2p] "+&r"(b2p),
          [b3p] "+&r"(b3p),
          [b0p] "+&r"(b0p)
        : [group_vl] "r"(group_vl),
          [operand_group_bytes] "i"(operand_group_bytes),
          [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl)
        : "memory", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "v16", "v17", "v18", "v19");
      // v0-v3 can be replaced only after all four tiles consume them.
      asm volatile(
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v0, (%[a0p])\n addi %[a0p], %[a0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        : [a0p] "+&r"(a0p)
        : [group_vl] "r"(group_vl),
          [operand_group_bytes] "i"(operand_group_bytes),
          [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl)
        : "memory", "v0", "v1", "v2", "v3");
      }

      // Final K group: move completed tiles while the next tile computes.
      asm volatile(
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v20, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vle32.v v21, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vle32.v v22, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vle32.v v23, (%[b1p])\n"
        "vtfmm.tvv mt4, v0, v20\n"
        "vle32.v v24, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtmv.v.t v4, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v1, v21\n"
        "vle32.v v25, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtmv.v.t v8, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v2, v22\n"
        "vle32.v v26, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtmv.v.t v12, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v3, v23\n"
        "vle32.v v27, (%[b2p])\n"
        "vtmv.v.t v16, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt8, v0, v24\n"
        "vle32.v v28, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtmv.v.t v5, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v1, v25\n"
        "vle32.v v29, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtmv.v.t v9, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v2, v26\n"
        "vle32.v v30, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtmv.v.t v13, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v3, v27\n"
        "vle32.v v31, (%[b3p])\n"
        "vtmv.v.t v17, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt12, v0, v28\n"
        "vtmv.v.t v6, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v1, v29\n"
        "vtmv.v.t v10, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v2, v30\n"
        "vtmv.v.t v14, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v3, v31\n"
        "vtmv.v.t v18, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v7, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v11, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        // Rows 0-1 are complete in v4-v7/v8-v11; store them before moving rows 2-3.
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vse32.v v4, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "vse32.v v8, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtmv.v.t v15, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v19, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vse32.v v12, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "vse32.v v16, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        : [b1p] "+&r"(b1p),
          [b2p] "+&r"(b2p),
          [tss0] "+&r"(tss0),
          [b3p] "+&r"(b3p),
          [tss4] "+&r"(tss4),
          [tss8] "+&r"(tss8),
          [tss12] "+&r"(tss12),
          [ptr] "+&r"(ptr)
        : [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl),
          [operand_bytes] "i"(operand_bytes),
          [group_vl] "r"(group_vl),
          [c_stride] "i"(c_stride)
        : "memory", "v20", "v21", "v22", "v23", "v24", "v4", "v25", "v8", "v26", "v12", "v27", "v16", "v28", "v5", "v29", "v9", "v30", "v13", "v31", "v17", "v6", "v10", "v14", "v18", "v7", "v11", "v15", "v19");

      // Rows 4-11: four contiguous 64-element rows per batch.
      do {
      asm volatile(
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtmv.v.t v4, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v5, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v6, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v7, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v8, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v9, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v10, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v11, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vse32.v v4, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "vse32.v v8, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtmv.v.t v12, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v13, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v14, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v15, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v16, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v17, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v18, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v19, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vse32.v v12, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "vse32.v v16, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        : [tss0] "+&r"(tss0),
          [tss4] "+&r"(tss4),
          [tss8] "+&r"(tss8),
          [tss12] "+&r"(tss12),
          [ptr] "+&r"(ptr)
        : [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl),
          [group_vl] "r"(group_vl),
          [c_stride] "i"(c_stride)
        : "memory", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19");
      } while (tss0 < TE - 4);
      
      old_c_ptr = ptr;
    }
    {
      // Second block: alternate paired operand loads / paired tile moves with m1 stores.
      // Retain four old rows per tile until they are captured before vtzero.
      const uint32_t mb1 = (nb + 1) * TE;
      uintptr_t a0p = (uintptr_t)(At + mb1 * K_CHUNK);
      uintptr_t b0p = (uintptr_t)B;
      uintptr_t b1p = (uintptr_t)(B + TE * K_CHUNK);
      uintptr_t b2p = (uintptr_t)(B + 2 * TE * K_CHUNK);
      uintptr_t b3p = (uintptr_t)(B + 3 * TE * K_CHUNK);
      uintptr_t ptr = (uintptr_t)(C + mb1 * 64);
      uintptr_t tss0 = 0x00000000u;
      uintptr_t tss4 = 0x20000000u;
      uintptr_t tss8 = 0x40000000u;
      uintptr_t tss12 = 0x60000000u;

      // Each buffer holds four rows of one old tile. A/B remain in v0-v3/v16-v31.
      uintptr_t old_tss0 = 12, old_tss4 = 0x2000000cu;
      uintptr_t old_tss8 = 0x4000000cu, old_tss12 = 0x6000000cu;
      uintptr_t old_store = old_c_ptr;
      asm volatile(
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v0, (%[a0p])\n addi %[a0p], %[a0p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n addi %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtmv.v.t v4, %[old_tss0]\n addi %[old_tss0], %[old_tss0], 1\n"
        "vtmv.v.t v5, %[old_tss0]\n addi %[old_tss0], %[old_tss0], 1\n"
        "vtmv.v.t v6, %[old_tss0]\n addi %[old_tss0], %[old_tss0], 1\n"
        "vtmv.v.t v7, %[old_tss0]\n"
        : [a0p] "+&r"(a0p),
          [b0p] "+&r"(b0p),
          [old_tss0] "+&r"(old_tss0)
        : [group_vl] "r"(group_vl),
          [operand_group_bytes] "i"(operand_group_bytes),
          [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl)
        : "memory", "v0", "v1", "v2", "v3", "v16", "v17", "v18", "v19", "v4", "v5", "v6", "v7");
      // Old mt0 is fully captured; reuse its accumulator for the new block.
      asm volatile(
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v20, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v21, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vse32.v v4, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vtmv.v.t v8, %[old_tss4]\n addi %[old_tss4], %[old_tss4], 1\n"
        "vtmv.v.t v9, %[old_tss4]\n addi %[old_tss4], %[old_tss4], 1\n"
        "vse32.v v5, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vle32.v v22, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v23, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vse32.v v6, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtmv.v.t v10, %[old_tss4]\n addi %[old_tss4], %[old_tss4], 1\n"
        "vtmv.v.t v11, %[old_tss4]\n"
        "vse32.v v7, (%[old_store])\n"
        : [b1p] "+&r"(b1p),
          [old_store] "+&r"(old_store),
          [old_tss4] "+&r"(old_tss4)
        : [operand_bytes] "i"(operand_bytes),
          [c_stride] "i"(c_stride)
        : "memory", "v20", "v8", "v21", "v9", "v22", "v10", "v23", "v11");
      // Old mt4 is fully captured; reuse its accumulator for the new block.
      asm volatile(
        "vtzero mt4\n"
        : 
        : 
        : "memory");
      old_store = old_c_ptr + 1 * TE * sizeof(float);
      asm volatile(
        "vtfmm.tvv mt4, v0, v20\n"
        "vle32.v v24, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vle32.v v25, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vse32.v v8, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt4, v1, v21\n"
        "vtmv.v.t v4, %[old_tss8]\n addi %[old_tss8], %[old_tss8], 1\n"
        "vtmv.v.t v5, %[old_tss8]\n addi %[old_tss8], %[old_tss8], 1\n"
        "vse32.v v9, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt4, v2, v22\n"
        "vle32.v v26, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vle32.v v27, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vse32.v v10, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt4, v3, v23\n"
        "vtmv.v.t v6, %[old_tss8]\n addi %[old_tss8], %[old_tss8], 1\n"
        "vtmv.v.t v7, %[old_tss8]\n"
        "vse32.v v11, (%[old_store])\n"
        : [b2p] "+&r"(b2p),
          [old_store] "+&r"(old_store),
          [old_tss8] "+&r"(old_tss8)
        : [operand_bytes] "i"(operand_bytes),
          [c_stride] "i"(c_stride)
        : "memory", "v24", "v4", "v25", "v5", "v26", "v6", "v27", "v7");
      // Old mt8 is fully captured; reuse its accumulator for the new block.
      asm volatile(
        "vtzero mt8\n"
        : 
        : 
        : "memory");
      old_store = old_c_ptr + 2 * TE * sizeof(float);
      asm volatile(
        "vtfmm.tvv mt8, v0, v24\n"
        "vle32.v v28, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vle32.v v29, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vse32.v v4, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt8, v1, v25\n"
        "vtmv.v.t v8, %[old_tss12]\n addi %[old_tss12], %[old_tss12], 1\n"
        "vtmv.v.t v9, %[old_tss12]\n addi %[old_tss12], %[old_tss12], 1\n"
        "vse32.v v5, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt8, v2, v26\n"
        "vle32.v v30, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vle32.v v31, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vse32.v v6, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt8, v3, v27\n"
        "vtmv.v.t v10, %[old_tss12]\n addi %[old_tss12], %[old_tss12], 1\n"
        "vtmv.v.t v11, %[old_tss12]\n"
        "vse32.v v7, (%[old_store])\n"
        : [b3p] "+&r"(b3p),
          [old_store] "+&r"(old_store),
          [old_tss12] "+&r"(old_tss12)
        : [operand_bytes] "i"(operand_bytes),
          [c_stride] "i"(c_stride)
        : "memory", "v28", "v8", "v29", "v9", "v30", "v10", "v31", "v11");
      // Old mt12 is fully captured; reuse its accumulator for the new block.
      asm volatile(
        "vtzero mt12\n"
        : 
        : 
        : "memory");
      old_store = old_c_ptr + 3 * TE * sizeof(float);
      asm volatile(
        "vtfmm.tvv mt12, v0, v28\n"
        "vle32.v v0, (%[a0p])\n addi %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16, (%[b0p])\n addi %[b0p], %[b0p], %[operand_bytes]\n"
        "vse32.v v8, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt12, v1, v29\n"
        "vle32.v v1, (%[a0p])\n addi %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17, (%[b0p])\n addi %[b0p], %[b0p], %[operand_bytes]\n"
        "vse32.v v9, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt12, v2, v30\n"
        "vle32.v v2, (%[a0p])\n addi %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18, (%[b0p])\n addi %[b0p], %[b0p], %[operand_bytes]\n"
        "vse32.v v10, (%[old_store])\n addi %[old_store], %[old_store], %[c_stride]\n"
        "vtfmm.tvv mt12, v3, v31\n"
        "vle32.v v3, (%[a0p])\n addi %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v19, (%[b0p])\n addi %[b0p], %[b0p], %[operand_bytes]\n"
        "vse32.v v11, (%[old_store])\n"
        : [a0p] "+&r"(a0p),
          [b0p] "+&r"(b0p),
          [old_store] "+&r"(old_store)
        : [operand_bytes] "i"(operand_bytes),
          [c_stride] "i"(c_stride)
        : "memory", "v0", "v16", "v1", "v17", "v2", "v18", "v3", "v19");

      #pragma clang loop unroll(disable)
      for (uint32_t remaining = K_CHUNK / 4 - 2;
           remaining != 0; --remaining) {
      asm volatile(
        "vtfmm.tvv mt0, v0, v16\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v20, (%[b1p])\n addi %[b1p], %[b1p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vtfmm.tvv mt4, v0, v20\n"
        "vtfmm.tvv mt4, v1, v21\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v24, (%[b2p])\n addi %[b2p], %[b2p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtfmm.tvv mt4, v2, v22\n"
        "vtfmm.tvv mt4, v3, v23\n"
        "vtfmm.tvv mt8, v0, v24\n"
        "vtfmm.tvv mt8, v1, v25\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v28, (%[b3p])\n addi %[b3p], %[b3p], %[operand_group_bytes]\n"
        "vle32.v v16, (%[b0p])\n addi %[b0p], %[b0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtfmm.tvv mt8, v2, v26\n"
        "vtfmm.tvv mt8, v3, v27\n"
        "vtfmm.tvv mt12, v0, v28\n"
        "vtfmm.tvv mt12, v1, v29\n"
        "vtfmm.tvv mt12, v2, v30\n"
        "vtfmm.tvv mt12, v3, v31\n"
        : [b1p] "+&r"(b1p),
          [b2p] "+&r"(b2p),
          [b3p] "+&r"(b3p),
          [b0p] "+&r"(b0p)
        : [group_vl] "r"(group_vl),
          [operand_group_bytes] "i"(operand_group_bytes),
          [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl)
        : "memory", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "v16", "v17", "v18", "v19");
      // v0-v3 can be replaced only after all four tiles consume them.
      asm volatile(
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vle32.v v0, (%[a0p])\n addi %[a0p], %[a0p], %[operand_group_bytes]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        : [a0p] "+&r"(a0p)
        : [group_vl] "r"(group_vl),
          [operand_group_bytes] "i"(operand_group_bytes),
          [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl)
        : "memory", "v0", "v1", "v2", "v3");
      }

      // Final K group: move completed tiles while the next tile computes.
      asm volatile(
        "vtfmm.tvv mt0, v0, v16\n"
        "vle32.v v20, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vle32.v v21, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vle32.v v22, (%[b1p])\n addi %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vle32.v v23, (%[b1p])\n"
        "vtfmm.tvv mt4, v0, v20\n"
        "vle32.v v24, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtmv.v.t v4, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v1, v21\n"
        "vle32.v v25, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtmv.v.t v8, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v2, v22\n"
        "vle32.v v26, (%[b2p])\n addi %[b2p], %[b2p], %[operand_bytes]\n"
        "vtmv.v.t v12, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt4, v3, v23\n"
        "vle32.v v27, (%[b2p])\n"
        "vtmv.v.t v16, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtfmm.tvv mt8, v0, v24\n"
        "vle32.v v28, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtmv.v.t v5, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v1, v25\n"
        "vle32.v v29, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtmv.v.t v9, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v2, v26\n"
        "vle32.v v30, (%[b3p])\n addi %[b3p], %[b3p], %[operand_bytes]\n"
        "vtmv.v.t v13, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt8, v3, v27\n"
        "vle32.v v31, (%[b3p])\n"
        "vtmv.v.t v17, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtfmm.tvv mt12, v0, v28\n"
        "vtmv.v.t v6, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v1, v29\n"
        "vtmv.v.t v10, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v2, v30\n"
        "vtmv.v.t v14, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtfmm.tvv mt12, v3, v31\n"
        "vtmv.v.t v18, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v7, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v11, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        // Rows 0-1 are complete in v4-v7/v8-v11; store them before moving rows 2-3.
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vse32.v v4, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "vse32.v v8, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtmv.v.t v15, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v19, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vse32.v v12, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "vse32.v v16, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        : [b1p] "+&r"(b1p),
          [b2p] "+&r"(b2p),
          [tss0] "+&r"(tss0),
          [b3p] "+&r"(b3p),
          [tss4] "+&r"(tss4),
          [tss8] "+&r"(tss8),
          [tss12] "+&r"(tss12),
          [ptr] "+&r"(ptr)
        : [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl),
          [operand_bytes] "i"(operand_bytes),
          [group_vl] "r"(group_vl),
          [c_stride] "i"(c_stride)
        : "memory", "v20", "v21", "v22", "v23", "v24", "v4", "v25", "v8", "v26", "v12", "v27", "v16", "v28", "v5", "v29", "v9", "v30", "v13", "v31", "v17", "v6", "v10", "v14", "v18", "v7", "v11", "v15", "v19");

      // Rows 4-15: four contiguous 64-element rows per batch.
      do {
      asm volatile(
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtmv.v.t v4, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v5, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v6, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v7, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v8, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v9, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v10, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v11, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vse32.v v4, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "vse32.v v8, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "msetmtype %[mtype], %[vtype]\n" "msettn x0, %[tile_vl]\n"
        "vtmv.v.t v12, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v13, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v14, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v15, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vtmv.v.t v16, %[tss0]\n addi %[tss0], %[tss0], 1\n"
        "vtmv.v.t v17, %[tss4]\n addi %[tss4], %[tss4], 1\n"
        "vtmv.v.t v18, %[tss8]\n addi %[tss8], %[tss8], 1\n"
        "vtmv.v.t v19, %[tss12]\n addi %[tss12], %[tss12], 1\n"
        "vsetvli x0, %[group_vl], e32, m4, ta, ma\n"
        "vse32.v v12, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        "vse32.v v16, (%[ptr])\n addi %[ptr], %[ptr], %[c_stride]\n"
        : [tss0] "+&r"(tss0),
          [tss4] "+&r"(tss4),
          [tss8] "+&r"(tss8),
          [tss12] "+&r"(tss12),
          [ptr] "+&r"(ptr)
        : [mtype] "r"(mtype),
          [vtype] "r"(vtype),
          [tile_vl] "r"(tile_vl),
          [group_vl] "r"(group_vl),
          [c_stride] "i"(c_stride)
        : "memory", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19");
      } while (tss0 < TE);
    }
  }
}

__attribute__((noinline, aligned(64))) void matmul_fp32(
    const float *At, const float *B, float *C)
{
  const uintptr_t c_stride = 64 * sizeof(float);
  const uintptr_t mtype = (TE << 10) | (2u << 5) | 1u;
  const uintptr_t vtype = 0xD0; // e32, m1: one vector per tile row
  const uintptr_t tile_vl = TE;
  const uintptr_t group_vl = 4 * TE;
  const uintptr_t operand_group_bytes = 4 * TE * sizeof(float);

  // Process each 16-row output block independently; no state crosses blocks.
  for (uint32_t nb = 0; nb < 4; nb += 1) {
    const uint32_t mb0 = nb * TE;
    {
      // Initialize and compute one complete 16-row output block.
      uintptr_t a0p = (uintptr_t)(At + mb0 * K_CHUNK);
      uintptr_t b0p = (uintptr_t)B;
      uintptr_t b1p = (uintptr_t)(B + TE * K_CHUNK);
      uintptr_t b2p = (uintptr_t)(B + 2 * TE * K_CHUNK);
      uintptr_t b3p = (uintptr_t)(B + 3 * TE * K_CHUNK);
      uintptr_t ptr = (uintptr_t)(C + mb0 * 64);
      uintptr_t tss0 = 0x00000000u;
      uintptr_t tss4 = 0x20000000u;
      uintptr_t tss8 = 0x40000000u;
      uintptr_t tss12 = 0x60000000u;

      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v0, (%0)" : : "r"(a0p) : "v0", "v1", "v2", "v3", "memory"); a0p += operand_group_bytes;
      asm volatile("vle32.v v16, (%0)" : : "r"(b0p) : "v16", "v17", "v18", "v19", "memory"); b0p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtzero mt0" : : : "memory");
      asm volatile("vtzero mt4" : : : "memory");
      asm volatile("vtzero mt8" : : : "memory");
      asm volatile("vtzero mt12" : : : "memory");

      for (uint32_t kg = 0; kg + 1 < K_CHUNK / 4; ++kg) {
      asm volatile("vtfmm.tvv mt0, v0, v16" : : : "memory");
      asm volatile("vtfmm.tvv mt0, v1, v17" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v20, (%0)" : : "r"(b1p) : "v20", "v21", "v22", "v23", "memory"); b1p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt0, v2, v18" : : : "memory");
      asm volatile("vtfmm.tvv mt0, v3, v19" : : : "memory");
      asm volatile("vtfmm.tvv mt4, v0, v20" : : : "memory");
      asm volatile("vtfmm.tvv mt4, v1, v21" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v24, (%0)" : : "r"(b2p) : "v24", "v25", "v26", "v27", "memory"); b2p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt4, v2, v22" : : : "memory");
      asm volatile("vtfmm.tvv mt4, v3, v23" : : : "memory");
      asm volatile("vtfmm.tvv mt8, v0, v24" : : : "memory");
      asm volatile("vtfmm.tvv mt8, v1, v25" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v28, (%0)" : : "r"(b3p) : "v28", "v29", "v30", "v31", "memory"); b3p += operand_group_bytes;
      asm volatile("vle32.v v16, (%0)" : : "r"(b0p) : "v16", "v17", "v18", "v19", "memory"); b0p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt8, v2, v26" : : : "memory");
      asm volatile("vtfmm.tvv mt8, v3, v27" : : : "memory");
      asm volatile("vtfmm.tvv mt12, v0, v28" : : : "memory");
      asm volatile("vtfmm.tvv mt12, v1, v29" : : : "memory");
      asm volatile("vtfmm.tvv mt12, v2, v30" : : : "memory");
      asm volatile("vtfmm.tvv mt12, v3, v31" : : : "memory");
      // v0-v3 can be replaced only after all four tiles consume them.
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v0, (%0)" : : "r"(a0p) : "v0", "v1", "v2", "v3", "memory"); a0p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      }

      // Final K group: move each completed tile row to the vector registers.
      asm volatile("vtfmm.tvv mt0, v0, v16" : : : "memory");
      asm volatile("vtfmm.tvv mt0, v1, v17" : : : "memory");
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vle32.v v20, (%0)" : : "r"(b1p) : "v20", "v21", "v22", "v23", "memory"); b1p += operand_group_bytes;
      asm volatile("vle32.v v24, (%0)" : : "r"(b2p) : "v24", "v25", "v26", "v27", "memory"); b2p += operand_group_bytes;
      asm volatile("vle32.v v28, (%0)" : : "r"(b3p) : "v28", "v29", "v30", "v31", "memory"); b3p += operand_group_bytes;
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtfmm.tvv mt0, v2, v18" : : : "memory");
      asm volatile("vtfmm.tvv mt0, v3, v19" : : : "memory");
      asm volatile("vtfmm.tvv mt4, v0, v20" : : : "memory");
      asm volatile("vtmv.v.t v4, %0" : : "r"(tss0) : "v4", "memory"); ++tss0;
      asm volatile("vtfmm.tvv mt4, v1, v21" : : : "memory");
      asm volatile("vtmv.v.t v8, %0" : : "r"(tss0) : "v8", "memory"); ++tss0;
      asm volatile("vtfmm.tvv mt4, v2, v22" : : : "memory");
      asm volatile("vtmv.v.t v12, %0" : : "r"(tss0) : "v12", "memory"); ++tss0;
      asm volatile("vtfmm.tvv mt4, v3, v23" : : : "memory");
      asm volatile("vtmv.v.t v16, %0" : : "r"(tss0) : "v16", "memory"); ++tss0;
      asm volatile("vtfmm.tvv mt8, v0, v24" : : : "memory");
      asm volatile("vtmv.v.t v5, %0" : : "r"(tss4) : "v5", "memory"); ++tss4;
      asm volatile("vtfmm.tvv mt8, v1, v25" : : : "memory");
      asm volatile("vtmv.v.t v9, %0" : : "r"(tss4) : "v9", "memory"); ++tss4;
      asm volatile("vtfmm.tvv mt8, v2, v26" : : : "memory");
      asm volatile("vtmv.v.t v13, %0" : : "r"(tss4) : "v13", "memory"); ++tss4;
      asm volatile("vtfmm.tvv mt8, v3, v27" : : : "memory");
      asm volatile("vtmv.v.t v17, %0" : : "r"(tss4) : "v17", "memory"); ++tss4;
      asm volatile("vtfmm.tvv mt12, v0, v28" : : : "memory");
      asm volatile("vtmv.v.t v6, %0" : : "r"(tss8) : "v6", "memory"); ++tss8;
      asm volatile("vtfmm.tvv mt12, v1, v29" : : : "memory");
      asm volatile("vtmv.v.t v10, %0" : : "r"(tss8) : "v10", "memory"); ++tss8;
      asm volatile("vtfmm.tvv mt12, v2, v30" : : : "memory");
      asm volatile("vtmv.v.t v14, %0" : : "r"(tss8) : "v14", "memory"); ++tss8;
      asm volatile("vtfmm.tvv mt12, v3, v31" : : : "memory");
      asm volatile("vtmv.v.t v18, %0" : : "r"(tss8) : "v18", "memory"); ++tss8;
      asm volatile("vtmv.v.t v7, %0" : : "r"(tss12) : "v7", "memory"); ++tss12;
      asm volatile("vtmv.v.t v11, %0" : : "r"(tss12) : "v11", "memory"); ++tss12;
      asm volatile("vtmv.v.t v15, %0" : : "r"(tss12) : "v15", "memory"); ++tss12;
      asm volatile("vtmv.v.t v19, %0" : : "r"(tss12) : "v19", "memory"); ++tss12;
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vse32.v v4, (%0)" : : "r"(ptr) : "memory"); ptr += c_stride;
      asm volatile("vse32.v v8, (%0)" : : "r"(ptr) : "memory"); ptr += c_stride;
      asm volatile("vse32.v v12, (%0)" : : "r"(ptr) : "memory"); ptr += c_stride;
      asm volatile("vse32.v v16, (%0)" : : "r"(ptr) : "memory"); ptr += c_stride;

      // Rows 4-15: four contiguous 64-element rows per batch.
      // Keep the store epilogue compact for the instruction cache.
      for (uint32_t row = 4; row < TE; row += 4) {
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtmv.v.t v4, %0" : : "r"(tss0) : "v4", "memory"); ++tss0;
      asm volatile("vtmv.v.t v5, %0" : : "r"(tss4) : "v5", "memory"); ++tss4;
      asm volatile("vtmv.v.t v6, %0" : : "r"(tss8) : "v6", "memory"); ++tss8;
      asm volatile("vtmv.v.t v7, %0" : : "r"(tss12) : "v7", "memory"); ++tss12;
      asm volatile("vtmv.v.t v8, %0" : : "r"(tss0) : "v8", "memory"); ++tss0;
      asm volatile("vtmv.v.t v9, %0" : : "r"(tss4) : "v9", "memory"); ++tss4;
      asm volatile("vtmv.v.t v10, %0" : : "r"(tss8) : "v10", "memory"); ++tss8;
      asm volatile("vtmv.v.t v11, %0" : : "r"(tss12) : "v11", "memory"); ++tss12;
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vse32.v v4, (%0)\n" "addi %0, %0, %1\n"
                   "vse32.v v8, (%0)\n" "addi %0, %0, %1\n"
                   : "+r"(ptr) : "i"(c_stride) : "memory");
      asm volatile("msetmtype %0, %1\n" "msettn x0, %2" : : "r"(mtype), "r"(vtype), "r"(tile_vl) : "memory");
      asm volatile("vtmv.v.t v12, %0" : : "r"(tss0) : "v12", "memory"); ++tss0;
      asm volatile("vtmv.v.t v13, %0" : : "r"(tss4) : "v13", "memory"); ++tss4;
      asm volatile("vtmv.v.t v14, %0" : : "r"(tss8) : "v14", "memory"); ++tss8;
      asm volatile("vtmv.v.t v15, %0" : : "r"(tss12) : "v15", "memory"); ++tss12;
      asm volatile("vtmv.v.t v16, %0" : : "r"(tss0) : "v16", "memory"); ++tss0;
      asm volatile("vtmv.v.t v17, %0" : : "r"(tss4) : "v17", "memory"); ++tss4;
      asm volatile("vtmv.v.t v18, %0" : : "r"(tss8) : "v18", "memory"); ++tss8;
      asm volatile("vtmv.v.t v19, %0" : : "r"(tss12) : "v19", "memory"); ++tss12;
      asm volatile("vsetvli x0, %0, e32, m4, ta, ma" : : "r"(group_vl) : "memory");
      asm volatile("vse32.v v12, (%0)\n" "addi %0, %0, %1\n"
                   "vse32.v v16, (%0)\n" "addi %0, %0, %1\n"
                   : "+r"(ptr) : "i"(c_stride) : "memory");
      }
    }
  }
}
