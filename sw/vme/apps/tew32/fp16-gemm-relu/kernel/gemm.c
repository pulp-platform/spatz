// Copyright 2026 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0

#include "gemm.h"
#include "gather_gemm_1x4.h"

enum {
  GR_TILE_WORDS_K64 = 32 * TE,
  GR_TILE_WORDS_K128 = 64 * TE,
  GR_GROUP_WORDS = 4 * TE,
  GR_GROUP16_WORDS = 8 * TE,
};

static inline __attribute__((always_inline)) void gr_zero_pair(unsigned high) {
  asm volatile("vsetivli zero, 16, e32, m1, ta, ma\n"
               "msetmtypei 1, 2, 0\n"
               "msettn zero, %0"
               :: "r"((uintptr_t)TE) : "memory");
  if (high)
    asm volatile("vtzero mt8\nvtzero mt12" ::: "memory");
  else
    asm volatile("vtzero mt0\nvtzero mt4" ::: "memory");
}

static inline __attribute__((always_inline)) void gr_load_operands(
    const uint32_t *a, const uint32_t *b0, const uint32_t *b1) {
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n"
               "vle32.v v0, (%1)\n"
               "vle32.v v16, (%2)\n"
               "vle32.v v20, (%3)"
               :: "r"((uintptr_t)(4 * TE)), "r"(a), "r"(b0), "r"(b1)
               : "v0", "v1", "v2", "v3",
                 "v16", "v17", "v18", "v19",
                 "v20", "v21", "v22", "v23", "memory");
}

static inline __attribute__((always_inline)) void gr_set_compute_type(void) {
  const uintptr_t mtype = (TE << 10) | (2u << 5) | 2u;
  asm volatile("msetmtype %0, %1\nmsettn zero, %2"
               :: "r"(mtype), "r"((uintptr_t)0xc8u),
                  "r"((uintptr_t)TE) : "memory");
}

static inline __attribute__((always_inline)) void gr_set_drain_type(void) {
  asm volatile("msetmtypei 1, 2, 0\n"
               "msettm zero, %0\n"
               "msettn zero, %0"
               :: "r"((uintptr_t)TE) : "memory");
}

static inline __attribute__((always_inline)) void gr_add_bias_pair(
    const float *bias, unsigned ldc, unsigned high) {
  const uintptr_t bias_mtype = (TE << 10) | (1u << 5) | 1u;
  const float one = 1.0f;
  asm volatile("vsetivli zero, 16, e32, m1, ta, ma\n"
               "vlse32.v v0, (%0), %1\n"
               "vfmv.v.f v16, %2\n"
               "msetmtype %3, %4\n"
               "msettn zero, %5"
               :: "r"(bias), "r"((uintptr_t)(ldc * sizeof(float))),
                  "f"(one), "r"(bias_mtype), "r"((uintptr_t)0xd0u),
                  "r"((uintptr_t)TE)
               : "v0", "v16", "memory");
  if (high)
    asm volatile("vtfmm.tvv mt8, v0, v16\n"
                 "vtfmm.tvv mt12, v0, v16" ::: "memory");
  else
    asm volatile("vtfmm.tvv mt0, v0, v16\n"
                 "vtfmm.tvv mt4, v0, v16" ::: "memory");
}

static inline __attribute__((always_inline)) void gr_fma_head(unsigned high) {
  if (high) {
    asm volatile("vtfmm.tvv mt8, v0, v16\n"
                 "vtfmm.tvv mt12, v0, v20\n"
                 "vtfmm.tvv mt8, v1, v17\n"
                 "vtfmm.tvv mt12, v1, v21" ::: "memory");
  } else {
    asm volatile("vtfmm.tvv mt0, v0, v16\n"
                 "vtfmm.tvv mt4, v0, v20\n"
                 "vtfmm.tvv mt0, v1, v17\n"
                 "vtfmm.tvv mt4, v1, v21" ::: "memory");
  }
}

