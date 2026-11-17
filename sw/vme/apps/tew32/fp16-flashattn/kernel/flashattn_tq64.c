// SPDX-License-Identifier: Apache-2.0
#include "runtime.h"
#define FLASHATTN_IMPLEMENTATION
#define FLASHATTN_TQ64_IMPLEMENTATION
#include "flashattn.h"
#define PG_GEMM_ATTRIBUTES static __attribute__((always_inline)) inline
#define PG_USE_VTLE_SEED 1
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

  asm volatile("vfmv.v.f v16, %0\n  vfmv.v.f v20, %0\n"
               "vfmv.v.f v24, %0\n  vfmv.v.f v28, %0"
               :: "f"(-1e30f) : "v16", "v20", "v24", "v28");

  uintptr_t p0 = (uintptr_t)st;
  uintptr_t p1 = (uintptr_t)(st + TQ);
  uintptr_t p2 = (uintptr_t)(st + 2 * TQ);
  uintptr_t p3 = (uintptr_t)(st + 3 * TQ);
#pragma clang loop unroll(disable)
  for (unsigned r = 0; r < NT * BC; r += 4)
    asm volatile("vle32.v v0, (%0)\n  vle32.v v4, (%1)\n"
                 "addi %0, %0, %4\n  addi %1, %1, %4\n"
                 "vle32.v v8, (%2)\n  vfmax.vv v16, v16, v0\n"
                 "vle32.v v12, (%3)\n  vfmax.vv v20, v20, v4\n"
                 "addi %2, %2, %4\n  addi %3, %3, %4\n"
                 "vfmax.vv v24, v24, v8\n  vfmax.vv v28, v28, v12"
                 : "+r"(p0), "+r"(p1), "+r"(p2), "+r"(p3)
                 : "i"(4 * TQ * sizeof(float))
                 : "v0", "v4", "v8", "v12", "memory");

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
    float *score, uint32_t *packed, const float *max_base,
    float *sum_base, int first) {

  const unsigned vl = vsetvl_e32_m4(TQ);

  // Prime the first quartet; subsequent quartets are loaded while the
  // previous quartet's packed rows are being stored.
  asm volatile("vle32.v v28, (%0)\n"
               "vle32.v v0, (%1)\n  vle32.v v4, (%2)\n"
               "vle32.v v8, (%3)\n  vle32.v v12, (%4)\n"
               "vfsub.vv v0, v0, v28\n  vfsub.vv v4, v4, v28\n"
               "vfsub.vv v8, v8, v28\n  vfsub.vv v12, v12, v28"
               :: "r"(max_base), "r"(score), "r"(score + TQ),
                  "r"(score + 2 * TQ), "r"(score + 3 * TQ)
               : "v0", "v4", "v8", "v12", "v28", "memory");

  uintptr_t next_score = (uintptr_t)(score + 4 * TQ);
  for (unsigned r = 0; r < BC; r += 4) {
    asm volatile(SOFTMAX_EXP_UNROLL4("v0", "v4", "v8", "v12",
                                     "v16", "v20", "v24", "v28")
                 :: EXP_INPUTS : "memory");

    asm volatile("vfadd.vv v24, v0, v4\n"
                 "vfadd.vv v28, v8, v12\n"
                 "vfadd.vv v24, v24, v28\n"
                 "vse32.v v24, (%0)"
                 :: "r"(score + r * TQ)
                 : "v24", "v28", "memory");

    uint16_t *const pack0 =
        (uint16_t *)(packed + (r >> 1) * TQ) + (r & 1);
    uint16_t *const pack1 =
        (uint16_t *)(packed + ((r + 1) >> 1) * TQ) + ((r + 1) & 1);
    uint16_t *const pack2 =
        (uint16_t *)(packed + ((r + 2) >> 1) * TQ) + ((r + 2) & 1);
    uint16_t *const pack3 =
        (uint16_t *)(packed + ((r + 3) >> 1) * TQ) + ((r + 3) & 1);
    asm volatile("vsetvli zero, %0, e16, m2, ta, ma\n"
                 "vfncvt.f.f.w v16, v0\n"
                 "vfncvt.f.f.w v18, v4\n"
                 "vsse16.v v16, (%1), %5\n"
                 "vfncvt.f.f.w v20, v8\n"
                 "vfncvt.f.f.w v22, v12\n"
                 "vsetvli zero, %0, e32, m4, ta, ma"
                 :: "r"((uintptr_t)vl),
                    "r"(pack0), "r"(pack1), "r"(pack2), "r"(pack3),
                    "r"((uintptr_t)4)
                 : "v16", "v18", "v20", "v22", "memory");

    if (r + 4 < BC) {
      const uintptr_t next1 = next_score + TQ * sizeof(float);
      const uintptr_t next2 = next_score + 2 * TQ * sizeof(float);
      const uintptr_t next3 = next_score + 3 * TQ * sizeof(float);
      asm volatile("vle32.v v28, (%[max])\n"
                   "vle32.v v0, (%[next0])\n"
                   "vsse16.v v18, (%[pack1]), %[stride]\n"
                   "vle32.v v4, (%[next1])\n"
                   "vsse16.v v20, (%[pack2]), %[stride]\n"
                   "vle32.v v8, (%[next2])\n"
                   "vfsub.vv v0, v0, v28\n"
                   "vle32.v v12, (%[next3])\n"
                   "vsse16.v v22, (%[pack3]), %[stride]\n"
                   "vfsub.vv v4, v4, v28\n"
                   "addi %[next0], %[next0], 1024\n"
                   "vfsub.vv v8, v8, v28\n"
                   "vfsub.vv v12, v12, v28"
                   : [next0] "+r"(next_score)
                   : [max] "r"(max_base), [next1] "r"(next1),
                     [next2] "r"(next2), [next3] "r"(next3),
                     [pack1] "r"(pack1), [pack2] "r"(pack2),
                     [pack3] "r"(pack3), [stride] "r"((uintptr_t)4)
                   : "v0", "v4", "v8", "v12", "v18", "v20", "v22", "v28",
                     "memory");
    } else {
      asm volatile("vsse16.v v18, (%0), %3\n"
                   "vmv.v.i v0, 0\n"
                   "vsse16.v v20, (%1), %3\n"
                   "vmv.v.i v4, 0\n"
                   "vsse16.v v22, (%2), %3\n"
                   "vmv.v.i v8, 0\n  vmv.v.i v12, 0"
                   :: "r"(pack1), "r"(pack2), "r"(pack3), "r"((uintptr_t)4)
                   : "v0", "v4", "v8", "v12", "v18", "v20", "v22", "memory");
    }
  }

#pragma clang loop unroll(disable)
  for (unsigned r = 0; r < BC; r += 16) {
    float *score_base = score + r * TQ;
    asm volatile("vle32.v v16, (%0)\n  vle32.v v20, (%1)\n"
                 "vfadd.vv v0, v0, v16\n"
                 "vle32.v v24, (%2)\n"
                 "vfadd.vv v4, v4, v20\n"
                 "vle32.v v28, (%3)\n"
                 "vfadd.vv v8, v8, v24\n  vfadd.vv v12, v12, v28"
                 :: "r"(score_base), "r"(score_base + 4 * TQ),
                    "r"(score_base + 8 * TQ),
                    "r"(score_base + 12 * TQ)
                 : "v0", "v4", "v8", "v12", "v16", "v20", "v24", "v28", "memory");
  }
  if (first)
    asm volatile("vfadd.vv v0, v0, v4\n  vfadd.vv v8, v8, v12\n"
                 "vfadd.vv v0, v0, v8\n  vse32.v v0, (%0)"
                 :: "r"(sum_base)
                 : "v0", "v4", "v8", "v12", "memory");
  else
    asm volatile("vfadd.vv v0, v0, v4\n  vfadd.vv v8, v8, v12\n"
                 "vfadd.vv v0, v0, v8\n  vle32.v v16, (%0)\n"
                 "vfadd.vv v0, v0, v16\n  vse32.v v0, (%0)"
                 :: "r"(sum_base)
                 : "v0", "v4", "v8", "v12", "v16", "memory");
}

