// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

#include "ope_runtime.h"
#define FLASHATTN_IMPLEMENTATION
#include "kernel/flashattn.h"

enum {
  kTile = 16,
  kReductionPairs = FLASH_D / 2,
  kKeyStages = FLASH_T / kTile,
};

static const uintptr_t kPvMtype = (kTile << 10) | (2u << 5) | 1u;
static const uintptr_t kPvVtype = 0xd0u;
static const uintptr_t kFp16Mtype = (kTile << 10) | (2u << 5) | 2u;
static const uintptr_t kFp16Vtype = 0xc8u;

static inline __attribute__((always_inline)) void set_pv_mode(void) {
  // kPvMtype already encodes tm=16.  msetmtype restores that value after a
  // vector configuration invalidates mtype; only tn/vl must be restored.
  asm volatile("msetmtype %0, %1\nmsettn zero, %2"
               :: "r"(kPvMtype), "r"(kPvVtype), "r"((uintptr_t)kTile)
               : "memory");
}

static inline __attribute__((always_inline)) void set_vector_mode(void) {
  asm volatile("vsetivli zero, 16, e32, m1, ta, ma" ::: "memory");
}

static inline __attribute__((always_inline)) void set_fp16_matrix_mode(void) {
  // kFp16Mtype already encodes tm=16; msettm would write the same value.
  asm volatile("msetmtype %0, %1\nmsettn zero, %2"
               :: "r"(kFp16Mtype), "r"(kFp16Vtype), "r"((uintptr_t)kTile)
               : "memory");
}

// Four reduction pairs fit in the free high vector registers. LMUL-2 uses
// four aligned group loads and passes golden; the LMUL-4 probe below fails.
#if defined(FLASH_QK_M2) && FLASH_QK_M2
#define FLASH_QK_FOUR_IMPL(TILE, QPTR, KPTR, CONFIGURE_VECTOR)             \
  do {                                                                     \
    asm volatile("vsetvli zero, %4, e32, m2, ta, ma\n"                  \
                 "vle32.v v24, (%0)\n"                                    \
                 "vle32.v v26, (%1)\n"                                    \
                 "vle32.v v28, (%2)\n"                                    \
                 "vle32.v v30, (%3)"                                      \
                 :: "r"(QPTR), "r"(KPTR), "r"((QPTR) + 2 * kTile),        \
                    "r"((KPTR) + 2 * kTile), "r"((uintptr_t)(2 * kTile))   \
                 : "v24", "v25", "v26", "v27", "v28", "v29", "v30",  \
                   "v31", "memory");                                      \
    set_fp16_matrix_mode();                                                \
    asm volatile("vtfmm.tvv " TILE ", v24, v26\n"                         \
                 "vtfmm.tvv " TILE ", v25, v27\n"                         \
                 "vtfmm.tvv " TILE ", v28, v30\n"                         \
                 "vtfmm.tvv " TILE ", v29, v31" ::: "memory");           \
    QPTR += 4 * kTile;                                                     \
    KPTR += 4 * kTile;                                                     \
  } while (0)
#elif defined(FLASH_QK_M4) && FLASH_QK_M4
// Diagnostic only: this faster two-load schedule failed the output check.
#define FLASH_QK_FOUR_IMPL(TILE, QPTR, KPTR, CONFIGURE_VECTOR)             \
  do {                                                                     \
    asm volatile("vsetvli zero, %2, e32, m4, ta, ma\n"                  \
                 "vle32.v v24, (%0)\n"                                    \
                 "vle32.v v28, (%1)"                                      \
                 :: "r"(QPTR), "r"(KPTR), "r"((uintptr_t)(4 * kTile))     \
                 : "v24", "v25", "v26", "v27", "v28", "v29", "v30",  \
                   "v31", "memory");                                      \
    set_fp16_matrix_mode();                                                \
    asm volatile("vtfmm.tvv " TILE ", v24, v28\n"                         \
                 "vtfmm.tvv " TILE ", v25, v29\n"                         \
                 "vtfmm.tvv " TILE ", v26, v30\n"                         \
                 "vtfmm.tvv " TILE ", v27, v31" ::: "memory");           \
    QPTR += 4 * kTile;                                                     \
    KPTR += 4 * kTile;                                                     \
  } while (0)