static inline __attribute__((always_inline)) void gr_fma_tail(unsigned high) {
  if (high) {
    asm volatile("vtfmm.tvv mt8, v2, v18\n"
                 "vtfmm.tvv mt12, v2, v22\n"
                 "vtfmm.tvv mt8, v3, v19\n"
                 "vtfmm.tvv mt12, v3, v23" ::: "memory");
  } else {
    asm volatile("vtfmm.tvv mt0, v2, v18\n"
                 "vtfmm.tvv mt4, v2, v22\n"
                 "vtfmm.tvv mt0, v3, v19\n"
                 "vtfmm.tvv mt4, v3, v23" ::: "memory");
  }
}

static inline __attribute__((always_inline)) void gr_fma_first(unsigned high) {
  if (high) {
    asm volatile("vtfmm.tvv mt8, v0, v16\n"
                 "vtfmm.tvv mt8, v1, v17\n"
                 "vtfmm.tvv mt8, v2, v18\n"
                 "vtfmm.tvv mt8, v3, v19" ::: "memory");
  } else {
    asm volatile("vtfmm.tvv mt0, v0, v16\n"
                 "vtfmm.tvv mt0, v1, v17\n"
                 "vtfmm.tvv mt0, v2, v18\n"
                 "vtfmm.tvv mt0, v3, v19" ::: "memory");
  }
}

static inline __attribute__((always_inline)) void gr_fma_second(unsigned high) {
  if (high) {
    asm volatile("vtfmm.tvv mt12, v0, v20\n"
                 "vtfmm.tvv mt12, v1, v21\n"
                 "vtfmm.tvv mt12, v2, v22\n"
                 "vtfmm.tvv mt12, v3, v23" ::: "memory");
  } else {
    asm volatile("vtfmm.tvv mt4, v0, v20\n"
                 "vtfmm.tvv mt4, v1, v21\n"
                 "vtfmm.tvv mt4, v2, v22\n"
                 "vtfmm.tvv mt4, v3, v23" ::: "memory");
  }
}

static inline __attribute__((always_inline)) void gr_preload_b0(
    const uint32_t *b0) {
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n"
               "vle32.v v16, (%1)"
               :: "r"((uintptr_t)(4 * TE)), "r"(b0)
               : "v16", "v17", "v18", "v19", "memory");
}

static inline __attribute__((always_inline)) void gr_preload_a_b1(
    const uint32_t *a, const uint32_t *b1) {
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n"
               "vle32.v v0, (%1)\n"
               "vle32.v v20, (%2)"
               :: "r"((uintptr_t)(4 * TE)), "r"(a), "r"(b1)
               : "v0", "v1", "v2", "v3",
                 "v20", "v21", "v22", "v23", "memory");
}

static inline __attribute__((always_inline)) void gr_load_operands16(
    const uint32_t *a, const uint32_t *b0, const uint32_t *b1) {
  asm volatile("vsetvli zero, %0, e16, m8, ta, ma\n"
               "vle16.v v0, (%1)\n"
               "vle16.v v16, (%2)\n"
               "vle16.v v24, (%3)"
               :: "r"((uintptr_t)(16 * TE)), "r"(a), "r"(b0), "r"(b1)
               : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                 "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                 "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                 "memory");
}

