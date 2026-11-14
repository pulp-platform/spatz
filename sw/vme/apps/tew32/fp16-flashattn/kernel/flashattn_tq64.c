// SPDX-License-Identifier: Apache-2.0
#include "ope_runtime.h"
#define FLASHATTN_IMPLEMENTATION
#define FLASHATTN_TQ64_IMPLEMENTATION
#include "flashattn.h"
#define PG_GEMM_ATTRIBUTES static __attribute__((always_inline)) inline
#define PG_USE_VTLE_SEED 1
#include "pipeline_gemm.h"
#include "kernel/qk_matmul_64x64x64.c"

static uint32_t *qw, *kw, *vw, *pw;
static float *st, *ot, *mrow, *lrow;
#if BENCHMARK
static unsigned stage_qk, stage_softmax, stage_pv, stage_normalize;
static unsigned stage_qk_each[NT], stage_softmax_each[NT], stage_pv_each[NT];
static unsigned stage_premax_each[NT], stage_exp_pack_each[NT],
    stage_rescale_each[NT];
static unsigned stage_pv_tile[4], stage_pv_block_tile[4][NT];
#endif

// l: the softmax normalization factor for each query row

// Scan all four resident 64-key score blocks once.  A single global row max
// lets every probability block feed the same output accumulator directly;
// no online output rescale or four-output merge is then required.
static __attribute__((always_inline)) inline void flash_global_max4(void) {
  
  const unsigned vl = vsetvl_e32_m4(TQ);

  // set init max to -inf for all four blocks
  asm volatile("vfmv.v.f v16, %0\n  vfmv.v.f v20, %0\n"
               "vfmv.v.f v24, %0\n  vfmv.v.f v28, %0"
               :: "f"(-1e30f) : "v16", "v20", "v24", "v28");

  // load each score block and update the four maxes in parallel
  for (unsigned r = 0; r < NT * BC; r += 4)
    asm volatile("vle32.v v0, (%0)\n    vle32.v v4, (%1)\n"
                 "vfmax.vv v16, v16, v0\n  vle32.v v8, (%2)\n"
                 "vfmax.vv v20, v20, v4\n  vle32.v v12, (%3)\n"
                 "vfmax.vv v24, v24, v8\n  vfmax.vv v28, v28, v12"
                 :: "r"(st + r * TQ),
                    "r"(st + (r + 1) * TQ),
                    "r"(st + (r + 2) * TQ),
                    "r"(st + (r + 3) * TQ)
                 : "v0", "v4", "v8", "v12", "memory");

  // reduce the four maxes to one and store it
  asm volatile("vfmax.vv v16, v16, v20\n"
               "vfmax.vv v24, v24, v28\n"
               "vfmax.vv v16, v16, v24\n"
               "vse32.v v16, (%0)"
               :: "r"(mrow) : "v16", "v20", "v24", "v28", "memory");
  (void)vl;
}