#else
#define FLASH_QK_FOUR_IMPL(TILE, QPTR, KPTR, CONFIGURE_VECTOR)             \
  do {                                                                     \
    CONFIGURE_VECTOR;                                                       \
    asm volatile("vle32.v v21, (%0)\n"                                   \
                 "vle32.v v22, (%1)\n"                                   \
                 "vle32.v v23, (%2)\n"                                   \
                 "vle32.v v24, (%3)\n"                                   \
                 "vle32.v v25, (%4)\n"                                   \
                 "vle32.v v26, (%5)\n"                                   \
                 "vle32.v v27, (%6)\n"                                   \
                 "vle32.v v28, (%7)"                                      \
                 :: "r"(QPTR), "r"(KPTR), "r"((QPTR) + kTile),             \
                    "r"((KPTR) + kTile), "r"((QPTR) + 2 * kTile),          \
                    "r"((KPTR) + 2 * kTile), "r"((QPTR) + 3 * kTile),      \
                    "r"((KPTR) + 3 * kTile)                                \
                 : "v21", "v22", "v23", "v24", "v25", "v26", "v27", \
                   "v28", "memory");                                      \
    set_fp16_matrix_mode();                                                \
    asm volatile("vtfmm.tvv " TILE ", v21, v22\n"                         \
                 "vtfmm.tvv " TILE ", v23, v24\n"                         \
                 "vtfmm.tvv " TILE ", v25, v26\n"                         \
                 "vtfmm.tvv " TILE ", v27, v28" ::: "memory");           \
    QPTR += 4 * kTile;                                                     \
    KPTR += 4 * kTile;                                                     \
  } while (0)
#endif

#define FLASH_QK_FOUR(TILE, QPTR, KPTR) \
  FLASH_QK_FOUR_IMPL(TILE, QPTR, KPTR, set_vector_mode())
#define FLASH_QK_FOUR_VECTOR_READY(TILE, QPTR, KPTR) \
  FLASH_QK_FOUR_IMPL(TILE, QPTR, KPTR, ((void)0))

static inline __attribute__((always_inline)) void qk_tile_mt0(
    const uint32_t *q, const uint32_t *k) {
  set_fp16_matrix_mode();
  asm volatile("vtzero mt0" ::: "memory");
  for (unsigned p = 0; p < kReductionPairs; p += 4) {
    FLASH_QK_FOUR("mt0", q, k);
  }
}

// QK is accumulated in the standard S=Q*K^T layout. Moving one tile column
// produces one score per query row. The PV matrix configuration also sets
// e32/VL16 for vfmax; vsetivli here would clear mtype and invalidate later
// tile moves. Two later tile moves separate each move from its vfmax consumer.
static inline __attribute__((always_inline)) void drain_mt0_and_max(void) {
  set_pv_mode();
  asm volatile(
      "li t0, 0x01000000\n"
      "vmv.v.v v2, v0\n"  "vmv.v.v v4, v0\n" // copy max -> v2 & v4 (pairwise)
      "vtmv.v.t v5, t0\n" "addi t0, t0, 1\n" // mv MT0[col=0] -> v5
      "vtmv.v.t v6, t0\n" "addi t0, t0, 1\n" // mv MT0[col=1] -> v6
      "vtmv.v.t v7, t0\n" "addi t0, t0, 1\n" // mv MT0[col=2] -> v7
      "vfmax.vv v2, v2, v5\n"                // max of col=0
      "vtmv.v.t v8, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v6\n"                // max of col=1 
      "vtmv.v.t v9, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v7\n"
      "vtmv.v.t v10, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v8\n"
      "vtmv.v.t v11, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v9\n"
      "vtmv.v.t v12, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v10\n"
      "vtmv.v.t v13, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v11\n"
      "vtmv.v.t v14, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v12\n"
      "vtmv.v.t v15, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v13\n"
      "vtmv.v.t v16, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v14\n"
      "vtmv.v.t v17, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v15\n"
      "vtmv.v.t v18, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v16\n"
      "vtmv.v.t v19, t0\n" "addi t0, t0, 1\n" // mv MT0[col=14] -> v19
      "vfmax.vv v2, v2, v17\n"
      "vtmv.v.t v20, t0\n"                    // mv MT0[col=15] -> v20
      "vfmax.vv v4, v4, v18\n"
      "vfmax.vv v2, v2, v19\n"                // max of col=14
      "vfmax.vv v4, v4, v20\n"                // max of col=15
      "vfmax.vv v2, v2, v4"                   // update max -> v2  
      ::: "t0", "v4", "memory");
  asm volatile("vfsub.vv v3, v0, v2\n" SOFTMAX_EXP_ASM("v3", "v23")
               :: EXP_INPUTS : "v23", "memory");
}