#define GR_FMA8_FIRST(mt_)                                                \
  asm volatile("vtfmm.tvv " #mt_ ", v0, v16\n"                       \
               "vtfmm.tvv " #mt_ ", v1, v17\n"                       \
               "vtfmm.tvv " #mt_ ", v2, v18\n"                       \
               "vtfmm.tvv " #mt_ ", v3, v19\n"                       \
               "vtfmm.tvv " #mt_ ", v4, v20\n"                       \
               "vtfmm.tvv " #mt_ ", v5, v21\n"                       \
               "vtfmm.tvv " #mt_ ", v6, v22\n"                       \
               "vtfmm.tvv " #mt_ ", v7, v23" ::: "memory")

#define GR_FMA8_SECOND(mt_)                                               \
  asm volatile("vtfmm.tvv " #mt_ ", v0, v24\n"                       \
               "vtfmm.tvv " #mt_ ", v1, v25\n"                       \
               "vtfmm.tvv " #mt_ ", v2, v26\n"                       \
               "vtfmm.tvv " #mt_ ", v3, v27\n"                       \
               "vtfmm.tvv " #mt_ ", v4, v28\n"                       \
               "vtfmm.tvv " #mt_ ", v5, v29\n"                       \
               "vtfmm.tvv " #mt_ ", v6, v30\n"                       \
               "vtfmm.tvv " #mt_ ", v7, v31" ::: "memory")

#define GR_FMA4_FIRST_LO(mt_)                                             \
  asm volatile("vtfmm.tvv " #mt_ ", v0, v16\n"                       \
               "vtfmm.tvv " #mt_ ", v1, v17\n"                       \
               "vtfmm.tvv " #mt_ ", v2, v18\n"                       \
               "vtfmm.tvv " #mt_ ", v3, v19" ::: "memory")
#define GR_FMA4_FIRST_HI(mt_)                                             \
  asm volatile("vtfmm.tvv " #mt_ ", v4, v20\n"                       \
               "vtfmm.tvv " #mt_ ", v5, v21\n"                       \
               "vtfmm.tvv " #mt_ ", v6, v22\n"                       \
               "vtfmm.tvv " #mt_ ", v7, v23" ::: "memory")
#define GR_FMA4_SECOND_LO(mt_)                                            \
  asm volatile("vtfmm.tvv " #mt_ ", v0, v24\n"                       \
               "vtfmm.tvv " #mt_ ", v1, v25\n"                       \
               "vtfmm.tvv " #mt_ ", v2, v26\n"                       \
               "vtfmm.tvv " #mt_ ", v3, v27" ::: "memory")
#define GR_FMA4_SECOND_LO_PREP(mt_, a_, b1_, next_offset_, next_a_,       \
                                next_b1_)                                  \
  asm volatile("vtfmm.tvv " #mt_ ", v0, v24\n"                        \
               "add %[na], %[a], %[off]\n"                               \
               "vtfmm.tvv " #mt_ ", v1, v25\n"                        \
               "add %[nb1], %[b1], %[off]\n"                             \
               "vtfmm.tvv " #mt_ ", v2, v26\n"                        \
               "vtfmm.tvv " #mt_ ", v3, v27"                           \
               : [na] "=&r"(next_a_), [nb1] "=&r"(next_b1_)             \
               : [a] "r"(a_), [b1] "r"(b1_), [off] "r"(next_offset_)   \
               : "memory")
#define GR_FMA4_SECOND_HI(mt_)                                            \
  asm volatile("vtfmm.tvv " #mt_ ", v4, v28\n"                       \
               "vtfmm.tvv " #mt_ ", v5, v29\n"                       \
               "vtfmm.tvv " #mt_ ", v6, v30\n"                       \
               "vtfmm.tvv " #mt_ ", v7, v31" ::: "memory")

#define GR_FMA4_FIRST_LO_PREP(mt_, selector_, sel_col_, sel_next_,         \
                              sel_next_col_)                                \
  asm volatile("vtfmm.tvv " #mt_ ", v0, v16\n"                        \
               "add %[sc], %[s], %[half]\n"                               \
               "vtfmm.tvv " #mt_ ", v1, v17\n"                        \
               "addi %[sn], %[s], 1\n"                                    \
               "vtfmm.tvv " #mt_ ", v2, v18\n"                        \
               "add %[snc], %[sn], %[half]\n"                             \
               "vtfmm.tvv " #mt_ ", v3, v19"                            \
               : [sc] "=&r"(sel_col_), [sn] "=&r"(sel_next_),           \
                 [snc] "=&r"(sel_next_col_)                               \
               : [s] "r"(selector_),                                      \
                 [half] "r"((uintptr_t)0x20000000u)                       \
               : "memory")

#define GR_FMA4_FIRST_HI_PREP(mt_, b0_, next_off_, next_b0_)               \
  asm volatile("vtfmm.tvv " #mt_ ", v4, v20\n"                        \
               "add %[nb0], %[b0], %[off]\n"                              \
               "vtfmm.tvv " #mt_ ", v5, v21\n"                        \
               "vtfmm.tvv " #mt_ ", v6, v22\n"                        \
               "vtfmm.tvv " #mt_ ", v7, v23"                            \
               : [nb0] "=&r"(next_b0_)                                    \
               : [b0] "r"((uintptr_t)(b0_)), [off] "r"(next_off_)        \
               : "memory")

static inline __attribute__((always_inline)) void gr_preload_b0_16(
    const uint32_t *b0) {
  asm volatile("vsetvli zero, %0, e16, m8, ta, ma\n"
               "vle16.v v16, (%1)"
               :: "r"((uintptr_t)(16 * TE)), "r"(b0)
               : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                 "memory");
}

static inline __attribute__((always_inline)) void gr_preload_b0_half(
    const uint32_t *b0, unsigned high_half) {
  const uint32_t *src = b0 + high_half * GR_GROUP_WORDS;
  if (high_half)
    asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                 "vle16.v v20, (%1)"
                 :: "r"((uintptr_t)(8 * TE)), "r"(src)
                 : "v20", "v21", "v22", "v23", "memory");
  else
    asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                 "vle16.v v16, (%1)"
                 :: "r"((uintptr_t)(8 * TE)), "r"(src)
                 : "v16", "v17", "v18", "v19", "memory");
}

static inline __attribute__((always_inline)) void gr_preload_a_b1_half(
    const uint32_t *a, const uint32_t *b1, unsigned high_half) {
  const uint32_t *asrc = a + high_half * GR_GROUP_WORDS;
  const uint32_t *bsrc = b1 + high_half * GR_GROUP_WORDS;
  if (high_half)
    asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                 "vle16.v v4, (%1)\n"
                 "vle16.v v28, (%2)"
                 :: "r"((uintptr_t)(8 * TE)), "r"(asrc), "r"(bsrc)
                 : "v4", "v5", "v6", "v7",
                   "v28", "v29", "v30", "v31", "memory");
  else
    asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                 "vle16.v v0, (%1)\n"
                 "vle16.v v24, (%2)"
                 :: "r"((uintptr_t)(8 * TE)), "r"(asrc), "r"(bsrc)
                 : "v0", "v1", "v2", "v3",
                   "v24", "v25", "v26", "v27", "memory");
}

