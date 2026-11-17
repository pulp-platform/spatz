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


// Four packed K pairs per operand group; each VTFMM consumes two FP16 K values.
#define GEMM_CONFIG_ASM \
    "msetmtype %[mt], %[vt]\nmsettn zero, %[tn]\n"
#define GEMM_MAC4_ASM(tile, b0, b1, b2, b3) \
    "vtfmm.tvv " tile ", v0, " b0 "\n" \
    "vtfmm.tvv " tile ", v1, " b1 "\n" \
    "vtfmm.tvv " tile ", v2, " b2 "\n" \
    "vtfmm.tvv " tile ", v3, " b3 "\n"

#define GEMM_1X4_MAC4_ASM(tile, b) \
    "vtfmm.tvv " tile ", " GS_A(0) ", " b(0) "\n" \
    "vtfmm.tvv " tile ", " GS_A(1) ", " b(1) "\n" \
    "vtfmm.tvv " tile ", " GS_A(2) ", " b(2) "\n" \
    "vtfmm.tvv " tile ", " GS_A(3) ", " b(3) "\n"

static inline __attribute__((always_inline)) void gemm_logits_body(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K, float *colmax)
{
  const uintptr_t c_stride = N * sizeof(float);
  const uintptr_t mtype = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t vtype = 0xC8; // e16, m1, tk=2 widening SDOTP
  const uintptr_t store_mtype = (TE << 10) | (1u << 5) | 1u;
  const uintptr_t store_vtype = 0xD0; // e32, m1: one vector per tile row
  const uintptr_t tile_vl = TE;
  const uintptr_t group_vl = 4 * TE;

  // Keep MAX live across all M blocks; the allocation header prevents
  // overlap with both operand and epilogue groups.
  if (colmax != 0) {
    asm volatile(
        "vsetvli zero, %[vl], e32, m4, ta, ma\n"
        "vfmv.v.f " GS_MAX(0) ", %[init]\n"
        : : [vl] "r"(group_vl), [init] "f"(-__builtin_inff())
        : GS_MAX_CLOBBERS, "memory");
  }

  // Compute each 16-vocab-row block with a 1x4 accumulator layout.
  // The complete [vocab][token] scratch feeds one global column softmax.
  for (uint32_t mb = 0; mb < M / TE; ++mb) {
    const uint32_t mb0 = mb * TE;
    {
      // Initialize and compute one complete 16-row output block.
      uintptr_t a0p = (uintptr_t)(Atpack + mb0 * K);
      uintptr_t b0p = (uintptr_t)Bpack;
      uintptr_t b1p = (uintptr_t)(Bpack + TE * K);
      uintptr_t b2p = (uintptr_t)(Bpack + 2 * TE * K);
      uintptr_t b3p = (uintptr_t)(Bpack + 3 * TE * K);
      uintptr_t ptr = (uintptr_t)(C + mb0 * N);
      uintptr_t tss0 = 0x00000000u;
      uintptr_t tss4 = 0x20000000u;
      uintptr_t tss8 = 0x40000000u;
      uintptr_t tss12 = 0x60000000u;

      asm volatile(
          "msetmtype %[mt], %[vt]\n"
          "msettn zero, %[tn]\n"
          "vtzero mt0\nvtzero mt4\nvtzero mt8\nvtzero mt12\n"
          "mv t0, %[groups]\n"
          "1:\n"
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v " GS_A(0) ", (%[a])\naddi %[a], %[a], 256\n"
          "vle32.v " GS_B0(0) ", (%[b0])\naddi %[b0], %[b0], 256\n"
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v " GS_B1(0) ", (%[b1])\naddi %[b1], %[b1], 256\n"
          GEMM_CONFIG_ASM
          GEMM_1X4_MAC4_ASM("mt0", GS_B0)
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v " GS_B0(0) ", (%[b2])\naddi %[b2], %[b2], 256\n"
          GEMM_CONFIG_ASM
          GEMM_1X4_MAC4_ASM("mt4", GS_B1)
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v " GS_B1(0) ", (%[b3])\naddi %[b3], %[b3], 256\n"
          GEMM_CONFIG_ASM
          GEMM_1X4_MAC4_ASM("mt8", GS_B0)
          GEMM_CONFIG_ASM
          GEMM_1X4_MAC4_ASM("mt12", GS_B1)
          "addi t0, t0, -1\nbnez t0, 1b\n"
          : [a] "+&r"(a0p), [b0] "+&r"(b0p), [b1] "+&r"(b1p),
            [b2] "+&r"(b2p), [b3] "+&r"(b3p)
          : [mt] "r"(mtype), [vt] "r"(vtype), [tn] "r"(tile_vl),
            [vl] "r"(group_vl), [groups] "r"(K / 8)
          : "t0", GS_A_CLOBBERS, GS_B0_CLOBBERS,
            GS_B1_CLOBBERS, "memory");

      /* Drain four complete feature-major rows per matrix/vector config. */
      for (uint32_t row = 0; row < TE; row += 4) {
        asm volatile(
            "msetmtype %4, %5\n"
            "msettn x0, %6\n"
            "vtmv.v.t " GS_ROW0(0) ", %0\naddi %0, %0, 1\n"
            "vtmv.v.t " GS_ROW1(0) ", %0\naddi %0, %0, 1\n"
            "vtmv.v.t " GS_ROW2(0) ", %0\naddi %0, %0, 1\n"
            "vtmv.v.t " GS_ROW3(0) ", %0\naddi %0, %0, 1\n"
            "vtmv.v.t " GS_ROW0(1) ", %1\naddi %1, %1, 1\n"
            "vtmv.v.t " GS_ROW1(1) ", %1\naddi %1, %1, 1\n"
            "vtmv.v.t " GS_ROW2(1) ", %1\naddi %1, %1, 1\n"
            "vtmv.v.t " GS_ROW3(1) ", %1\naddi %1, %1, 1\n"
            "vtmv.v.t " GS_ROW0(2) ", %2\naddi %2, %2, 1\n"
            "vtmv.v.t " GS_ROW1(2) ", %2\naddi %2, %2, 1\n"
            "vtmv.v.t " GS_ROW2(2) ", %2\naddi %2, %2, 1\n"
            "vtmv.v.t " GS_ROW3(2) ", %2\naddi %2, %2, 1\n"
            "vtmv.v.t " GS_ROW0(3) ", %3\naddi %3, %3, 1\n"
            "vtmv.v.t " GS_ROW1(3) ", %3\naddi %3, %3, 1\n"
            "vtmv.v.t " GS_ROW2(3) ", %3\naddi %3, %3, 1\n"
            "vtmv.v.t " GS_ROW3(3) ", %3\naddi %3, %3, 1\n"
            : "+&r"(tss0), "+&r"(tss4), "+&r"(tss8), "+&r"(tss12)
            : "r"(store_mtype), "r"(store_vtype), "r"(tile_vl)
            : GS_ROW0_CLOBBERS, GS_ROW1_CLOBBERS,
              GS_ROW2_CLOBBERS, GS_ROW3_CLOBBERS,
              "memory");
        asm volatile(
            "vsetvli x0, %[vl], e32, m4, ta, ma\n"
            "vfadd.vf " GS_ROW0(0) ", " GS_ROW0(0) ", %[b0]\n"
            "vfadd.vf " GS_ROW1(0) ", " GS_ROW1(0) ", %[b1]\n"
            "vfadd.vf " GS_ROW2(0) ", " GS_ROW2(0) ", %[b2]\n"
            "vfadd.vf " GS_ROW3(0) ", " GS_ROW3(0) ", %[b3]\n"
            :
            : [vl] "r"(group_vl),
              [b0] "f"(C[(mb0 + row) * N]),
              [b1] "f"(C[(mb0 + row + 1) * N]),
              [b2] "f"(C[(mb0 + row + 2) * N]),
              [b3] "f"(C[(mb0 + row + 3) * N])
            : GS_ROW0_CLOBBERS, GS_ROW1_CLOBBERS,
              GS_ROW2_CLOBBERS, GS_ROW3_CLOBBERS,
              "memory");
        if (colmax != 0) {
          asm volatile(
              "vfmax.vv " GS_MAX(0) ", " GS_MAX(0) ", " GS_ROW0(0) "\n"
              "vse32.v " GS_ROW0(0) ", (%[ptr])\nadd %[ptr], %[ptr], %[stride]\n"
              "vfmax.vv " GS_MAX(0) ", " GS_MAX(0) ", " GS_ROW1(0) "\n"
              "vse32.v " GS_ROW1(0) ", (%[ptr])\nadd %[ptr], %[ptr], %[stride]\n"
              "vfmax.vv " GS_MAX(0) ", " GS_MAX(0) ", " GS_ROW2(0) "\n"
              "vse32.v " GS_ROW2(0) ", (%[ptr])\nadd %[ptr], %[ptr], %[stride]\n"
              "vfmax.vv " GS_MAX(0) ", " GS_MAX(0) ", " GS_ROW3(0) "\n"
              "vse32.v " GS_ROW3(0) ", (%[ptr])\nadd %[ptr], %[ptr], %[stride]\n"
              : [ptr] "+&r"(ptr)
              : [stride] "r"(c_stride)
              : GS_MAX_CLOBBERS, "memory");
        } else {
          asm volatile(
              "vse32.v " GS_ROW0(0) ", (%[ptr])\nadd %[ptr], %[ptr], %[stride]\n"
              "vse32.v " GS_ROW1(0) ", (%[ptr])\nadd %[ptr], %[ptr], %[stride]\n"
              "vse32.v " GS_ROW2(0) ", (%[ptr])\nadd %[ptr], %[ptr], %[stride]\n"
              "vse32.v " GS_ROW3(0) ", (%[ptr])\nadd %[ptr], %[ptr], %[stride]\n"
              : [ptr] "+&r"(ptr)
              : [stride] "r"(c_stride) : "memory");
        }
      }
    }
  }
  if (colmax != 0)
    asm volatile("vse32.v " GS_MAX(0) ", (%0)" : : "r"(colmax) : "memory");
}