// Exponentiate one score block against the already-computed global max,
// accumulate its denominator contribution, and produce packed FP16 P.
static __attribute__((always_inline)) inline void flash_exp_pack_global(
    float *score, uint32_t *packed, int first) {

  const unsigned vl = vsetvl_e32_m4(TQ);

  // set init exp to 0 for all four blocks
  asm volatile("vfmv.v.f v28, %0" :: "f"(0.0f) : "v28");

  // load the global max and subtract it from each score row
  for (unsigned r = 0; r < BC; r += 4) {
    asm volatile("vle32.v v28, (%0)\n"
                 "vle32.v v0, (%1)\n  vle32.v v4, (%2)\n"
                 "vle32.v v8, (%3)\n  vle32.v v12, (%4)\n"
                 "vfsub.vv v0, v0, v28\n  vfsub.vv v4, v4, v28\n"
                 "vfsub.vv v8, v8, v28\n  vfsub.vv v12, v12, v28"
                 :: "r"(mrow), 
                    "r"(score + r * TQ),
                    "r"(score + (r + 1) * TQ),
                    "r"(score + (r + 2) * TQ),
                    "r"(score + (r + 3) * TQ)
                 : "v0", "v4", "v8", "v12", "v28", "memory");

    // The 5-degree polynomial and six repeated squarings
    asm volatile(SOFTMAX_EXP_UNROLL4("v0", "v4", "v8", "v12",     // inputs
                                     "v16", "v20", "v24", "v28")  // temps
                 :: EXP_INPUTS : "memory");

    // accumulate the denominator (l) and store it
    asm volatile("vfadd.vv v24, v0, v4\n"   
                 "vfadd.vv v28, v8, v12\n"
                 "vfadd.vv v24, v24, v28\n"
                 "vse32.v v24, (%0)"
                 :: "r"(score + r * TQ)
                 : "v24", "v28", "memory");

    // pack the FP16 Softmax output into the P layout
    asm volatile("vsetvli zero, %0, e16, m2, ta, ma\n"
                 "vfncvt.f.f.w v16, v0\n    vfncvt.f.f.w v18, v4\n"
                 "vfncvt.f.f.w v20, v8\n    vfncvt.f.f.w v22, v12\n"
                 "vsse16.v v16, (%1), %5\n  vsse16.v v18, (%2), %5\n"
                 "vsse16.v v20, (%3), %5\n  vsse16.v v22, (%4), %5\n"
                 "vsetvli zero, %0, e32, m4, ta, ma"
                 :: "r"((uintptr_t)vl),
                    "r"((uint16_t *)(packed + (r >> 1) * TQ) + (r & 1)),
                    "r"((uint16_t *)(packed + ((r + 1) >> 1) * TQ) + ((r + 1) & 1)),
                    "r"((uint16_t *)(packed + ((r + 2) >> 1) * TQ) + ((r + 2) & 1)),
                    "r"((uint16_t *)(packed + ((r + 3) >> 1) * TQ) + ((r + 3) & 1)),
                    "r"((uintptr_t)4)
                 : "v16", "v18", "v20", "v22", "memory");
  }

  // initialize the l' to 0 for the next block
  asm volatile("vmv.v.i v0, 0\n  vmv.v.i v4, 0\n"
               "vmv.v.i v8, 0\n  vmv.v.i v12, 0"
               ::: "v0", "v4", "v8", "v12");

  // accumulate l contributions from all four rows
  for (unsigned r = 0; r < BC; r += 16)
    asm volatile("vle32.v v16, (%0)\n  vle32.v v20, (%1)\n"
                 "vle32.v v24, (%2)\n  vle32.v v28, (%3)\n"
                 "vfadd.vv v0, v0, v16\n  vfadd.vv v4, v4, v20\n"
                 "vfadd.vv v8, v8, v24\n  vfadd.vv v12, v12, v28"
                 :: "r"(score + r * TQ), 
                    "r"(score + (r + 4) * TQ),
                    "r"(score + (r + 8) * TQ),
                    "r"(score + (r + 12) * TQ)
                 : "v0", "v4", "v8", "v12", "v16", "v20", "v24", "v28", "memory");
  if (first)
    asm volatile("vfadd.vv v0, v0, v4\n  vfadd.vv v8, v8, v12\n"
                 "vfadd.vv v0, v0, v8\n  vse32.v v0, (%0)"
                 :: "r"(lrow) 
                 : "v0", "v4", "v8", "v12", "memory");
  else
    asm volatile("vfadd.vv v0, v0, v4\n  vfadd.vv v8, v8, v12\n"
                 "vfadd.vv v0, v0, v8\n  vle32.v v16, (%0)\n"
                 "vfadd.vv v0, v0, v16\n  vse32.v v0, (%0)"
                 :: "r"(lrow) 
                 : "v0", "v4", "v8", "v12", "v16", "memory");
}

// Accumulate P_b*V_b for all four key blocks.
// each 32x32 output panel is resident in mt0/mt4/mt8/mt12.

// Before: block-major caller stores and reloads the complete accumulator three times.
// Here:   panel-major ordering needs only one tile zero and one final tile store.

