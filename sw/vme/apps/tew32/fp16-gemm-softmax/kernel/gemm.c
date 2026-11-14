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

// fp16->fp32 optimal kernel for TE=16, CE=8, and four accumulator tiles.

#include "gemm.h"
#include "softmax.h"

// Four packed K pairs per operand group; each VTFMM consumes two FP16 K values.
#define GEMM_CONFIG_ASM \
    "msetmtype %[mt], %[vt]\nmsettn zero, %[tn]\n"
#define GEMM_MAC4_ASM(tile, b0, b1, b2, b3) \
    "vtfmm.tvv " tile ", v0, " b0 "\n" \
    "vtfmm.tvv " tile ", v1, " b1 "\n" \
    "vtfmm.tvv " tile ", v2, " b2 "\n" \
    "vtfmm.tvv " tile ", v3, " b3 "\n"

__attribute__((noinline, aligned(64))) void gemm_softmax_fp16_fp32(
    const __fp16 *Atpack, const __fp16 *Bpack, const float *bias, float *C)
{
  const uintptr_t c_stride = 64 * sizeof(float);
  const uintptr_t mtype = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t vtype = 0xC8; // e16, m1, tk=2 widening SDOTP
  const uintptr_t store_mtype = (TE << 10) | (1u << 5) | 1u;
  const uintptr_t store_vtype = 0xD0; // e32, m1: one vector per tile row
  const uintptr_t tile_vl = TE;
  const uintptr_t group_vl = 4 * TE;

  // Compute each 16-vocab-row block with a 1x4 accumulator layout.
  // The complete [vocab][token] scratch feeds one global column softmax.
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
      uintptr_t tss0 = 0x00000000u;
      uintptr_t tss4 = 0x20000000u;
      uintptr_t tss8 = 0x40000000u;
      uintptr_t tss12 = 0x60000000u;

      asm volatile(
          "msetmtype %[mt], %[vt]\n"
          "msettn zero, %[tn]\n"
          "vtzero mt0\nvtzero mt4\nvtzero mt8\nvtzero mt12\n"
          "li t0, %[groups]\n"
          "1:\n"
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v v0, (%[a])\naddi %[a], %[a], 256\n"
          "vle32.v v16, (%[b0])\naddi %[b0], %[b0], 256\n"
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v v24, (%[b1])\naddi %[b1], %[b1], 256\n"
          GEMM_CONFIG_ASM
          GEMM_MAC4_ASM("mt0", "v16", "v17", "v18", "v19")
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v v16, (%[b2])\naddi %[b2], %[b2], 256\n"
          GEMM_CONFIG_ASM
          GEMM_MAC4_ASM("mt4", "v24", "v25", "v26", "v27")
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v v24, (%[b3])\naddi %[b3], %[b3], 256\n"
          GEMM_CONFIG_ASM
          GEMM_MAC4_ASM("mt8", "v16", "v17", "v18", "v19")
          GEMM_CONFIG_ASM
          GEMM_MAC4_ASM("mt12", "v24", "v25", "v26", "v27")
          "addi t0, t0, -1\nbnez t0, 1b\n"
          : [a] "+&r"(a0p), [b0] "+&r"(b0p), [b1] "+&r"(b1p),
            [b2] "+&r"(b2p), [b3] "+&r"(b3p)
          : [mt] "r"(mtype), [vt] "r"(vtype), [tn] "r"(tile_vl),
            [vl] "r"(group_vl), [groups] "i"(K_CHUNK / 8)
          : "t0", "v0", "v1", "v2", "v3", "v16", "v17", "v18", "v19",
            "v24", "v25", "v26", "v27", "memory");

      /* Drain two complete feature-major rows at a time. */
      for (uint32_t row = 0; row < TE; row += 2) {
        {
          asm volatile(
              "msetmtype %4, %5\n"
              "msettn x0, %6\n"
              "vtmv.v.t v8, %0\naddi %0, %0, 1\n"
              "vtmv.v.t v12, %0\naddi %0, %0, 1\n"
              "vtmv.v.t v9, %1\naddi %1, %1, 1\n"
              "vtmv.v.t v13, %1\naddi %1, %1, 1\n"
              "vtmv.v.t v10, %2\naddi %2, %2, 1\n"
              "vtmv.v.t v14, %2\naddi %2, %2, 1\n"
              "vtmv.v.t v11, %3\naddi %3, %3, 1\n"
              "vtmv.v.t v15, %3\naddi %3, %3, 1\n"
              : "+&r"(tss0), "+&r"(tss4), "+&r"(tss8), "+&r"(tss12)
              : "r"(store_mtype), "r"(store_vtype), "r"(tile_vl)
              : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory");
        }
        // Feature-major logits: each lane is a token, each row a vocab entry.
        // This is scratch storage; normalization spans ALL four 16-row blocks.
        asm volatile(
            "vsetvli x0, %[vl], e32, m4, ta, ma\n"
            "vfadd.vf v8, v8, %[b0]\n"
            "vfadd.vf v12, v12, %[b1]\n"
            "vse32.v v8, (%[ptr])\n"
            "addi %[ptr], %[ptr], %[stride]\n"
            "vse32.v v12, (%[ptr])\n"
            "addi %[ptr], %[ptr], %[stride]\n"
            : [ptr] "+&r"(ptr)
            : [vl] "r"(group_vl), [stride] "i"(c_stride),
              [b0] "f"(bias[mb0 + row]), [b1] "f"(bias[mb0 + row + 1])
            : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory");
      }
    }
  }
#ifdef GEMM_SOFTMAX_DIAGNOSTIC
  for (unsigned i = 0, count = 0; i < 4096; ++i) {
    float d = C[i] - Logits_dram[i];
    if ((d > 0.001f || d < -0.001f) && count++ < 8) {
      union { float f; unsigned u; } a = {C[i]}, b = {Logits_dram[i]};
      printf("Logits [%u,%u] got=%08x ref=%08x\n", i/64, i%64, a.u, b.u);
    }
  }
#endif
  softmax_cols(C);
}
