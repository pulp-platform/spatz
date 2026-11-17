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
};

void gemm_gelu_fused(const __fp16 *Atpack, const __fp16 *Bpack, float *C,
                     uint32_t M, uint32_t N, uint32_t K);
void gemm_gelu_non_fused(const __fp16 *Atpack, const __fp16 *Bpack, float *C,
                         uint32_t M, uint32_t N, uint32_t K);
void gelu_non_fused(float *C, uint32_t elements);

#include "runtime.h"

#ifndef GG_BULK_B_TILES
#define GG_BULK_B_TILES 0
#endif

#ifndef GG_OPE_BIAS
#define GG_OPE_BIAS 0
#endif

#ifndef BENCHMARK
#define BENCHMARK 0
#endif
#if BENCHMARK
static unsigned gg_setup_cycles, gg_compute_cycles, gg_drain_cycles;
#endif

#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
// An application that enables interleaved gather must provide this helper.
// The prototype lets the first N64 GEMM panel finish producing its tail range
// before the remaining bags are materialized.
static __attribute__((always_inline)) inline void gather_pool_range(
    unsigned begin, unsigned count);
#endif

#define GG_VTFMM8(mt) do { \
  asm volatile("vtfmm.tvv " #mt ", v0, v16" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v1, v17" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v2, v18" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v3, v19" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v4, v20" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v5, v21" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v6, v22" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v7, v23" ::: "memory"); \
} while (0)

#define GG_VTFMM8_B24(mt) do { \
  asm volatile("vtfmm.tvv " #mt ", v0, v24" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v1, v25" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v2, v26" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v3, v27" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v4, v28" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v5, v29" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v6, v30" ::: "memory"); \
  asm volatile("vtfmm.tvv " #mt ", v7, v31" ::: "memory"); \
} while (0)

#define GG_FMA4_LO16(mt) do { \
  asm volatile("vtfmm.tvv " #mt ",v0,v16":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v1,v17":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v2,v18":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v3,v19":::"memory"); \
} while (0)
#define GG_FMA4_HI16(mt) do { \
  asm volatile("vtfmm.tvv " #mt ",v4,v20":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v5,v21":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v6,v22":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v7,v23":::"memory"); \
} while (0)
#define GG_FMA4_LO24(mt) do { \
  asm volatile("vtfmm.tvv " #mt ",v0,v24":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v1,v25":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v2,v26":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v3,v27":::"memory"); \
} while (0)
#define GG_FMA4_HI24(mt) do { \
  asm volatile("vtfmm.tvv " #mt ",v4,v28":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v5,v29":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v6,v30":::"memory"); \
  asm volatile("vtfmm.tvv " #mt ",v7,v31":::"memory"); \
} while (0)

#define GG_GATHER_INIT(p_) do { const uint16_t *g_=(p_); \
  /* v8..v15 are eight independent scratch vectors.  The surrounding GEMM
   * normally leaves RVV at e16,m8 for its bulk operand loads, where v8, v9,
   * ... denote overlapping register groups.  Reconfigure before using these
   * names individually; otherwise the gather corrupts the operand state and
   * can propagate unknown values into the checker. */ \
  asm volatile("vsetivli zero,16,e16,m1,ta,ma\n" \
               "vle16.v v8,(%0)\nvle16.v v9,(%1)\n" \
               "vle16.v v10,(%2)\nvle16.v v11,(%3)" \
               ::"r"(g_),"r"(g_+16),"r"(g_+32),"r"(g_+48):"memory"); \
} while (0)
#define GG_GATHER_ADD(p_) do { const uint16_t *g_=(p_); \
  asm volatile("vle16.v v12,(%0)\nvle16.v v13,(%1)\n" \
               "vle16.v v14,(%2)\nvle16.v v15,(%3)\n" \
               "vfadd.vv v8,v8,v12\nvfadd.vv v9,v9,v13\n" \
               "vfadd.vv v10,v10,v14\nvfadd.vv v11,v11,v15" \
               ::"r"(g_),"r"(g_+16),"r"(g_+32),"r"(g_+48):"memory"); \
} while (0)

#define GG_EXTRACT_ROW(v0_, v1_, v2_, v3_, row_) do { \
  asm volatile("vtmv.v.t " #v0_ ", %0\n" \
               "vtmv.v.t " #v1_ ", %1\n" \
               "vtmv.v.t " #v2_ ", %2\n" \
               "vtmv.v.t " #v3_ ", %3" \
               :: "r"((uintptr_t)(row_)), \
                  "r"((uintptr_t)0x20000000u + (row_)), \
                  "r"((uintptr_t)0x40000000u + (row_)), \
                  "r"((uintptr_t)0x60000000u + (row_)) \
               : #v0_, #v1_, #v2_, #v3_, "memory"); \
} while (0)