static inline __attribute__((always_inline)) void gr_compute_pair16(
    const uint32_t *a, const uint32_t *b0, const uint32_t *b1,
    unsigned high, unsigned groups) {
  gr_zero_pair(high);
  gr_load_operands16(a, b0, b1);
  gr_set_compute_type();
#pragma clang loop unroll(disable)
  for (unsigned kg = 0; kg < groups; ++kg) {
    const unsigned has_next = kg + 1 != groups;
    const uint32_t *next_a = a + (kg + 1) * GR_GROUP16_WORDS;
    const uint32_t *next_b0 = b0 + (kg + 1) * GR_GROUP16_WORDS;
    const uint32_t *next_b1 = b1 + (kg + 1) * GR_GROUP16_WORDS;
    gr_set_compute_type();
    if (high) {
      GR_FMA4_FIRST_LO(mt8);
      if (has_next) gr_preload_b0_half(next_b0, 0);
      if (has_next) gr_set_compute_type();
      GR_FMA4_FIRST_HI(mt8);
      if (has_next) gr_preload_b0_half(next_b0, 1);
      if (has_next) gr_set_compute_type();
      GR_FMA4_SECOND_LO(mt12);
      if (has_next) gr_preload_a_b1_half(next_a, next_b1, 0);
      if (has_next) gr_set_compute_type();
      GR_FMA4_SECOND_HI(mt12);
    } else {
      GR_FMA4_FIRST_LO(mt0);
      if (has_next) gr_preload_b0_half(next_b0, 0);
      if (has_next) gr_set_compute_type();
      GR_FMA4_FIRST_HI(mt0);
      if (has_next) gr_preload_b0_half(next_b0, 1);
      if (has_next) gr_set_compute_type();
      GR_FMA4_SECOND_LO(mt4);
      if (has_next) gr_preload_a_b1_half(next_a, next_b1, 0);
      if (has_next) gr_set_compute_type();
      GR_FMA4_SECOND_HI(mt4);
    }
    if (has_next)
      gr_preload_a_b1_half(next_a, next_b1, 1);
  }
}