static inline __attribute__((always_inline)) void drain_mt8_and_max(void) {
  set_pv_mode();
  asm volatile(
      "li t0, 0x41000000\n"
      "vmv.v.v v2, v0\n"  "vmv.v.v v4, v0\n" // copy max -> v2 & v4 (pairwise)
      "vtmv.v.t v5, t0\n" "addi t0, t0, 1\n" // mv MT8[col=0] -> v5
      "vtmv.v.t v6, t0\n" "addi t0, t0, 1\n" // mv MT8[col=1] -> v6
      "vtmv.v.t v7, t0\n" "addi t0, t0, 1\n" // mv MT8[col=2] -> v7
      "vfmax.vv v2, v2, v5\n"                // max of col=0
      "vtmv.v.t v8, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v6\n"                // max of col=1
      "vtmv.v.t v9, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v7\n"
      "vtmv.v.t v10, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v8\n"
      "vtmv.v.t v11, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v9\n"
      "vtmv.v.t v12, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v10\n"
      "vtmv.v.t v13, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v11\n"
      "vtmv.v.t v14, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v12\n"
      "vtmv.v.t v15, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v13\n"
      "vtmv.v.t v16, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v14\n"
      "vtmv.v.t v17, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v2, v2, v15\n"
      "vtmv.v.t v18, t0\n" "addi t0, t0, 1\n"
      "vfmax.vv v4, v4, v16\n"
      "vtmv.v.t v19, t0\n" "addi t0, t0, 1\n" // mv MT8[col=14] -> v19
      "vfmax.vv v2, v2, v17\n"
      "vtmv.v.t v20, t0\n"                    // mv MT8[col=15] -> v20
      "vfmax.vv v4, v4, v18\n"
      "vfmax.vv v2, v2, v19\n"                // max of col=14
      "vfmax.vv v4, v4, v20\n"                // max of col=15
      "vfmax.vv v2, v2, v4"                   // update max -> v2
      ::: "t0", "v4", "memory");
  // alpha = exp(m_old - m_new)
  asm volatile("vfsub.vv v3, v0, v2\n"  // v3 = m_old - m_new
               SOFTMAX_EXP_ASM("v3", "v23")
               :: EXP_INPUTS : "v23", "memory");
}

// Four independent score columns through the exponential polynomial.
#define FLASH_EXP_QUAD_ASM(x0, x1, x2, x3, t0, t1, t2, t3) \
    "vfmax.vf " x0 ", " x0 ", %[ope_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[ope_lo]\n" \
    "vfmax.vf " x2 ", " x2 ", %[ope_lo]\n" \
    "vfmax.vf " x3 ", " x3 ", %[ope_lo]\n" \
    "vfmul.vf " x0 ", " x0 ", %[ope_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[ope_scale]\n" \
    "vfmul.vf " x2 ", " x2 ", %[ope_scale]\n" \
    "vfmul.vf " x3 ", " x3 ", %[ope_scale]\n" \
    "vfmv.v.f " t0 ", %[ope_c5]\n" \
    "vfmv.v.f " t1 ", %[ope_c5]\n" \
    "vfmv.v.f " t2 ", %[ope_c5]\n" \
    "vfmv.v.f " t3 ", %[ope_c5]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_c4]\n" \
    "vfadd.vf " t2 ", " t2 ", %[ope_c4]\n" \
    "vfadd.vf " t3 ", " t3 ", %[ope_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_c3]\n" \
    "vfadd.vf " t2 ", " t2 ", %[ope_c3]\n" \
    "vfadd.vf " t3 ", " t3 ", %[ope_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_c2]\n" \
    "vfadd.vf " t2 ", " t2 ", %[ope_c2]\n" \
    "vfadd.vf " t3 ", " t3 ", %[ope_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_one]\n" \
    "vfadd.vf " t2 ", " t2 ", %[ope_one]\n" \
    "vfadd.vf " t3 ", " t3 ", %[ope_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[ope_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[ope_one]\n" \
    "vfadd.vf " t2 ", " t2 ", %[ope_one]\n" \
    "vfadd.vf " t3 ", " t3 ", %[ope_one]\n" \
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
    "vfmul.vv " x3 ", " x3 ", " x3 "\n"