// Fixed-layout postprocessing GEMM used by the matched gather workloads.
// A: [M/16][K/2][16], B: [N/16][K/2][16], high half = even K.
// One accumulator panel is M16 x N64 (1x4); C remains row-major with ldc=N.
static __attribute__((always_inline)) inline void gather_gemm_1x4(
    const uint32_t *a, const uint32_t *b, float *c,
    unsigned m, unsigned n, unsigned k, unsigned post_op) {
  const uintptr_t matrix_mtype = (16u << 10) | (2u << 5) | 2u;
  const uintptr_t matrix_vtype = 0xc8u;
  const uintptr_t store_mtype = (16u << 10) | (1u << 5) | 1u;
  const uintptr_t store_vtype = 0xd0u;
  const unsigned pair_rows = k / 2;
  const unsigned tile_words = pair_rows * 16;
#if !GG_OPE_BIAS
  unsigned panel_preloaded = 0;
#endif

  for (unsigned nb = 0; nb < n; nb += 64) {
#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
    if (nb != 0) {
      // The first N64 panel generated GG_INTERLEAVE_COUNT tail bags in its
      // otherwise idle v8..v15 scratch bank.  Keep the remaining tail in the
      // canonical packed layout and fence all vector stores before it is read.
      gather_pool_range(GG_INTERLEAVE_BEGIN + GG_INTERLEAVE_COUNT,
                        B_SIZE / 2 - GG_INTERLEAVE_COUNT);
      wait_for_vector_store(pool);
    }
#endif
#if GG_OPE_BIAS
    const uint32_t *first_bp = b + (nb / 16) * tile_words;
    asm volatile("vsetvli zero, %0, e16, m8, ta, ma\n"
                 "vle16.v v0, (%1)\n"
                 "vle16.v v16, (%2)\n"
                 "vle16.v v24, (%3)"
                 :: "r"((uintptr_t)256), "r"(a), "r"(first_bp),
                    "r"(first_bp + tile_words)
                 : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                   "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                   "memory");
#endif
    for (unsigned mb = 0; mb < m; mb += 16) {
#if BENCHMARK
      unsigned gg_region_start = get_cycle();
#endif
      // ReLU uses lazy-zero accumulation and adds the unchanged Zinit bias
      // after extraction.  Other paths retain accumulator seeding so their
      // floating-point operation order is unchanged.
#if GG_OPE_BIAS
      asm volatile("msetmtype %0, %1\n"
                   "msettn zero, %2"
                   :: "r"(matrix_mtype), "r"(matrix_vtype),
                      "r"((uintptr_t)16) : "memory");
#else
      asm volatile("vsetivli zero, 16, e32, m1, ta, ma\n"
                   "msetmtypei 1, 2, 0\n"
                   "msettn zero, %0" :: "r"((uintptr_t)16) : "memory");
#endif
      if (post_op == 1) {
        asm volatile("vtzero mt0\nvtzero mt4\nvtzero mt8\nvtzero mt12" ::: "memory");
      } else if (post_op == 3) {
        // General FP32 initial C: seed each N16 tile from its own vector.
        // This is used by the MLP residual path, whose token lanes differ.
        for (unsigned row = 0; row < 16; ++row) {
          const float *src = c + (mb + row) * n + nb;
          asm volatile("vle32.v v8, (%0)\nvtmv.t.v %1, v8"
                       :: "r"(src), "r"((uintptr_t)row) : "v8", "memory");
          asm volatile("vle32.v v8, (%0)\nvtmv.t.v %1, v8"
                       :: "r"(src + 16),
                          "r"((uintptr_t)0x20000000u + row) : "v8", "memory");
          asm volatile("vle32.v v8, (%0)\nvtmv.t.v %1, v8"
                       :: "r"(src + 32),
                          "r"((uintptr_t)0x40000000u + row) : "v8", "memory");
          asm volatile("vle32.v v8, (%0)\nvtmv.t.v %1, v8"
                       :: "r"(src + 48),
                          "r"((uintptr_t)0x60000000u + row) : "v8", "memory");
        }
      } else {
        for (unsigned row = 0; row < 16; ++row) {
          asm volatile("vfmv.v.f v8, %0" :: "f"(c[(mb + row) * n + nb]) : "v8");
          asm volatile("vtmv.t.v %0, v8" :: "r"((uintptr_t)row) : "memory");
          asm volatile("vtmv.t.v %0, v8" :: "r"((uintptr_t)0x20000000u + row) : "memory");
          asm volatile("vtmv.t.v %0, v8" :: "r"((uintptr_t)0x40000000u + row) : "memory");
          asm volatile("vtmv.t.v %0, v8" :: "r"((uintptr_t)0x60000000u + row) : "memory");
        }
      }

      const uint32_t *ap = a + (mb / 16) * tile_words;
#if BENCHMARK
      unsigned gg_region_end = get_cycle();
      gg_setup_cycles += gg_region_end - gg_region_start;
      gg_region_start = gg_region_end;
#endif
#if GG_OPE_BIAS
      unsigned operands_preloaded = 1;
#else
      unsigned operands_preloaded = panel_preloaded;
      panel_preloaded = 0;
#endif
#if !GG_OPE_BIAS
      if (operands_preloaded)
        asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                     :: "r"(matrix_mtype), "r"(matrix_vtype),
                        "r"((uintptr_t)16) : "memory");
#endif
      for (unsigned pg = 0; pg < pair_rows; pg += 8) {
        const uint32_t *bp = b + (nb / 16) * tile_words + pg * 16;
#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
        // One completed bag is scheduled in each M16 panel of the first
        // N64 block.  v8..v15 are otherwise unused by the matrix operands.
        const unsigned gather_tail = (nb == 0);
        const unsigned gb = GG_INTERLEAVE_BEGIN +
                            (mb / 16) * (pair_rows / 8) + pg / 8;
        const uint16_t *g0=0,*g1=0,*g2=0,*g3=0,*g4=0,*g5=0,*g6=0,*g7=0;
        if (gather_tail) {
          g0=table+idx[gb*L_SIZE+0]*D_SIZE; g1=table+idx[gb*L_SIZE+1]*D_SIZE;
          g2=table+idx[gb*L_SIZE+2]*D_SIZE; g3=table+idx[gb*L_SIZE+3]*D_SIZE;
          g4=table+idx[gb*L_SIZE+4]*D_SIZE; g5=table+idx[gb*L_SIZE+5]*D_SIZE;
          g6=table+idx[gb*L_SIZE+6]*D_SIZE; g7=table+idx[gb*L_SIZE+7]*D_SIZE;
        }
#endif
        if (!operands_preloaded) {
          asm volatile("vsetvli zero, %0, e16, m8, ta, ma\n"
                       "vle16.v v0, (%1)\n"
                       "vle16.v v16, (%2)\n"
                       "vle16.v v24, (%3)"
                       :: "r"((uintptr_t)256), "r"(ap + pg * 16),
                          "r"(bp), "r"(bp + tile_words)
                       : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                         "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                         "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
          asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                       :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
        }
        operands_preloaded = 0;
#if GG_BULK_B_TILES
        // Queue a complete tile before overwriting its operand registers.
        // Once all eight requests have entered the OPE queue their operands
        // are captured, so a single e16,m8 load may replace the two m4
        // halves and their two matrix reconfigurations.  This is an
        // alternative scheduling experiment; arithmetic and packed layout
        // are identical to the m4-overlap path below.
        GG_VTFMM8(mt0);
        asm volatile("vsetvli zero, %0, e16, m8, ta, ma\n"
                     "vle16.v v16, (%1)"
                     :: "r"((uintptr_t)256), "r"(bp + 2 * tile_words)
                     : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "memory");
        asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                     :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
        GG_VTFMM8(mt4);
        asm volatile("vsetvli zero, %0, e16, m8, ta, ma\n"
                     "vle16.v v24, (%1)"
                     :: "r"((uintptr_t)256), "r"(bp + 3 * tile_words)
                     : "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
        asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                     :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
        GG_VTFMM8(mt8);
        GG_VTFMM8_B24(mt12);
        if (pg + 8 < pair_rows) {
          const uint32_t *next_bp = bp + 8 * 16;
          asm volatile("vsetvli zero, %0, e16, m8, ta, ma\n"
                       "vle16.v v0, (%1)\n"
                       "vle16.v v16, (%2)\n"
                       "vle16.v v24, (%3)"
                       :: "r"((uintptr_t)256), "r"(ap + (pg + 8) * 16),
                          "r"(next_bp), "r"(next_bp + tile_words)
                       : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                         "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                         "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
          asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                       :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
          operands_preloaded = 1;
        }
#else
#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
        if (gather_tail) {
          GG_FMA4_LO16(mt0); GG_GATHER_INIT(g0);
        } else
#endif
        // The operand queue holds four vtfmm instructions.  Use that exact
        // depth, then replace the consumed low half of B0 with B2 while the
        // OPE drains the queue.  Repeat for the high half.  This preserves
        // the packed input and arithmetic order while overlapping VLSU and
        // OPE work instead of stalling on an eight-instruction burst.
        GG_FMA4_LO16(mt0);
        asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                     "vle16.v v16, (%1)"
                     :: "r"((uintptr_t)128), "r"(bp + 2 * tile_words)
                     : "v16", "v17", "v18", "v19", "memory");
        asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                     :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
        GG_FMA4_HI16(mt0);
#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
        if (gather_tail) GG_GATHER_ADD(g1);
#endif
        asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                     "vle16.v v20, (%1)"
                     :: "r"((uintptr_t)128),
                        "r"(bp + 2 * tile_words + 4 * 16)
                     : "v20", "v21", "v22", "v23", "memory");
        asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                     :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
        if (gather_tail) {
          GG_FMA4_LO24(mt4); GG_GATHER_ADD(g2);
        } else
#endif
        GG_FMA4_LO24(mt4);
        asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                     "vle16.v v24, (%1)"
                     :: "r"((uintptr_t)128), "r"(bp + 3 * tile_words)
                     : "v24", "v25", "v26", "v27", "memory");
        asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                     :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
        GG_FMA4_HI24(mt4);
#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
        if (gather_tail) GG_GATHER_ADD(g3);
#endif
        asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                     "vle16.v v28, (%1)"
                     :: "r"((uintptr_t)128),
                        "r"(bp + 3 * tile_words + 4 * 16)
                     : "v28", "v29", "v30", "v31", "memory");
        asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                     :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
        if (gather_tail) {
          GG_FMA4_LO16(mt8); GG_GATHER_ADD(g4);
          GG_FMA4_HI16(mt8); GG_GATHER_ADD(g5);
          GG_FMA4_LO24(mt12); GG_GATHER_ADD(g6);
          GG_FMA4_HI24(mt12); GG_GATHER_ADD(g7);
#if MODE_GCN
          { uint16_t sb=SCALE_F16_BITS; _Float16 s=*(_Float16*)&sb;
            asm volatile("vfmul.vf v8,v8,%0\nvfmul.vf v9,v9,%0\n"
                         "vfmul.vf v10,v10,%0\nvfmul.vf v11,v11,%0"::"f"(s)); }
#endif
          const unsigned stride=16*sizeof(uint32_t);
          uint32_t *q=pool+(gb/16)*(D_SIZE/2)*16+(gb%16);
          uint32_t *qh=q+16*16;
          asm volatile("vsse16.v v10,(%0),%1"::"r"((uint16_t*)q),"r"(stride):"memory");
          asm volatile("vsse16.v v8,(%0),%1"::"r"((uint16_t*)q+1),"r"(stride):"memory");
          asm volatile("vsse16.v v11,(%0),%1"::"r"((uint16_t*)qh),"r"(stride):"memory");
          asm volatile("vsse16.v v9,(%0),%1"::"r"((uint16_t*)qh+1),"r"(stride):"memory");
        } else {
#endif
          if (pg + 8 < pair_rows) {
            const uint32_t *next_bp = bp + 8 * 16;
            GG_FMA4_LO16(mt8);
            GG_FMA4_LO24(mt12);
            asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                         "vle16.v v0, (%1)\n"
                         "vle16.v v16, (%2)\n"
                         "vle16.v v24, (%3)"
                         :: "r"((uintptr_t)128), "r"(ap + (pg + 8) * 16),
                            "r"(next_bp), "r"(next_bp + tile_words)
                         : "v0", "v1", "v2", "v3", "v16", "v17", "v18", "v19",
                           "v24", "v25", "v26", "v27", "memory");
            asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                         :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
            GG_FMA4_HI16(mt8);
            GG_FMA4_HI24(mt12);
            asm volatile("vsetvli zero, %0, e16, m4, ta, ma\n"
                         "vle16.v v4, (%1)\n"
                         "vle16.v v20, (%2)\n"
                         "vle16.v v28, (%3)"
                         :: "r"((uintptr_t)128), "r"(ap + (pg + 8) * 16 + 4 * 16),
                            "r"(next_bp + 4 * 16),
                            "r"(next_bp + tile_words + 4 * 16)
                         : "v4", "v5", "v6", "v7", "v20", "v21", "v22", "v23",
                           "v28", "v29", "v30", "v31", "memory");
            asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                         :: "r"(matrix_mtype), "r"(matrix_vtype), "r"((uintptr_t)16) : "memory");
            operands_preloaded = 1;
          } else {
            GG_VTFMM8(mt8);
            GG_VTFMM8_B24(mt12);
          }
#if defined(GG_INTERLEAVE_GATHER) && GG_INTERLEAVE_GATHER
        }
#endif
#endif
      }