static __attribute__((always_inline)) inline void flash_pv_keyblocks4(void) {
  asm volatile("vsetivli zero, 16, e32, m1, ta, ma" ::: "memory");
  unsigned tile = 0;
  for (unsigned i = 0; i < DV_SIZE; i += 32) {
    for (unsigned j = 0; j < TQ; j += 32, ++tile) {
#if BENCHMARK
      unsigned tile_start = get_cycle();
#endif
      asm volatile("msetmtype %0, %1\n  msettn zero, %2\n  msettm zero, %2"
                   :: "r"(16450u), "r"(0xc8u), "r"(16u) : "memory");
      asm volatile("vtzero mt0\n  vtzero mt4\n  vtzero mt8\n  vtzero mt12"
                   ::: "memory");
      const uint32_t *a = vw + i;
      const uint32_t *b = pw + j;
#pragma clang loop unroll(disable)
      for (unsigned block = 0; block < NT; ++block) {
#if BENCHMARK
        unsigned block_start = get_cycle();
#endif
        PG_LOAD(v0, a);
        PG_LOAD(v2, a + 16);
        PG_LOAD(v16, b);
        PG_LOAD(v18, b + 16);
        // Advance the shared row bases instead of rebuilding indexed addresses.
        const uint32_t *const end = a + (BC / 2 - 2) * DV_SIZE;
#pragma clang loop unroll(disable)
        while (a != end) {
          a += DV_SIZE;
          b += TQ;
          PG_MAC(mt0, v0, v16); PG_LOAD(v4, a);
          PG_MAC(mt4, v0, v18); PG_LOAD(v6, a + 16);
          PG_MAC(mt8, v2, v16); PG_LOAD(v20, b);
          PG_MAC(mt12, v2, v18); PG_LOAD(v22, b + 16);
          a += DV_SIZE;
          b += TQ;
          PG_MAC(mt0, v4, v20); PG_LOAD(v0, a);
          PG_MAC(mt4, v4, v22); PG_LOAD(v2, a + 16);
          PG_MAC(mt8, v6, v20); PG_LOAD(v16, b);
          PG_MAC(mt12, v6, v22); PG_LOAD(v18, b + 16);
        }
        a += DV_SIZE;
        b += TQ;
        PG_MAC(mt0, v0, v16); PG_LOAD(v4, a);
        PG_MAC(mt4, v0, v18); PG_LOAD(v6, a + 16);
        PG_MAC(mt8, v2, v16); PG_LOAD(v20, b);
        PG_MAC(mt12, v2, v18); PG_LOAD(v22, b + 16);
        PG_MAC(mt0, v4, v20); PG_MAC(mt4, v4, v22);
        PG_MAC(mt8, v6, v20); PG_MAC(mt12, v6, v22);
        a += DV_SIZE;
        b += TQ;
#if BENCHMARK
        unsigned block_cycles = get_cycle() - block_start;
        stage_pv_each[block] += block_cycles;
        stage_pv_block_tile[tile][block] = block_cycles;
#endif
      }
      PG_CFG32();
      float *dst0 = ot + i * TQ + j;
      float *dst1 = dst0 + 16;
      float *dst2 = dst0 + 16 * TQ;
      float *dst3 = dst2 + 16;
      uintptr_t src_tss0 = 0;
      uintptr_t src_tss1 = 0x20000000u;
      uintptr_t src_tss2 = 0x40000000u;
      uintptr_t src_tss3 = 0x60000000u;
      unsigned rows = 16;
      asm volatile(
          "1:\n"
          "vtse32 %0, (%4)\n  vtse32 %1, (%5)\n"
          "vtse32 %2, (%6)\n  vtse32 %3, (%7)\n"
          "addi %0, %0, 1\n  addi %1, %1, 1\n"
          "addi %2, %2, 1\n  addi %3, %3, 1\n"
          "addi %4, %4, 256\n  addi %5, %5, 256\n"
          "addi %6, %6, 256\n  addi %7, %7, 256\n"
          "addi %8, %8, -1\n  bnez %8, 1b"
          : "+r"(src_tss0), "+r"(src_tss1), "+r"(src_tss2), "+r"(src_tss3),
            "+r"(dst0), "+r"(dst1), "+r"(dst2), "+r"(dst3),
            "+r"(rows)
          :: "memory");
#if BENCHMARK
      stage_pv_tile[tile] = get_cycle() - tile_start;
#endif
    }
  }
  asm volatile("lw zero, 0(%0)" :: "r"(ot) : "memory");
}