// Interleave four independent columns; the RTL scoreboard enforces RAW.
#define FLASH_EXP_QUAD(REG0, REG1, REG2, REG3)                            \
  asm volatile("vfsub.vv " REG0 ", " REG0 ", v2\n"                       \
               "vfsub.vv " REG1 ", " REG1 ", v2\n"                       \
               "vfsub.vv " REG2 ", " REG2 ", v2\n"                       \
               "vfsub.vv " REG3 ", " REG3 ", v2\n"                       \
               FLASH_EXP_QUAD_ASM(REG0, REG1, REG2, REG3, "v28", "v29", \
                                  "v30", "v31")                            \
               :: [ope_lo] "f"(-16.0f), [ope_scale] "f"(0.125f),           \
                  [ope_c5] "f"(0.0083333333f),                            \
                  [ope_c4] "f"(0.0416666667f),                            \
                  [ope_c3] "f"(0.1666666667f), [ope_c2] "f"(0.5f),         \
                  [ope_one] "f"(1.0f)                                     \
               : "v28", "v29", "v30", "v31", "memory")

#define FLASH_EXP_QK_QUAD(REG0, REG1, REG2, REG3, TILE, QPTR, KPTR)       \
  do {                                                                     \
    set_vector_mode();                                                     \
    FLASH_EXP_QUAD(REG0, REG1, REG2, REG3);                               \
    FLASH_QK_FOUR_VECTOR_READY(TILE, QPTR, KPTR);                          \
    FLASH_QK_FOUR(TILE, QPTR, KPTR);                                       \
  } while (0)

#if defined(FLASH_EXP_QK_GROUP2) && FLASH_EXP_QK_GROUP2
// Evaluate eight score columns before four QK reduction groups. This halves
// the exp/QK mode transitions while keeping all four QK groups in one tile.
#define FLASH_EXP_QK_TWOQUAD(A, B, C, D, E, F, G, H, TILE, QPTR, KPTR)   \
  do {                                                                     \
    set_vector_mode();                                                     \
    FLASH_EXP_QUAD(A, B, C, D);                                           \
    FLASH_EXP_QUAD(E, F, G, H);                                           \
    FLASH_QK_FOUR_VECTOR_READY(TILE, QPTR, KPTR);                         \
    FLASH_QK_FOUR(TILE, QPTR, KPTR);                                      \
    FLASH_QK_FOUR(TILE, QPTR, KPTR);                                      \
    FLASH_QK_FOUR(TILE, QPTR, KPTR);                                      \
  } while (0)
#endif