/*
 * Fused-only 2x2 schedule. A 32x32 output block reuses two A and two B
 * operands, so each K group needs four vector loads instead of five. Bias
 * addition and the running column maximum remain between VTMV and the store.
 */
static inline __attribute__((always_inline)) void gemm_logits_body_2x2(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K, float *colmax)
{
  const uintptr_t mtype = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t vtype = 0xC8;
  const uintptr_t store_mtype = (TE << 10) | (1u << 5) | 1u;
  const uintptr_t store_vtype = 0xD0;
  const uintptr_t tile_vl = TE;
  const uintptr_t operand_vl = 4 * TE;
  const uintptr_t row_vl = 2 * TE;

  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vfmv.v.f v28, %[init]\n"
      : : [vl] "r"(operand_vl), [init] "f"(-__builtin_inff())
      : "v28", "v29", "v30", "v31", "memory");

  for (uint32_t mb0 = 0; mb0 < M; mb0 += 2 * TE) {
    for (uint32_t nb0 = 0; nb0 < N; nb0 += 2 * TE) {
      uintptr_t a0p = (uintptr_t)(Atpack + mb0 * K);
      uintptr_t a1p = (uintptr_t)(Atpack + (mb0 + TE) * K);
      uintptr_t b0p = (uintptr_t)(Bpack + nb0 * K);
      uintptr_t b1p = (uintptr_t)(Bpack + (nb0 + TE) * K);
      uintptr_t tss0 = 0x00000000u;
      uintptr_t tss4 = 0x20000000u;
      uintptr_t tss8 = 0x40000000u;
      uintptr_t tss12 = 0x60000000u;

      asm volatile(
          "msetmtype %[mt], %[vt]\n"
          "msettn zero, %[tn]\n"
          "vtzero mt0\nvtzero mt4\nvtzero mt8\nvtzero mt12\n"
          "mv t0, %[groups]\n"
          "1:\n"
          "vsetvli zero, %[vl], e32, m4, ta, ma\n"
          "vle32.v v0, (%[a0])\naddi %[a0], %[a0], 256\n"
          "vle32.v v16, (%[b0])\naddi %[b0], %[b0], 256\n"
          "vle32.v v8, (%[a1])\naddi %[a1], %[a1], 256\n"
          "vle32.v v24, (%[b1])\naddi %[b1], %[b1], 256\n"
          GEMM_CONFIG_ASM
          GEMM_MAC4_ASM("mt0", "v16", "v17", "v18", "v19")
          "vtfmm.tvv mt4, v0, v24\n"
          "vtfmm.tvv mt4, v1, v25\n"
          "vtfmm.tvv mt4, v2, v26\n"
          "vtfmm.tvv mt4, v3, v27\n"
          "vtfmm.tvv mt8, v8, v16\n"
          "vtfmm.tvv mt8, v9, v17\n"
          "vtfmm.tvv mt8, v10, v18\n"
          "vtfmm.tvv mt8, v11, v19\n"
          "vtfmm.tvv mt12, v8, v24\n"
          "vtfmm.tvv mt12, v9, v25\n"
          "vtfmm.tvv mt12, v10, v26\n"
          "vtfmm.tvv mt12, v11, v27\n"
          "addi t0, t0, -1\nbnez t0, 1b\n"
          : [a0] "+&r"(a0p), [a1] "+&r"(a1p),
            [b0] "+&r"(b0p), [b1] "+&r"(b1p)
          : [mt] "r"(mtype), [vt] "r"(vtype), [tn] "r"(tile_vl),
            [vl] "r"(operand_vl), [groups] "r"(K / 8)
          : "t0", "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11",
            "v16", "v17", "v18", "v19", "v24", "v25", "v26", "v27",
            "memory");

      asm volatile(".rept 16\n nop\n .endr" : : : "memory");

      for (uint32_t row = 0; row < TE; ++row) {
        asm volatile(
            "msetmtype %[mt], %[vt]\n"
            "msettn zero, %[tn]\n"
            "vtmv.v.t v8, %[t0]\naddi %[t0], %[t0], 1\n"
            "vtmv.v.t v9, %[t4]\naddi %[t4], %[t4], 1\n"
            "vsetvli zero, %[vl], e32, m2, ta, ma\n"
            "vfadd.vf v8, v8, %[bias]\n"
            "vse32.v v8, (%[out])\n"
            : [t0] "+&r"(tss0), [t4] "+&r"(tss4)
            : [mt] "r"(store_mtype), [vt] "r"(store_vtype),
              [tn] "r"(tile_vl), [vl] "r"(row_vl),
              [bias] "f"(C[(mb0 + row) * N + nb0]),
              [out] "r"(C + (mb0 + row) * N + nb0)
            : "v8", "v9", "memory");
        if (nb0 == 0)
          asm volatile("vfmax.vv v28, v28, v8"
                       : : : "v28", "v29", "memory");
        else
          asm volatile("vfmax.vv v30, v30, v8"
                       : : : "v30", "v31", "memory");

        asm volatile(
            "msetmtype %[mt], %[vt]\n"
            "msettn zero, %[tn]\n"
            "vtmv.v.t v8, %[t8]\naddi %[t8], %[t8], 1\n"
            "vtmv.v.t v9, %[t12]\naddi %[t12], %[t12], 1\n"
            "vsetvli zero, %[vl], e32, m2, ta, ma\n"
            "vfadd.vf v8, v8, %[bias]\n"
            "vse32.v v8, (%[out])\n"
            : [t8] "+&r"(tss8), [t12] "+&r"(tss12)
            : [mt] "r"(store_mtype), [vt] "r"(store_vtype),
              [tn] "r"(tile_vl), [vl] "r"(row_vl),
              [bias] "f"(C[(mb0 + TE + row) * N + nb0]),
              [out] "r"(C + (mb0 + TE + row) * N + nb0)
            : "v8", "v9", "memory");
        if (nb0 == 0)
          asm volatile("vfmax.vv v28, v28, v8"
                       : : : "v28", "v29", "memory");
        else
          asm volatile("vfmax.vv v30, v30, v8"
                       : : : "v30", "v31", "memory");
      }
    }
  }

  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vse32.v v28, (%[out])\n"
      : : [vl] "r"(operand_vl), [out] "r"(colmax)
      : "memory");
}

__attribute__((noinline, aligned(64))) void gemm_softmax_fused(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K) {
  float *const colmax = (float *)(uintptr_t)Atpack;
  gemm_logits_body(Atpack, Bpack, C, M, N, K, colmax);
#if defined(GEMM_SOFTMAX_OPOPE_ORDER)
  softmax_cols_premax_optransformer_order(C, M, colmax);
#else
  softmax_cols_rows_premax(C, M, colmax);
#endif
}

__attribute__((noinline, aligned(64))) void gemm_logits_non_fused(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K) {
  gemm_logits_body(Atpack, Bpack, C, M, N, K, 0);
}

__attribute__((noinline, aligned(64))) void softmax_non_fused(
    float *C, uint32_t M) {
  softmax_cols_rows(C, M);
}