static inline __attribute__((always_inline)) void gr_compute_pair(
    const uint32_t *a, const uint32_t *b0, const uint32_t *b1,
    unsigned high, unsigned groups) {
  gr_zero_pair(high);
  gr_load_operands(a, b0, b1);
#pragma clang loop unroll(disable)
  for (unsigned kg = 0; kg < groups; ++kg) {
    gr_set_compute_type();
    gr_fma_head(high);
    gr_fma_tail(high);
    if (kg + 1 != groups)
      gr_load_operands(a + (kg + 1) * GR_GROUP_WORDS,
                       b0 + (kg + 1) * GR_GROUP_WORDS,
                       b1 + (kg + 1) * GR_GROUP_WORDS);
  }
}

// K64 has eight K8 groups, exactly matching the eight two-row drain batches.
// One group is split around each four-row tile move, so OPE work brackets the
// otherwise costly compute-to-drain type transition.
static inline __attribute__((always_inline)) void gr_compute_drain_pair_k64(
    const uint32_t *a, const uint32_t *b0, const uint32_t *b1,
    float *previous, unsigned high) {
  gr_zero_pair(high);
  gr_load_operands(a, b0, b1);
  uintptr_t ap = (uintptr_t)a;
  uintptr_t bp0 = (uintptr_t)b0;
  uintptr_t bp1 = (uintptr_t)b1;
  uintptr_t out = (uintptr_t)previous;
  uintptr_t out1 = out + 64 * sizeof(float);
  uintptr_t sel0 = high ? 0u : 0x40000000u;
  uintptr_t sel1 = sel0 + 0x20000000u;
  uintptr_t count = TE / 2;
  const uintptr_t compute_mtype = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t drain_mtype = (TE << 10) | (1u << 5) | 1u;
#define GR_K64_DRAIN_LOOP(mt0_, mt1_)                                      \
  asm volatile(                                                            \
      "1:\n"                                                              \
      "msetmtype %[cm], %[cv]\n"                                          \
      "msettn zero, %[tn]\n"                                              \
      "vtfmm.tvv " #mt0_ ", v0, v16\n"                                   \
      "vtfmm.tvv " #mt0_ ", v1, v17\n"                                   \
      "vtfmm.tvv " #mt0_ ", v2, v18\n"                                   \
      "vtfmm.tvv " #mt0_ ", v3, v19\n"                                   \
      "msetmtype %[dm], %[dv]\n"                                          \
      "msettn zero, %[tn]\n"                                              \
      "vtmv.v.t v8, %[s0]\n"                                              \
      "vtmv.v.t v9, %[s1]\n"                                              \
      "addi %[s0], %[s0], 1\n"                                            \
      "addi %[s1], %[s1], 1\n"                                            \
      "vtmv.v.t v10, %[s0]\n"                                             \
      "vtmv.v.t v11, %[s1]\n"                                             \
      "addi %[s0], %[s0], 1\n"                                            \
      "addi %[s1], %[s1], 1\n"                                            \
      "msetmtype %[cm], %[cv]\n"                                          \
      "msettn zero, %[tn]\n"                                              \
      "vtfmm.tvv " #mt1_ ", v0, v20\n"                                   \
      "vtfmm.tvv " #mt1_ ", v1, v21\n"                                   \
      "vtfmm.tvv " #mt1_ ", v2, v22\n"                                   \
      "vtfmm.tvv " #mt1_ ", v3, v23\n"                                   \
      "addi %[cnt], %[cnt], -1\n"                                         \
      "beqz %[cnt], 2f\n"                                                  \
      "addi %[ap], %[ap], 256\n"                                          \
      "addi %[b0], %[b0], 256\n"                                          \
      "addi %[b1], %[b1], 256\n"                                          \
      "vsetvli zero, %[vl4], e32, m4, ta, ma\n"                            \
      "vle32.v v0, (%[ap])\n"                                             \
      "vle32.v v16, (%[b0])\n"                                            \
      "vle32.v v20, (%[b1])\n"                                            \
      "2:\n"                                                              \
      "vsetvli zero, %[vl2], e32, m2, ta, ma\n"                            \
      "flw ft1, 0(%[out])\n"                                               \
      "flw ft2, 0(%[out1])\n"                                              \
      "vfadd.vf v8, v8, ft1\n"                                            \
      "vfadd.vf v10, v10, ft2\n"                                          \
      RELU_ASM("v8")                                                       \
      RELU_ASM("v10")                                                      \
      "vse32.v v8, (%[out])\n"                                            \
      "vse32.v v10, (%[out1])\n"                                          \
      "addi %[out], %[out], 512\n"                                        \
      "addi %[out1], %[out1], 512\n"                                      \
      "bnez %[cnt], 1b"                                                    \
      : [ap] "+&r"(ap), [b0] "+&r"(bp0), [b1] "+&r"(bp1),              \
        [out] "+&r"(out), [out1] "+&r"(out1), [s0] "+&r"(sel0),       \
        [s1] "+&r"(sel1),                                                  \
        [cnt] "+&r"(count)                                                 \
      : [cm] "r"(compute_mtype), [cv] "r"((uintptr_t)0xc8u),             \
        [dm] "r"(drain_mtype), [dv] "r"((uintptr_t)0xd0u),               \
        [tn] "r"((uintptr_t)TE), [vl4] "r"((uintptr_t)(4 * TE)),         \
        [vl2] "r"((uintptr_t)(2 * TE)), RELU_INPUTS                       \
      : "ft1", "ft2", "v0", "v1", "v2", "v3", "v8", "v9",       \
        "v10", "v11", "v16", "v17", "v18", "v19", "v20", "v21", \
        "v22", "v23", "memory")
  if (high)
    GR_K64_DRAIN_LOOP(mt8, mt12);
  else
    GR_K64_DRAIN_LOOP(mt0, mt4);
#undef GR_K64_DRAIN_LOOP
}