static inline __attribute__((always_inline)) void exp_scores_build_mt8(
    const uint32_t *q, const uint32_t *next_k) {
  set_fp16_matrix_mode();
  asm volatile("vtzero mt8" ::: "memory");
#if defined(FLASH_EXP_QK_GROUP4) && FLASH_EXP_QK_GROUP4
  set_vector_mode();
  FLASH_EXP_QUAD("v5", "v6", "v7", "v8");
  FLASH_EXP_QUAD("v9", "v10", "v11", "v12");
  FLASH_EXP_QUAD("v13", "v14", "v15", "v16");
  FLASH_EXP_QUAD("v17", "v18", "v19", "v20");
  FLASH_QK_FOUR_VECTOR_READY("mt8", q, next_k);
  FLASH_QK_FOUR("mt8", q, next_k);
  FLASH_QK_FOUR("mt8", q, next_k);
  FLASH_QK_FOUR("mt8", q, next_k);
  FLASH_QK_FOUR("mt8", q, next_k);
  FLASH_QK_FOUR("mt8", q, next_k);
  FLASH_QK_FOUR("mt8", q, next_k);
  FLASH_QK_FOUR("mt8", q, next_k);
#elif defined(FLASH_EXP_QK_GROUP2) && FLASH_EXP_QK_GROUP2
  FLASH_EXP_QK_TWOQUAD("v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "mt8", q, next_k);
  FLASH_EXP_QK_TWOQUAD("v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "mt8", q, next_k);
#else
  FLASH_EXP_QK_QUAD("v5", "v6", "v7", "v8", "mt8", q, next_k);
  FLASH_EXP_QK_QUAD("v9", "v10", "v11", "v12", "mt8", q, next_k);
  FLASH_EXP_QK_QUAD("v13", "v14", "v15", "v16", "mt8", q, next_k);
  FLASH_EXP_QK_QUAD("v17", "v18", "v19", "v20", "mt8", q, next_k);
#endif
}

static inline __attribute__((always_inline)) void exp_scores_build_mt0(
    const uint32_t *q, const uint32_t *next_k) {
  set_fp16_matrix_mode();
  asm volatile("vtzero mt0" ::: "memory");
#if defined(FLASH_EXP_QK_GROUP4) && FLASH_EXP_QK_GROUP4
  set_vector_mode();
  FLASH_EXP_QUAD("v5", "v6", "v7", "v8");
  FLASH_EXP_QUAD("v9", "v10", "v11", "v12");
  FLASH_EXP_QUAD("v13", "v14", "v15", "v16");
  FLASH_EXP_QUAD("v17", "v18", "v19", "v20");
  FLASH_QK_FOUR_VECTOR_READY("mt0", q, next_k);
  FLASH_QK_FOUR("mt0", q, next_k);
  FLASH_QK_FOUR("mt0", q, next_k);
  FLASH_QK_FOUR("mt0", q, next_k);
  FLASH_QK_FOUR("mt0", q, next_k);
  FLASH_QK_FOUR("mt0", q, next_k);
  FLASH_QK_FOUR("mt0", q, next_k);
  FLASH_QK_FOUR("mt0", q, next_k);
#elif defined(FLASH_EXP_QK_GROUP2) && FLASH_EXP_QK_GROUP2
  FLASH_EXP_QK_TWOQUAD("v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "mt0", q, next_k);
  FLASH_EXP_QK_TWOQUAD("v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "mt0", q, next_k);
#else
  FLASH_EXP_QK_QUAD("v5", "v6", "v7", "v8", "mt0", q, next_k);
  FLASH_EXP_QK_QUAD("v9", "v10", "v11", "v12", "mt0", q, next_k);
  FLASH_EXP_QK_QUAD("v13", "v14", "v15", "v16", "mt0", q, next_k);
  FLASH_EXP_QK_QUAD("v17", "v18", "v19", "v20", "mt0", q, next_k);
#endif
}

static inline __attribute__((always_inline)) void exp_scores_last(void) {
  set_vector_mode();
  FLASH_EXP_QUAD("v5", "v6", "v7", "v8");
  FLASH_EXP_QUAD("v9", "v10", "v11", "v12");
  FLASH_EXP_QUAD("v13", "v14", "v15", "v16");
  FLASH_EXP_QUAD("v17", "v18", "v19", "v20");
}

static inline __attribute__((always_inline)) void update_metadata(void) {
  set_vector_mode();
  asm volatile("vfmul.vv v1, v1, v3\n"
               "vmv.v.i v29, 0\n    vmv.v.i v30, 0\n    vmv.v.i v31, 0\n"
               // rowSum (E)
               "vfadd.vv v1, v1, v5\n     vfadd.vv v29, v29, v6\n"
               "vfadd.vv v30, v30, v7\n   vfadd.vv v31, v31, v8\n"
               "vfadd.vv v1, v1, v9\n     vfadd.vv v29, v29, v10\n"
               "vfadd.vv v30, v30, v11\n  vfadd.vv v31, v31, v12\n"
               "vfadd.vv v1, v1, v13\n    vfadd.vv v29, v29, v14\n"
               "vfadd.vv v30, v30, v15\n  vfadd.vv v31, v31, v16\n"
               "vfadd.vv v1, v1, v17\n    vfadd.vv v29, v29, v18\n"
               "vfadd.vv v30, v30, v19\n  vfadd.vv v31, v31, v20\n"
               "vfadd.vv v1, v1, v29\n"
               "vfadd.vv v30, v30, v31\n"
               "vmv.v.v v0, v2\n"         // update max v2 -> v0
               "vfadd.vv v1, v1, v30\n"
               ::: "v29", "v30", "v31", "memory");
}

static inline __attribute__((always_inline)) void scale_output(void) {
  set_pv_mode();
  uintptr_t tss = 0x20000000u;
  for (unsigned row = 0; row < kTile; row += 4) {
    asm volatile("vtmv.v.t v28, %0\n"
                 "vtmv.v.t v29, %1\n"
                 "vtmv.v.t v30, %2\n"
                 "vfmul.vv v28, v28, v3\n"
                 "vtmv.v.t v31, %3\n"
                 "vfmul.vv v29, v29, v3\n"
                 "vfmul.vv v30, v30, v3\n"
                 "vfmul.vv v31, v31, v3"
                 :: "r"(tss), "r"(tss + 1), "r"(tss + 2), "r"(tss + 3)
                 : "v28", "v29", "v30", "v31", "memory");
    asm volatile("vtmv.t.v %0, v28\n"
                 "vtmv.t.v %1, v29\n"
                 "vtmv.t.v %2, v30\n"
                 "vtmv.t.v %3, v31"
                 :: "r"(tss), "r"(tss + 1), "r"(tss + 2), "r"(tss + 3)
                 : "memory");
    tss += 4;
  }
}

#if FLASH_VRF_PACK
// Each 32-bit lane of v26 contains two adjacent FP16 probabilities.  The
// widening sum followed by 65535 * high computes low | (high << 16) without
// a scratch-memory round trip.  The following vtfmm consumes v26 directly.
#define FLASH_PV_PAIR_PRELOADED(REG0, REG1, VREG)                     \
  do {                                                                 \
    const uintptr_t pack_multiplier = 65535u;                         \
    asm volatile(                                                      \
        "vsetivli zero, 16, e16, mf2, ta, ma\n"                     \
        "vfncvt.f.f.w v24, " REG0 "\n"                              \
        "vfncvt.f.f.w v25, " REG1 "\n"                              \
        "vwaddu.vv v26, v24, v25\n"                                 \
        "vwmaccu.vx v26, %0, v25"                                    \
        :: "r"(pack_multiplier)                                        \
        : "v24", "v25", "v26", "memory");                       \
    set_fp16_matrix_mode();                                            \
    asm volatile("vtfmm.tvv mt4, " VREG ", v26" ::: "memory");    \
  } while (0)
#endif
#define FLASH_PV_PAIR(REG0, REG1, VPTR, SCRATCH)                 \
  do {                                                                        \
    asm volatile(                                                             \
        "vsetivli zero, 16, e16, mf2, ta, ma\n"                             \
        "vfncvt.f.f.w v24, " REG0 "\n"                                     \
        "vfncvt.f.f.w v25, " REG1 "\n"                                     \
        "vsse16.v v24, (%0), %1"                                            \
        :: "r"(SCRATCH), "r"((uintptr_t)4)                                 \
        : "v24", "v25", "memory");                                        \
    asm volatile("vsse16.v v25, (%0), %1"                                   \
                 :: "r"((uint16_t *)(SCRATCH) + 1), "r"((uintptr_t)4)      \
                 : "memory");                                               \
    ope_vsync(SCRATCH);                                                       \
    asm volatile(                                                             \
        "vsetivli zero, 16, e32, m1, ta, ma\n"                              \
        "vle32.v v26, (%0)\n"                                               \
        "vle32.v v21, (%1)"                                                  \
        :: "r"(SCRATCH), "r"(VPTR)                                         \
        : "v21", "v24", "v25", "v26", "memory");                        \
    set_fp16_matrix_mode();                                                   \
    asm volatile("vtfmm.tvv mt4, v21, v26" ::: "memory");            \
    VPTR += kTile;                                                            \
  } while (0)

static inline __attribute__((always_inline)) void update_output(
    const uint32_t *v, uint32_t *pair_scratch) {
#if FLASH_VRF_PACK
  (void)pair_scratch;
  // v5-v20 still hold probabilities.  Fill the free activation registers
  // before any PV instruction, using aligned LMUL 2/4 group loads.
  asm volatile("vsetivli zero, 16, e32, m1, ta, ma\n"
               "vle32.v v21, (%0)\n"
               "vle32.v v27, (%1)\n"
               "vsetvli zero, %4, e32, m2, ta, ma\n"
               "vle32.v v22, (%2)\n"
               "vsetvli zero, %5, e32, m4, ta, ma\n"
               "vle32.v v28, (%3)"
               :: "r"(v), "r"(v + 3 * kTile), "r"(v + kTile),
                  "r"(v + 4 * kTile), "r"((uintptr_t)32),
                  "r"((uintptr_t)64)
               : "v21", "v22", "v23", "v27", "v28", "v29", "v30",
                 "v31", "memory");
  FLASH_PV_PAIR_PRELOADED("v5", "v6", "v21");
  FLASH_PV_PAIR_PRELOADED("v7", "v8", "v22");
  FLASH_PV_PAIR_PRELOADED("v9", "v10", "v23");
  FLASH_PV_PAIR_PRELOADED("v11", "v12", "v27");
  FLASH_PV_PAIR_PRELOADED("v13", "v14", "v28");
  FLASH_PV_PAIR_PRELOADED("v15", "v16", "v29");
  FLASH_PV_PAIR_PRELOADED("v17", "v18", "v30");
  FLASH_PV_PAIR_PRELOADED("v19", "v20", "v31");
#else
  FLASH_PV_PAIR("v5", "v6", v, pair_scratch);
  FLASH_PV_PAIR("v7", "v8", v, pair_scratch);
  FLASH_PV_PAIR("v9", "v10", v, pair_scratch);
  FLASH_PV_PAIR("v11", "v12", v, pair_scratch);
  FLASH_PV_PAIR("v13", "v14", v, pair_scratch);
  FLASH_PV_PAIR("v15", "v16", v, pair_scratch);
  FLASH_PV_PAIR("v17", "v18", v, pair_scratch);
  FLASH_PV_PAIR("v19", "v20", v, pair_scratch);
#endif
}

#if FLASH_EARLY_DRAIN
// Four probability source registers are dead after two PV pairs. Refill them
// from the next score tile while the preceding PV work retires into mt4.
static inline __attribute__((always_inline)) void update_output_predrain(
    const uint32_t *v, uint32_t *pair_scratch, uintptr_t score_base) {
  set_pv_mode();
  asm volatile("vmv.v.v v2, v0\nvmv.v.v v4, v0" ::: "v4", "memory");
#if FLASH_EARLY_DRAIN == 2
#define FLASH_DRAIN_PAIR(A, B, BASE, PAIR)                             \
  do {                                                                 \
    set_pv_mode();                                                     \
    asm volatile("vtmv.v.t " A ", %0\n"                              \
                 "vtmv.v.t " B ", %1\n"                              \
                 "vfmax.vv v2, v2, " A "\n"                          \
                 "vfmax.vv v4, v4, " B                                \
                 :: "r"((BASE) + 2 * (PAIR)),                         \
                    "r"((BASE) + 2 * (PAIR) + 1)                     \
                 : "memory");                                        \
  } while (0)
  FLASH_PV_PAIR("v5", "v6", v, pair_scratch);
  FLASH_DRAIN_PAIR("v5", "v6", score_base, 0);
  FLASH_PV_PAIR("v7", "v8", v, pair_scratch);
  FLASH_DRAIN_PAIR("v7", "v8", score_base, 1);
  FLASH_PV_PAIR("v9", "v10", v, pair_scratch);
  FLASH_DRAIN_PAIR("v9", "v10", score_base, 2);
  FLASH_PV_PAIR("v11", "v12", v, pair_scratch);
  FLASH_DRAIN_PAIR("v11", "v12", score_base, 3);
  FLASH_PV_PAIR("v13", "v14", v, pair_scratch);
  FLASH_DRAIN_PAIR("v13", "v14", score_base, 4);
  FLASH_PV_PAIR("v15", "v16", v, pair_scratch);
  FLASH_DRAIN_PAIR("v15", "v16", score_base, 5);
  FLASH_PV_PAIR("v17", "v18", v, pair_scratch);
  FLASH_DRAIN_PAIR("v17", "v18", score_base, 6);
  FLASH_PV_PAIR("v19", "v20", v, pair_scratch);
  FLASH_DRAIN_PAIR("v19", "v20", score_base, 7);
#undef FLASH_DRAIN_PAIR
#else
#define FLASH_DRAIN_QUAD(A, B, C, D, BASE, GROUP)                         \
  do {                                                                    \
    set_pv_mode();                                                        \
    asm volatile("vtmv.v.t " A ", %0\n"                                   \
                 "vtmv.v.t " B ", %1\n"                                   \
                 "vtmv.v.t " C ", %2\n"                                   \
                 "vfmax.vv v2, v2, " A "\n"                               \
                 "vtmv.v.t " D ", %3\n"                                   \
                 "vfmax.vv v4, v4, " B "\n"                               \
                 "vfmax.vv v2, v2, " C "\n"                               \
                 "vfmax.vv v4, v4, " D                                   \
                 :: "r"((BASE) + 4 * (GROUP)),                            \
                    "r"((BASE) + 4 * (GROUP) + 1),                        \
                    "r"((BASE) + 4 * (GROUP) + 2),                        \
                    "r"((BASE) + 4 * (GROUP) + 3)                        \
                 : "memory");                                           \
  } while (0)
  FLASH_PV_PAIR("v5", "v6", v, pair_scratch);
  FLASH_PV_PAIR("v7", "v8", v, pair_scratch);
  FLASH_DRAIN_QUAD("v5", "v6", "v7", "v8", score_base, 0);
  FLASH_PV_PAIR("v9", "v10", v, pair_scratch);
  FLASH_PV_PAIR("v11", "v12", v, pair_scratch);
  FLASH_DRAIN_QUAD("v9", "v10", "v11", "v12", score_base, 1);
  FLASH_PV_PAIR("v13", "v14", v, pair_scratch);
  FLASH_PV_PAIR("v15", "v16", v, pair_scratch);
  FLASH_DRAIN_QUAD("v13", "v14", "v15", "v16", score_base, 2);
  FLASH_PV_PAIR("v17", "v18", v, pair_scratch);
  FLASH_PV_PAIR("v19", "v20", v, pair_scratch);
  FLASH_DRAIN_QUAD("v17", "v18", "v19", "v20", score_base, 3);
#undef FLASH_DRAIN_QUAD
#endif
}

static inline __attribute__((always_inline)) void finish_predrained_max(void) {
  set_pv_mode();
  asm volatile("vfmax.vv v2, v2, v4" ::: "memory");
  asm volatile("vfsub.vv v3, v0, v2\n" SOFTMAX_EXP_ASM("v3", "v23")
               :: EXP_INPUTS : "v23", "memory");
}
#endif

static inline __attribute__((always_inline)) void normalize_and_store(float *o) {
  set_vector_mode();
  asm volatile(RECIP_ASM("v1", "v2", "v23")
               :: RECIP_INPUTS : "v2", "v23", "memory");
  set_pv_mode();
  uintptr_t tss = 0x20000000u;
  for (unsigned row = 0; row < kTile; row += 4) {
    asm volatile("vtmv.v.t v28, %0\n"
                 "vtmv.v.t v29, %1\n"
                 "vtmv.v.t v30, %2\n"
                 "vfmul.vv v28, v28, v2\n"
                 "vtmv.v.t v31, %3\n"
                 "vfmul.vv v29, v29, v2\n"
                 "vfmul.vv v30, v30, v2\n"
                 "vfmul.vv v31, v31, v2\n"
                 "vtmv.t.v %0, v28\n"
                 "vtmv.t.v %1, v29\n"
                 "vtmv.t.v %2, v30\n"
                 "vtmv.t.v %3, v31"
                 :: "r"(tss), "r"(tss + 1), "r"(tss + 2), "r"(tss + 3)
                 : "v28", "v29", "v30", "v31", "memory");
    tss += 4;
  }
  tss = 0x20000000u;
  for (unsigned row = 0; row < kTile; ++row) {
    asm volatile("vtse32 %0, (%1)" :: "r"(tss), "r"(o) : "memory");
    ++tss;
    o += kTile;
  }
  asm volatile("lw zero, -4(%0)" :: "r"(o) : "memory");
}

void __attribute__((noinline, aligned(64))) flashattn_tq16(
    const uint32_t *q, const uint32_t *k, const uint32_t *v,
    uint32_t *pair_scratch, float *output) {
  _Static_assert(FLASH_T == 64 && FLASH_D == 64 && FLASH_DV == 16,
                 "the first direct-VRF schedule is T64 D64 DV16");

  set_vector_mode();
  asm volatile("vfmv.v.f v0, %0\nvmv.v.i v1, 0"
               :: "f"(-1e30f) : "memory");
  set_pv_mode();
  asm volatile("vtzero mt4" ::: "memory");

  qk_tile_mt0(q, k);
  for (unsigned stage = 0; stage < kKeyStages; ++stage) {
#if FLASH_EARLY_DRAIN
    if (stage == 0) drain_mt0_and_max();
    else finish_predrained_max();
#else
    if ((stage & 1u) == 0) drain_mt0_and_max();
    else drain_mt8_and_max();
#endif

    if (stage + 1 < kKeyStages) {
      const uint32_t *next_k =
          k + (stage + 1) * kReductionPairs * kTile;
      if ((stage & 1u) == 0) exp_scores_build_mt8(q, next_k);
      else exp_scores_build_mt0(q, next_k);
    } else {
      exp_scores_last();
    }
    update_metadata();
    // Temporary architectural completion fence for the RVV -> OPE boundary.
    // The final output overwrites this scratch after all key stages.
    set_vector_mode();
    asm volatile("vse32.v v1, (%0)\nlw zero, 0(%0)"
                 :: "r"(output + 16) : "memory");
    scale_output();
    const uint32_t *stage_v = v + stage * (kTile / 2) * kTile;
#if FLASH_EARLY_DRAIN
    if (stage + 1 < kKeyStages)
      update_output_predrain(stage_v, pair_scratch,
                             (stage & 1u) ? 0x01000000u : 0x41000000u);
    else
      update_output(stage_v, pair_scratch);
#else
    update_output(stage_v, pair_scratch);
#endif
  }
  normalize_and_store(output);
}
