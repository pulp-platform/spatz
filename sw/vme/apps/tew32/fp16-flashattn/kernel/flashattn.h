// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

// --------------------------------------------------------------------------
// Build configuration
// --------------------------------------------------------------------------

#ifndef BENCHMARK
#define BENCHMARK 0
#endif

// --------------------------------------------------------------------------
// TQ16 public API: TQ16, T64, D64, DV16
// --------------------------------------------------------------------------

void flashattn_tq16(const uint32_t *q, const uint32_t *k, const uint32_t *v,
                    uint32_t *pair_scratch, float *output);

// --------------------------------------------------------------------------
// TQ64 public constants and data types: TQ64, D64, DV64, BC64
// --------------------------------------------------------------------------

enum {
  FLASHATTN_MAX_BLOCKS = 4,
  FLASHATTN_PV_TILES = 4,
};

typedef struct {
  uint32_t *q;
  uint32_t *k;
  uint32_t *v;
  float *scores;
  uint32_t *probabilities;
  float *output;
  float *row_max;
  float *row_sum;
} flashattn_tq64_buffers_t;

// Optional per-stage counters, populated only by BENCHMARK builds.
typedef struct {
  unsigned qk, softmax, pv, normalize;
  unsigned premax, exp_pack, rescale;
  unsigned qk_block[FLASHATTN_MAX_BLOCKS];
  unsigned softmax_block[FLASHATTN_MAX_BLOCKS];
  unsigned pv_block[FLASHATTN_MAX_BLOCKS];
  unsigned premax_block[FLASHATTN_MAX_BLOCKS];
  unsigned exp_pack_block[FLASHATTN_MAX_BLOCKS];
  unsigned rescale_block[FLASHATTN_MAX_BLOCKS];
  unsigned pv_tile[FLASHATTN_PV_TILES];
  unsigned pv_block_tile[FLASHATTN_PV_TILES][FLASHATTN_MAX_BLOCKS];
} flashattn_tq64_counters_t;

// --------------------------------------------------------------------------
// TQ64 public API
// --------------------------------------------------------------------------

// Configure outside the measured region so pointer setup does not perturb the
// kernel cycle count. All buffers must reside in L1 and remain valid until the
// run completes.
void flashattn_tq64_configure(const flashattn_tq64_buffers_t *buffers);
void flashattn_tq64_run(void);
void flashattn_tq64_get_counters(flashattn_tq64_counters_t *counters);

// --------------------------------------------------------------------------
// Shared private implementation: exponential and reciprocal primitives
// --------------------------------------------------------------------------

#ifdef FLASHATTN_IMPLEMENTATION
#include "ope_runtime.h"
#include "ope_transformer.h"

// FlashAttention only needs the non-positive-input exponential variants from
// ope_softmax.h. Keep them local so this fixed kernel does not pull in the
// generic softmax implementations.
#define SOFTMAX_EXP_ASM(x, t) \
  "vfmax.vf " x ", " x ", %[ope_lo]\n" \
  "vfmul.vf " x ", " x ", %[ope_scale]\n" \
  "vfmv.v.f " t ", %[ope_c5]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[ope_c4]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[ope_c3]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[ope_c2]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[ope_one]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[ope_one]\n" \
  "vfmul.vv " x ", " t ", " t "\n" \
  "vfmul.vv " x ", " x ", " x "\n" \
  "vfmul.vv " x ", " x ", " x "\n" \
  "vfmul.vv " x ", " x ", " x "\n" \
  "vfmul.vv " x ", " x ", " x "\n" \
  "vfmul.vv " x ", " x ", " x "\n"

#define SOFTMAX_EXP_UNROLL4(x0, x1, x2, x3, t0, t1, t2, t3) \
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
  "vfmul.vv " x3 ", " x3 ", " x3 "\n"
#endif  // FLASHATTN_IMPLEMENTATION

// --------------------------------------------------------------------------
// TQ64 private implementation: counters and output normalization
// --------------------------------------------------------------------------

#ifdef FLASHATTN_TQ64_IMPLEMENTATION

#if BENCHMARK
static unsigned flash_premax_cycles, flash_exp_pack_cycles, flash_rescale_cycles;
#endif  // BENCHMARK

static __attribute__((always_inline)) inline void flash_normalize(float *ot,
                                                                  float *lrow) {
  vsetvl_e32_m4(TQ);
  asm volatile("vle32.v v8, (%0)" :: "r"(lrow) : "v8", "memory");
#if defined(FLASH_RECIP2) && FLASH_RECIP2
  asm volatile(RECIP2_ASM("v8", "v12", "v16")
               :: RECIP_INPUTS : "memory");
#else
  RECIP("v8", "v12", "v16");
#endif  // FLASH_RECIP2
  asm volatile("vmv.v.v v16, v12\nvmv.v.v v20, v12\n"
               "vsetvli zero, %0, e32, m8, ta, ma"
               :: "r"((uintptr_t)(2 * TQ)) : "v16", "v20", "memory");
  unsigned j = 0;
  for (; j + 5 < DV_SIZE; j += 6)
    asm volatile("vle32.v v0, (%0)\n"
                 "vle32.v v8, (%1)\n"
                 "vle32.v v24, (%2)\n"
                 "vfmul.vv v0, v0, v16\n"
                 "vfmul.vv v8, v8, v16\n"
                 "vfmul.vv v24, v24, v16\n"
                 "vse32.v v0, (%0)\n"
                 "vse32.v v8, (%1)\n"
                 "vse32.v v24, (%2)"
                 :: "r"(ot + j * TQ), "r"(ot + (j + 2) * TQ),
                    "r"(ot + (j + 4) * TQ)
                 : "v0", "v8", "v24", "memory");
  for (; j < DV_SIZE; j += 2)
    asm volatile("vle32.v v0, (%0)\nvfmul.vv v0, v0, v16\n"
                 "vse32.v v0, (%0)"
                 :: "r"(ot + j * TQ) : "v0", "memory");
}
#endif  // FLASHATTN_TQ64_IMPLEMENTATION