static inline __attribute__((always_inline)) void gr_compute_drain_pair(
    const uint32_t *a, const uint32_t *b0, const uint32_t *b1,
    float *previous, float *current, unsigned ldc, unsigned high,
    unsigned groups) {
  const uintptr_t previous_base = high ? 0u : 0x40000000u;
  const unsigned pure_groups = groups - TE / 2;
  gr_zero_pair(high);
  gr_load_operands16(a, b0, b1);
  gr_set_compute_type();

  // Draining a 16-row tile needs eight two-row batches.  Any extra reduction
  // depth is completed first without touching the previous result.  The final
  // eight K16 groups form the steady-state microkernel: one group hides each
  // move/post-op batch.
#pragma clang loop unroll(disable)
  for (unsigned kg = 0; kg < pure_groups; ++kg) {
    gr_set_compute_type();
    if (high) {
      GR_FMA8_FIRST(mt8);
      GR_FMA8_SECOND(mt12);
    } else {
      GR_FMA8_FIRST(mt0);
      GR_FMA8_SECOND(mt4);
    }
    gr_load_operands16(a + (kg + 1) * GR_GROUP16_WORDS,
                       b0 + (kg + 1) * GR_GROUP16_WORDS,
                       b1 + (kg + 1) * GR_GROUP16_WORDS);
  }

#pragma clang loop unroll(disable)
  for (unsigned batch = 0; batch < TE / 2; ++batch) {
    const unsigned row = 2 * batch;
    const unsigned kg = pure_groups + batch;
    const uintptr_t selector = previous_base + row;
    uintptr_t sel_col, sel_next, sel_next_col;
    const uintptr_t next_offset =
        (uintptr_t)(kg + 1) * GR_GROUP16_WORDS * sizeof(uint32_t);
    uintptr_t next_b0_addr;
    gr_set_compute_type();
    if (high)
      GR_FMA4_FIRST_LO_PREP(mt8, selector, sel_col, sel_next, sel_next_col);
    else
      GR_FMA4_FIRST_LO_PREP(mt0, selector, sel_col, sel_next, sel_next_col);
    gr_set_drain_type();
    asm volatile("vtmv.v.t v8, %0\n"
                 "vtmv.v.t v9, %1\n"
                 "vtmv.v.t v10, %2\n"
                 "vtmv.v.t v11, %3"
                 :: "r"(selector), "r"(sel_col),
                    "r"(sel_next), "r"(sel_next_col)
                 : "v8", "v9", "v10", "v11", "memory");

    gr_set_compute_type();
    if (high)
      GR_FMA4_FIRST_HI_PREP(mt8, b0, next_offset, next_b0_addr);
    else
      GR_FMA4_FIRST_HI_PREP(mt0, b0, next_offset, next_b0_addr);
    if (batch + 1 != TE / 2)
      gr_preload_b0_16((const uint32_t *)next_b0_addr);
    gr_set_compute_type();
    const uint32_t *next_a;
    const uint32_t *next_b1;
    if (high)
      GR_FMA4_SECOND_LO_PREP(mt12, a, b1, next_offset, next_a, next_b1);
    else
      GR_FMA4_SECOND_LO_PREP(mt4, a, b1, next_offset, next_a, next_b1);
    if (batch + 1 != TE / 2)
      gr_preload_a_b1_half(next_a, next_b1, 0);

    float *out0 = previous + row * ldc;
    float *out1 = out0 + ldc;
    asm volatile("vsetvli zero, %0, e32, m2, ta, ma\n"
                 RELU_ASM("v8") RELU_ASM("v10")
                 "vse32.v v8, (%1)\nvse32.v v10, (%2)"
                 :: "r"((uintptr_t)(2 * TE)), "r"(out0), "r"(out1),
                    RELU_INPUTS
                 : "v8", "v9", "v10", "v11", "memory");
    gr_set_compute_type();
    if (high)
      GR_FMA4_SECOND_HI(mt12);
    else
      GR_FMA4_SECOND_HI(mt4);
    if (batch + 1 != TE / 2)
      gr_preload_a_b1_half(next_a, next_b1, 1);
  }
  gr_add_bias_pair(current, ldc, high);
}