#if BENCHMARK
      gg_region_end = get_cycle();
      gg_compute_cycles += gg_region_end - gg_region_start;
      gg_region_start = gg_region_end;
#endif

#if GG_OPE_BIAS
      if (post_op == 1) {
        const uintptr_t bias_mtype = (16u << 10) | (1u << 5) | 1u;
        const float one = 1.0f;
        asm volatile("vsetivli zero, 16, e32, m1, ta, ma\n"
                     "vlse32.v v0, (%0), %1\n"
                     "vfmv.v.f v16, %2\n"
                     "msetmtype %3, %4\n"
                     "msettn zero, %5\n"
                     "vtfmm.tvv mt0, v0, v16\n"
                     "vtfmm.tvv mt4, v0, v16\n"
                     "vtfmm.tvv mt8, v0, v16\n"
                     "vtfmm.tvv mt12, v0, v16"
                     :: "r"(c + mb * n + nb),
                        "r"((uintptr_t)(n * sizeof(float))), "f"(one),
                        "r"(bias_mtype), "r"((uintptr_t)0xd0u),
                        "r"((uintptr_t)16)
                     : "v0", "v16", "memory");
      }
#endif

      // Extract four output rows at a time.  Each row occupies one aligned
      // m4 group, so one matrix configuration and one vector configuration
      // are shared by four rows instead of being repeated for every row.
      float *out0 = c + mb * n + nb;
      float *out1 = out0 + n;
      float *out2 = out1 + n;
      float *out3 = out2 + n;
      // N=64 lets two adjacent rows form one aligned e32,m8 group.  Extract
      // eight rows per matrix configuration, add their distinct scalar
      // biases as m4 groups, then use four m8 ReLU/store pairs.  Compared to
      // four-row batches this halves the configuration changes and halves
      // both the ReLU and store instruction counts without changing data.
      if (post_op == 1) {
        float *r0 = c + mb * n + nb;
        uintptr_t ts0 = 0;
        uintptr_t ts1 = 0x20000000u;
        uintptr_t ts2 = 0x40000000u;
        uintptr_t ts3 = 0x60000000u;
        for (unsigned batch = 0; batch < 2; ++batch) {
          float *r1 = r0 + n;
          float *r2 = r1 + n;
          float *r3 = r2 + n;
          float *r4 = r3 + n;
          float *r5 = r4 + n;
          float *r6 = r5 + n;
          float *r7 = r6 + n;
#if GG_OPE_BIAS
          if (batch != 0)
#endif
            asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                         :: "r"(store_mtype), "r"(store_vtype),
                            "r"((uintptr_t)16) : "memory");
          // Drain tile-major.  mt0/mt4/mt8 are complete before mt12's final
          // queue entries retire, so their 24 moves hide that tail instead
          // of immediately blocking on the first mt12 row of every batch.
          asm volatile(
              "vtmv.v.t v0, %0\n  addi %0, %0, 1\n"
              "vtmv.v.t v4, %0\n  addi %0, %0, 1\n"
              "vtmv.v.t v8, %0\n  addi %0, %0, 1\n"
              "vtmv.v.t v12, %0\n addi %0, %0, 1\n"
              "vtmv.v.t v16, %0\n addi %0, %0, 1\n"
              "vtmv.v.t v20, %0\n addi %0, %0, 1\n"
              "vtmv.v.t v24, %0\n addi %0, %0, 1\n"
              "vtmv.v.t v28, %0\n"
              "vtmv.v.t v1, %1\n  addi %1, %1, 1\n"
              "vtmv.v.t v5, %1\n  addi %1, %1, 1\n"
              "vtmv.v.t v9, %1\n  addi %1, %1, 1\n"
              "vtmv.v.t v13, %1\n addi %1, %1, 1\n"
              "vtmv.v.t v17, %1\n addi %1, %1, 1\n"
              "vtmv.v.t v21, %1\n addi %1, %1, 1\n"
              "vtmv.v.t v25, %1\n addi %1, %1, 1\n"
              "vtmv.v.t v29, %1\n"
              "vtmv.v.t v2, %2\n  addi %2, %2, 1\n"
              "vtmv.v.t v6, %2\n  addi %2, %2, 1\n"
              "vtmv.v.t v10, %2\n addi %2, %2, 1\n"
              "vtmv.v.t v14, %2\n addi %2, %2, 1\n"
              "vtmv.v.t v18, %2\n addi %2, %2, 1\n"
              "vtmv.v.t v22, %2\n addi %2, %2, 1\n"
              "vtmv.v.t v26, %2\n addi %2, %2, 1\n"
              "vtmv.v.t v30, %2\n"
              "vtmv.v.t v3, %3\n  addi %3, %3, 1\n"
              "vtmv.v.t v7, %3\n  addi %3, %3, 1\n"
              "vtmv.v.t v11, %3\n addi %3, %3, 1\n"
              "vtmv.v.t v15, %3\n addi %3, %3, 1\n"
              "vtmv.v.t v19, %3\n addi %3, %3, 1\n"
              "vtmv.v.t v23, %3\n addi %3, %3, 1\n"
              "vtmv.v.t v27, %3\n addi %3, %3, 1\n"
              "vtmv.v.t v31, %3"
              : "+r"(ts0), "+r"(ts1), "+r"(ts2), "+r"(ts3)
              :: "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                 "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                 "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                 "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                 "memory");
          if (batch == 0) {
            ++ts0;
            ++ts1;
            ++ts2;
            ++ts3;
          }
#if !GG_OPE_BIAS
          asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n"
                       "vfadd.vf v0, v0, %[b0]\n"
                       "vfadd.vf v4, v4, %[b1]\n"
                       "vfadd.vf v8, v8, %[b2]\n"
                       "vfadd.vf v12, v12, %[b3]\n"
                       "vfadd.vf v16, v16, %[b4]\n"
                       "vfadd.vf v20, v20, %[b5]\n"
                       "vfadd.vf v24, v24, %[b6]\n"
                       "vfadd.vf v28, v28, %[b7]"
                       :: "r"((uintptr_t)64)
                          , [b0] "f"(*r0), [b1] "f"(*r1),
                          [b2] "f"(*r2), [b3] "f"(*r3),
                          [b4] "f"(*r4), [b5] "f"(*r5),
                          [b6] "f"(*r6), [b7] "f"(*r7)
                       : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                         "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                         "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                         "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                         "memory");
#endif
          if (n == 64) {
#if defined(GG_OPE_BIAS_SINGLE_POINTER) && GG_OPE_BIAS_SINGLE_POINTER
            uintptr_t outp = (uintptr_t)r0;
            if (batch == 1 && mb + 16 < m) {
              const uint32_t *next_ap = a + (mb / 16 + 1) * tile_words;
              const uint32_t *next_bp = b + (nb / 16) * tile_words;
              asm volatile("vsetvli zero, %[vl], e32, m8, ta, ma\n"
                           RELU_ASM("v0") "vse32.v v0, (%[out])\n"
                           "addi %[out], %[out], 512\n"
                           "vle32.v v0, (%[na])\n"
                           RELU_ASM("v8") "vse32.v v8, (%[out])\n"
                           "addi %[out], %[out], 512\n"
                           RELU_ASM("v16") "vse32.v v16, (%[out])\n"
                           "addi %[out], %[out], 512\n"
                           "vle32.v v16, (%[nb0])\n"
                           RELU_ASM("v24") "vse32.v v24, (%[out])\n"
                           "addi %[out], %[out], 512\n"
                           "vle32.v v24, (%[nb1])"
                           : [out] "+&r"(outp)
                           : [vl] "r"((uintptr_t)128), [na] "r"(next_ap),
                              [nb0] "r"(next_bp),
                              [nb1] "r"(next_bp + tile_words),
                              RELU_INPUTS
                           : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                             "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                             "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                             "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                             "memory");
#if !GG_OPE_BIAS
              panel_preloaded = 1;
#endif
            } else {
              asm volatile("vsetvli zero, %[vl], e32, m8, ta, ma\n"
                           RELU_ASM("v0")  "vse32.v v0, (%[out])\n"
                           "addi %[out], %[out], 512\n"
                           RELU_ASM("v8")  "vse32.v v8, (%[out])\n"
                           "addi %[out], %[out], 512\n"
                           RELU_ASM("v16") "vse32.v v16, (%[out])\n"
                           "addi %[out], %[out], 512\n"
                           RELU_ASM("v24") "vse32.v v24, (%[out])\n"
                           "addi %[out], %[out], 512"
                           : [out] "+&r"(outp)
                           : [vl] "r"((uintptr_t)128), RELU_INPUTS
                           : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                             "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                             "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                             "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                             "memory");
            }
            r0 = (float *)outp;
#else
            if (batch == 1 && mb + 16 < m) {
              const uint32_t *next_ap = a + (mb / 16 + 1) * tile_words;
              const uint32_t *next_bp = b + (nb / 16) * tile_words;
              asm volatile("vsetvli zero, %0, e32, m8, ta, ma\n"
                           RELU_ASM("v0")
                           RELU_ASM("v8")
                           RELU_ASM("v16")
                           "vse32.v v0, (%1)\n"
                           RELU_ASM("v24")
                           "vle32.v v0, (%5)\n"
                           "vse32.v v8, (%2)\n"
                           "vse32.v v16, (%3)\n"
                           "vse32.v v24, (%4)\n"
                           "vle32.v v16, (%6)\n"
                           "vle32.v v24, (%7)"
                           :: "r"((uintptr_t)128), "r"(r0), "r"(r2),
                              "r"(r4), "r"(r6), "r"(next_ap),
                              "r"(next_bp), "r"(next_bp + tile_words),
                              RELU_INPUTS
                           : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                             "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                             "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                             "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                             "memory");
#if !GG_OPE_BIAS
              panel_preloaded = 1;
#endif
            } else {
              asm volatile("vsetvli zero, %0, e32, m8, ta, ma\n"
                           RELU_ASM("v0")
                           RELU_ASM("v8")
                           RELU_ASM("v16")
                           "vse32.v v0, (%1)\n"
                           RELU_ASM("v24")
                           "vse32.v v8, (%2)\n"
                           "vse32.v v16, (%3)\n"
                           "vse32.v v24, (%4)"
                           :: "r"((uintptr_t)128), "r"(r0), "r"(r2),
                              "r"(r4), "r"(r6), RELU_INPUTS
                           : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                             "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                             "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                             "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                             "memory");
            }
#endif
          } else {
            // With ldc > 64, adjacent N64 rows are not contiguous and may
            // not be combined into one m8 store.  Keep the same eight-row
            // extraction/configuration batch, but issue one legal m4
            // ReLU/store per row.
            asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n"
                         RELU_ASM("v0")  "vse32.v v0, (%1)\n"
                         RELU_ASM("v4")  "vse32.v v4, (%2)\n"
                         RELU_ASM("v8")  "vse32.v v8, (%3)\n"
                         RELU_ASM("v12") "vse32.v v12, (%4)\n"
                         RELU_ASM("v16") "vse32.v v16, (%5)\n"
                         RELU_ASM("v20") "vse32.v v20, (%6)\n"
                         RELU_ASM("v24") "vse32.v v24, (%7)\n"
                         RELU_ASM("v28") "vse32.v v28, (%8)"
                         :: "r"((uintptr_t)64), "r"(r0), "r"(r1),
                            "r"(r2), "r"(r3), "r"(r4), "r"(r5),
                            "r"(r6), "r"(r7), RELU_INPUTS
                         : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                           "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                           "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                           "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                           "memory");
          }
#if !defined(GG_OPE_BIAS_SINGLE_POINTER) || !GG_OPE_BIAS_SINGLE_POINTER
          r0 += 8 * n;
#endif
        }
#if BENCHMARK
        gg_region_end = get_cycle();
        gg_drain_cycles += gg_region_end - gg_region_start;
#endif
        continue;
      }
      for (unsigned row = 0; row < 16; row += 4) {
        asm volatile("msetmtype %0, %1\nmsettn zero, %2"
                     :: "r"(store_mtype), "r"(store_vtype), "r"((uintptr_t)16) : "memory");
        asm volatile("vtmv.v.t v0, %0\nvtmv.v.t v1, %1\nvtmv.v.t v2, %2\nvtmv.v.t v3, %3"
                     :: "r"((uintptr_t)row), "r"((uintptr_t)0x20000000u + row),
                        "r"((uintptr_t)0x40000000u + row), "r"((uintptr_t)0x60000000u + row)
                     : "v0", "v1", "v2", "v3", "memory");
        asm volatile("vtmv.v.t v4, %0\nvtmv.v.t v5, %1\nvtmv.v.t v6, %2\nvtmv.v.t v7, %3"
                     :: "r"((uintptr_t)row + 1), "r"((uintptr_t)0x20000000u + row + 1),
                        "r"((uintptr_t)0x40000000u + row + 1), "r"((uintptr_t)0x60000000u + row + 1)
                     : "v4", "v5", "v6", "v7", "memory");
        asm volatile("vtmv.v.t v8, %0\nvtmv.v.t v9, %1\nvtmv.v.t v10, %2\nvtmv.v.t v11, %3"
                     :: "r"((uintptr_t)row + 2), "r"((uintptr_t)0x20000000u + row + 2),
                        "r"((uintptr_t)0x40000000u + row + 2), "r"((uintptr_t)0x60000000u + row + 2)
                     : "v8", "v9", "v10", "v11", "memory");
        asm volatile("vtmv.v.t v12, %0\nvtmv.v.t v13, %1\nvtmv.v.t v14, %2\nvtmv.v.t v15, %3"
                     :: "r"((uintptr_t)row + 3), "r"((uintptr_t)0x20000000u + row + 3),
                        "r"((uintptr_t)0x40000000u + row + 3), "r"((uintptr_t)0x60000000u + row + 3)
                     : "v12", "v13", "v14", "v15", "memory");
        // Keep one complete N64 row in an m4 group.  Issue its store before
        // processing the next row so the VLSU can drain the previous result
        // while the VAU computes the next ReLU.
        // GELU is issued on two independent row pairs.  Each output row is
        // already an aligned e32,m4 group, so no intermediate L1 round trip
        // is introduced between the matrix operation and the post-op.
        if (post_op == 2) {
          asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n"
                       GELU_PAIR_ASM("v0", "v4", "v16", "v20", "v24", "v28")
                       :: "r"((uintptr_t)64), GELU_INPUTS
                       : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                         "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                         "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                         "memory");
          asm volatile("vse32.v v0, (%0)" :: "r"(out0)
                       : "v0", "v1", "v2", "v3", "memory");
          asm volatile("vse32.v v4, (%0)" :: "r"(out1)
                       : "v4", "v5", "v6", "v7", "memory");
          asm volatile(GELU_PAIR_ASM("v8", "v12", "v16", "v20", "v24", "v28")
                       :: GELU_INPUTS
                       : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                         "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                         "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                         "memory");
          asm volatile("vse32.v v8, (%0)" :: "r"(out2)
                       : "v8", "v9", "v10", "v11", "memory");
          asm volatile("vse32.v v12, (%0)" :: "r"(out3)
                       : "v12", "v13", "v14", "v15", "memory");
        }
        if (post_op == 0 || post_op == 3) {
          asm volatile("vsetvli zero, %0, e32, m4, ta, ma" :: "r"((uintptr_t)64) : "memory");
          asm volatile("vse32.v v0, (%0)" :: "r"(out0)
                       : "v0", "v1", "v2", "v3", "memory");
          asm volatile("vse32.v v4, (%0)" :: "r"(out1)
                       : "v4", "v5", "v6", "v7", "memory");
          asm volatile("vse32.v v8, (%0)" :: "r"(out2)
                       : "v8", "v9", "v10", "v11", "memory");
          asm volatile("vse32.v v12, (%0)" :: "r"(out3)
                       : "v12", "v13", "v14", "v15", "memory");
        }
        out0 += 4 * n;
        out1 += 4 * n;
        out2 += 4 * n;
        out3 += 4 * n;
      }
#if BENCHMARK
      gg_region_end = get_cycle();
      gg_drain_cycles += gg_region_end - gg_region_start;
#endif
    }
  }
  asm volatile("lw zero, 0(%0)" :: "r"(c) : "memory");
}

#undef GG_VTFMM8
#undef GG_VTFMM8_B24
#undef GG_EXTRACT_ROW
#undef GG_FMA4_LO16
#undef GG_FMA4_HI16
#undef GG_FMA4_LO24
#undef GG_FMA4_HI24
#undef GG_GATHER_INIT
#undef GG_GATHER_ADD