__attribute__((noinline)) void flashattn_tq64_run(void) {
#if BENCHMARK
  stage_qk = stage_softmax = stage_pv = stage_normalize = 0;
  flash_premax_cycles = flash_exp_pack_cycles = flash_rescale_cycles = 0;
  for (unsigned block = 0; block < NT; ++block) {
    stage_qk_each[block] = stage_softmax_each[block] = 0;
    stage_pv_each[block] = stage_premax_each[block] = 0;
    stage_exp_pack_each[block] = stage_rescale_each[block] = 0;
  }
  for (unsigned tile = 0; tile < 4; ++tile) {
    stage_pv_tile[tile] = 0;
    for (unsigned block = 0; block < NT; ++block)
      stage_pv_block_tile[tile][block] = 0;
  }
#endif
  // Phase 1: make all four score blocks resident.  Phase 2 computes one row
  // maximum across the full 256 keys.  Phase 3 consumes each score block,
  // immediately packs P and accumulates P*V into the one shared output.
#pragma clang loop unroll(disable)
  for (unsigned t = 0; t < NT; ++t) {
#if BENCHMARK
    unsigned s = get_cycle();
#endif
    qk_matmul_64x64x64((const __fp16 *)(kw + t * (D_SIZE / 2) * BC),
                       (const __fp16 *)qw, st + t * BC * TQ);
#if BENCHMARK
    stage_qk_each[t] = get_cycle() - s;
    stage_qk += stage_qk_each[t];
#endif
  }
#if BENCHMARK
  unsigned s = get_cycle();
#endif
  flash_global_max4();
#if BENCHMARK
  unsigned global_max_cycles = get_cycle() - s;
  flash_premax_cycles += global_max_cycles;
#endif
  for (unsigned t = 0; t < NT; ++t) {
#if BENCHMARK
    s = get_cycle();
#endif
    flash_exp_pack_global(st + t * BC * TQ, pw + t * (BC / 2) * TQ, t == 0);
#if BENCHMARK
    unsigned exp_cycles = get_cycle() - s;
    stage_softmax_each[t] = exp_cycles + (t == 0 ? global_max_cycles : 0);
    stage_premax_each[t] = t == 0 ? global_max_cycles : 0;
    stage_exp_pack_each[t] = exp_cycles;
    stage_softmax += stage_softmax_each[t];
    flash_exp_pack_cycles += exp_cycles;
#endif
  }
#if BENCHMARK
  s = get_cycle();
#endif
  flash_pv_keyblocks4();
#if BENCHMARK
  stage_pv = get_cycle() - s;
#endif
#if BENCHMARK
  s = get_cycle();
#endif
  flash_normalize(ot, lrow);
#if BENCHMARK
  stage_normalize = get_cycle() - s;
#endif
}

void flashattn_tq64_configure(const flashattn_tq64_buffers_t *buffers) {
  qw = buffers->q;
  kw = buffers->k;
  vw = buffers->v;
  st = buffers->scores;
  pw = buffers->probabilities;
  ot = buffers->output;
  mrow = buffers->row_max;
  lrow = buffers->row_sum;
}

void flashattn_tq64_get_counters(flashattn_tq64_counters_t *counters) {
#if BENCHMARK
  counters->qk = stage_qk;
  counters->softmax = stage_softmax;
  counters->pv = stage_pv;
  counters->normalize = stage_normalize;
  counters->premax = flash_premax_cycles;
  counters->exp_pack = flash_exp_pack_cycles;
  counters->rescale = flash_rescale_cycles;
  for (unsigned block = 0; block < NT; ++block) {
    counters->qk_block[block] = stage_qk_each[block];
    counters->softmax_block[block] = stage_softmax_each[block];
    counters->pv_block[block] = stage_pv_each[block];
    counters->premax_block[block] = stage_premax_each[block];
    counters->exp_pack_block[block] = stage_exp_pack_each[block];
    counters->rescale_block[block] = stage_rescale_each[block];
  }
  for (unsigned tile = 0; tile < 4; ++tile) {
    counters->pv_tile[tile] = stage_pv_tile[tile];
    for (unsigned block = 0; block < NT; ++block)
      counters->pv_block_tile[tile][block] =
          stage_pv_block_tile[tile][block];
  }
#else
  (void)counters;
#endif
}