static inline __attribute__((always_inline)) void gr_drain_pair(
    float *previous, unsigned ldc, unsigned high) {
  const uintptr_t base = high ? 0x40000000u : 0u;
#pragma clang loop unroll(disable)
  for (unsigned row = 0; row < TE; row += 2) {
    gr_set_drain_type();
    const uintptr_t selector = base + row;
    float *out0 = previous + row * ldc;
    float *out1 = out0 + ldc;
    asm volatile("vtmv.v.t v8, %0\n"
                 "vtmv.v.t v9, %1\n"
                 "vtmv.v.t v10, %2\n"
                 "vtmv.v.t v11, %3\n"
                 "vsetvli zero, %4, e32, m2, ta, ma\n"
                 RELU_ASM("v8")
                 RELU_ASM("v10")
                 "vse32.v v8, (%5)\n"
                 "vse32.v v10, (%6)"
                 :: "r"(selector), "r"(selector + 0x20000000u),
                    "r"(selector + 1),
                    "r"(selector + 0x20000001u),
                    "r"((uintptr_t)(2 * TE)), "r"(out0), "r"(out1),
                    RELU_INPUTS
                 : "v8", "v9", "v10", "v11", "memory");
  }
}

static inline __attribute__((always_inline)) void
gemm_relu_staggered_m192_n64_k128(const __fp16 *Atpack,
                                  const __fp16 *Bpack, float *C) {
  const uint32_t *a_words = (const uint32_t *)Atpack;
  const uint32_t *b_words = (const uint32_t *)Bpack;
  const unsigned tile_words = GR_TILE_WORDS_K128;
  const unsigned groups = 8;

#if BENCHMARK
  unsigned phase = gg_get_cycle();
#endif
  gr_compute_pair16(a_words, b_words, b_words + tile_words, 0, groups);
  gr_add_bias_pair(C, 64, 0);
#if BENCHMARK
  unsigned now = gg_get_cycle();
  gg_setup_cycles += now - phase;
  phase = now;
#endif

  float *previous = C;
  unsigned previous_high = 0;
  unsigned first = 1;
#pragma clang loop unroll(disable)
  for (unsigned mb64 = 0; mb64 != 192; mb64 += 64) {
    const uint32_t *a_block = a_words + (mb64 / TE) * tile_words;
#pragma clang loop unroll(disable)
    for (unsigned nb = 0; nb != 64; nb += 2 * TE) {
      const uint32_t *b0 = b_words + (nb / TE) * tile_words;
      const uint32_t *b1 = b0 + tile_words;
      const uint32_t *a = a_block;
      float *current = C + mb64 * 64 + nb;
#pragma clang loop unroll(disable)
      for (unsigned row = 0; row != 64; row += TE) {
        if (first) {
          first = 0;
        } else {
          const unsigned compute_high = previous_high ^ 1u;
          gr_compute_drain_pair(a, b0, b1, previous, current, 64,
                                compute_high, groups);
          previous = current;
          previous_high = compute_high;
        }
        a += tile_words;
        current += TE * 64;
      }
    }
  }
#if BENCHMARK
  now = gg_get_cycle();
  gg_compute_cycles += now - phase;
  phase = now;
#endif
  gr_drain_pair(previous, 64, previous_high);
#if BENCHMARK
  now = gg_get_cycle();
  gg_drain_cycles += now - phase;
#endif
  asm volatile("lw zero, 0(%0)" :: "r"(C) : "memory");
}