// Materialize the reciprocal once before PV.  The tile drain consumes the
// four 16-query slices directly, avoiding a second pass over the 64x64 output.
static __attribute__((always_inline)) inline void flash_reciprocal_l(void) {
  vsetvl_e32_m4(TQ);
  asm volatile("vle32.v v8, (%0)\n" RECIP2_ASM("v8", "v12", "v16")
               "vse32.v v12, (%0)"
               :: "r"(lrow), RECIP_INPUTS
               : "v8", "v12", "v16", "memory");
}

// Accumulate P_b*V_b for all four key blocks.
// each 32x32 output panel is resident in mt0/mt4/mt8/mt12.

// Before: block-major caller stores and reloads the complete accumulator three times.
// Here:   panel-major ordering needs only one tile zero and one final tile store.

static __attribute__((always_inline)) inline void flash_pv_keyblocks4(void) {
  const uint32_t *const v_base = vw;
  const uint32_t *const p_base = pw;
  float *const out_base = ot;
  const float *const inv_l_base = lrow;
  asm volatile("vsetivli zero, 16, e32, m1, ta, ma" ::: "memory");
  unsigned tile = 0;
  for (unsigned i = 0; i < DV_SIZE; i += 32) {
    for (unsigned j = 0; j < TQ; j += 32, ++tile) {
#if BENCHMARK
      unsigned tile_start = get_cycle();
#endif
      asm volatile("msetmtype %0, %1\n  msettn zero, %2"
                   :: "r"(16450u), "r"(0xc8u), "r"(16u) : "memory");
      asm volatile("vtzero mt0\n  vtzero mt4\n  vtzero mt8\n  vtzero mt12"
                   ::: "memory");
      const uint32_t *a = v_base + i;
      const uint32_t *b = p_base + j;
#pragma clang loop unroll(disable)
      for (unsigned block = 0; block < NT; ++block) {
#if BENCHMARK
        unsigned block_start = get_cycle();
#endif
        PG_LOAD(v0, a);
        PG_LOAD(v2, a + 16);
        PG_LOAD(v16, b);
        PG_LOAD(v18, b + 16);
        const uint32_t *const end = a + (BC / 2 - 2) * DV_SIZE;
#pragma clang loop unroll(disable)
        while (a != end) {
          uintptr_t an, ah, bn, bh;
          PG_MAC(mt0, v0, v16);
          asm volatile("addi %0, %2, 256\naddi %1, %0, 64"
                       : "=&r"(an), "=&r"(ah) : "r"(a));
          PG_LOAD(v4, an);
          PG_MAC(mt4, v0, v18);
          asm volatile("addi %0, %2, 256\naddi %1, %0, 64"
                       : "=&r"(bn), "=&r"(bh) : "r"(b));
          PG_LOAD(v6, ah);
          PG_MAC(mt8, v2, v16);
          PG_LOAD(v20, bn);
          a = (const uint32_t *)an;
          PG_MAC(mt12, v2, v18);
          PG_LOAD(v22, bh);
          b = (const uint32_t *)bn;

          PG_MAC(mt0, v4, v20);
          asm volatile("addi %0, %2, 256\naddi %1, %0, 64"
                       : "=&r"(an), "=&r"(ah) : "r"(a));
          PG_LOAD(v0, an);
          PG_MAC(mt4, v4, v22);
          asm volatile("addi %0, %2, 256\naddi %1, %0, 64"
                       : "=&r"(bn), "=&r"(bh) : "r"(b));
          PG_LOAD(v2, ah);
          PG_MAC(mt8, v6, v20);
          PG_LOAD(v16, bn);
          a = (const uint32_t *)an;
          PG_MAC(mt12, v6, v22);
          PG_LOAD(v18, bh);
          b = (const uint32_t *)bn;
        }
        uintptr_t tail_a_hi, tail_b_hi;
        asm volatile("addi %0, %0, 256\n"
                     "addi %2, %0, 64\n"
                     "addi %1, %1, 256\n"
                     "addi %3, %1, 64\n"
                     "vtfmm.tvv mt0, v0, v16\n"
                     "vle32.v v4, (%0)\n"
                     "vle32.v v6, (%2)\n"
                     "vle32.v v20, (%1)\n"
                     "vtfmm.tvv mt4, v0, v18\n"
                     "vle32.v v22, (%3)\n"
                     "vtfmm.tvv mt8, v2, v16\n"
                     "vtfmm.tvv mt12, v2, v18\n"
                     "vtfmm.tvv mt0, v4, v20\n"
                     "addi %0, %0, 256\n"
                     "vtfmm.tvv mt4, v4, v22\n"
                     "addi %1, %1, 256\n"
                     "vtfmm.tvv mt8, v6, v20\n"
                     "vtfmm.tvv mt12, v6, v22"
                     : "+r"(a), "+r"(b), "=&r"(tail_a_hi), "=&r"(tail_b_hi)
                     :: "v4", "v6", "v20", "v22", "memory");
#if BENCHMARK
        unsigned block_cycles = get_cycle() - block_start;
        stage_pv_each[block] += block_cycles;
        stage_pv_block_tile[tile][block] = block_cycles;
#endif
      }
      asm volatile("msetmtype %0, %1\n  msettn zero, %2"
                   :: "r"(16417u), "r"(0xd0u), "r"(16u) : "memory");
      float *dst0 = out_base + i * TQ + j;
      float *dst1 = dst0 + 16;
      float *dst2 = dst0 + 16 * TQ;
      float *dst3 = dst2 + 16;
      uintptr_t src_tss0 = 0;
      uintptr_t src_tss1 = 0x20000000u;
      uintptr_t src_tss2 = 0x40000000u;
      uintptr_t src_tss3 = 0x60000000u;
      unsigned rows = 16;
      const float *scale0 = inv_l_base + j;
      const float *scale1 = scale0 + 16;
      asm volatile(
          "vle32.v v8, (%9)\n  vle32.v v9, (%10)\n"
          "1:\n"
          "vtmv.v.t v0, %0\n  addi %0, %0, 1\n"
          "vtmv.v.t v1, %1\n  addi %1, %1, 1\n"
          "vfmul.vv v0, v0, v8\n  vtmv.v.t v2, %2\n"
          "addi %2, %2, 1\n  vfmul.vv v1, v1, v9\n"
          "vse32.v v0, (%4)\n  addi %4, %4, 256\n"
          "vtmv.v.t v3, %3\n  addi %3, %3, 1\n"
          "vfmul.vv v2, v2, v8\n  vse32.v v1, (%5)\n"
          "addi %5, %5, 256\n  vfmul.vv v3, v3, v9\n"
          "vse32.v v2, (%6)\n  addi %6, %6, 256\n"
          "vse32.v v3, (%7)\n  addi %7, %7, 256\n"
          "addi %8, %8, -1\n  bnez %8, 1b"
          : "+r"(src_tss0), "+r"(src_tss1), "+r"(src_tss2), "+r"(src_tss3),
            "+r"(dst0), "+r"(dst1), "+r"(dst2), "+r"(dst3),
            "+r"(rows)
          : "r"(scale0), "r"(scale1)
          : "v0", "v1", "v2", "v3", "v8", "v9", "memory");
#if BENCHMARK
      stage_pv_tile[tile] = get_cycle() - tile_start;
#endif
    }
  }
  asm volatile("lw zero, 0(%0)" :: "r"(out_base) : "memory");
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
#if BENCHMARK
  unsigned s;
#endif
#pragma clang loop unroll(disable)
  for (unsigned t = 0; t < NT; ++t) {
#if BENCHMARK
    s = get_cycle();
#endif
    qk_matmul_64x64x64((const __fp16 *)(kw + t * (D_SIZE / 2) * BC),
                       (const __fp16 *)qw, st + t * BC * TQ);
#if BENCHMARK
    stage_qk_each[t] = get_cycle() - s;
    stage_qk += stage_qk_each[t];
#endif
  }
#if BENCHMARK
  s = get_cycle();
#endif
  flash_global_max4();
#if BENCHMARK
  unsigned global_max_cycles = get_cycle() - s;
  flash_premax_cycles += global_max_cycles;
#endif
  float *const score_base = st;
  uint32_t *const packed_base = pw;
  const float *const max_base = mrow;
  float *const sum_base = lrow;
#pragma clang loop unroll(disable)
  for (unsigned t = 0; t < NT; ++t) {
#if BENCHMARK
    s = get_cycle();
#endif
    flash_exp_pack_global(score_base + t * BC * TQ,
                          packed_base + t * (BC / 2) * TQ,
                          max_base, sum_base, t == 0);
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
  flash_reciprocal_l();
#if BENCHMARK
  stage_normalize = get_cycle() - s;
#endif
#if BENCHMARK
  s = get_cycle();
#endif
  flash_pv_keyblocks4();
#if BENCHMARK
  stage_pv = get_cycle() - s;
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
