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

// FP16-to-FP32 alpha-beta GEMM kernel for TE=16, CE=8, and four accumulator tiles.

#include "gemm.h"

__attribute__((noinline, aligned(64))) void gemm_fp16_fp32_beta_vle(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    const float *alpha, const float *beta)
{
  register float alpha_ft0 asm("ft0");
  register float beta_ft1 asm("ft1");
  asm volatile("flw %0, 0(%2)\nflw %1, 0(%3)"
               : "=f"(alpha_ft0), "=f"(beta_ft1)
               : "r"(alpha), "r"(beta)
               : "memory");

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
      asm volatile("vfmul.vf v0, v0, %0" : : "f"(beta_ft1) : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
      asm volatile("vfmacc.vf v0, %0, v8" : : "f"(alpha_ft0) : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
      asm volatile("vfmul.vf v24, v24, %0" : : "f"(beta_ft1) : "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
      asm volatile("vfmacc.vf v24, %0, v16" : : "f"(alpha_ft0) : "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
      asm volatile("vse32.v v0, (%0)\n" "addi %0, %0, %1" : "+r"(ptr) : "i"(c_stride) : "memory");
      asm volatile("vse32.v v24, (%0)\n" "addi %0, %0, %1" : "+r"(ptr) : "i"(c_stride) : "memory");

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
      asm volatile("vfmul.vf v0, v0, %0" : : "f"(beta_ft1) : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
      asm volatile("vfmacc.vf v0, %0, v8" : : "f"(alpha_ft0) : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
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
      asm volatile("vfmul.vf v24, v24, %0" : : "f"(beta_ft1) : "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
      asm volatile("vfmacc.vf v24, %0, v16" : : "f"(alpha_ft0) : "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
      asm volatile("vse32.v v0, (%0)\n" "addi %0, %0, %1" : "+r"(ptr) : "i"(c_stride) : "memory");
      asm volatile("vse32.v v24, (%0)\n" "addi %0, %0, %1" : "+r"(ptr) : "i"(c_stride) : "memory");
      }
    }
  }
}