static inline __attribute__((always_inline)) void gemm_relu_staggered_m64_k64(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C) {
  const uint32_t *a_words = (const uint32_t *)Atpack;
  const uint32_t *b0 = (const uint32_t *)Bpack;
  const uint32_t *b1 = b0 + GR_TILE_WORDS_K64;
  const uint32_t *b2 = b1 + GR_TILE_WORDS_K64;
  const uint32_t *b3 = b2 + GR_TILE_WORDS_K64;

#if BENCHMARK
  unsigned phase = gg_get_cycle();
#endif
  gr_compute_pair(a_words, b0, b1, 0, 8);
#if BENCHMARK
  unsigned now = gg_get_cycle();
  gg_setup_cycles += now - phase;
  phase = now;
#endif
  for (unsigned mb = 0; mb < 64; mb += TE) {
    const uint32_t *a = a_words + (mb / TE) * GR_TILE_WORDS_K64;
    if (mb != 0)
      gr_compute_drain_pair_k64(a, b0, b1,
                                C + (mb - TE) * 64 + 2 * TE, 0);
    gr_compute_drain_pair_k64(a, b2, b3, C + mb * 64, 1);
  }
#if BENCHMARK
  now = gg_get_cycle();
  gg_compute_cycles += now - phase;
  phase = now;
#endif
  gr_drain_pair(C + (64 - TE) * 64 + 2 * TE, 64, 1);
#if BENCHMARK
  now = gg_get_cycle();
  gg_drain_cycles += now - phase;
#endif
  asm volatile("lw zero, 0(%0)" :: "r"(C) : "memory");
}

static inline __attribute__((always_inline)) void gemm_relu_body(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K, uint32_t fuse_relu) {
  gather_gemm_1x4((const uint32_t *)Atpack, (const uint32_t *)Bpack, C,
                  M, N, K, fuse_relu);
}


__attribute__((noinline, aligned(64))) void gemm_relu_fused(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K) {
#if BENCHMARK
  gg_setup_cycles = 0;
  gg_compute_cycles = 0;
  gg_drain_cycles = 0;
#endif
  (void)M;
  (void)N;
  (void)K;
  gemm_relu_staggered_m192_n64_k128(Atpack, Bpack, C);
}

#if BENCHMARK
void gemm_relu_get_counters(gemm_relu_counters_t *counters) {
  counters->setup = gg_setup_cycles;
  counters->compute = gg_compute_cycles;
  counters->drain = gg_drain_cycles;
}
#endif

__attribute__((always_inline, aligned(64))) void gemm_relu_fused_m64_k64(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C) {
#if BENCHMARK
  gg_setup_cycles = 0;
  gg_compute_cycles = 0;
  gg_drain_cycles = 0;
#endif
#if defined(GEMM_RELU_M64_K64)
  gemm_relu_body(Atpack, Bpack, C, 64, 64, 64, 1);
#else
  gemm_relu_staggered_m64_k64(Atpack, Bpack, C);
#endif
}
