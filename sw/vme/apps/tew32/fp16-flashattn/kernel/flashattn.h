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
// FlashAttention only needs the non-positive-input exponential variants from
// transformer_softmax.h. Keep them local so this fixed kernel does not pull in the
// generic softmax implementations.
#define SOFTMAX_EXP_ASM(x, t) \
  "vfmax.vf " x ", " x ", %[transformer_lo]\n" \
  "vfmul.vf " x ", " x ", %[transformer_scale]\n" \
  "vfmv.v.f " t ", %[transformer_c5]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[transformer_c4]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[transformer_c3]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[transformer_c2]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[transformer_one]\n" \
  "vfmul.vv " t ", " t ", " x "\n" \
  "vfadd.vf " t ", " t ", %[transformer_one]\n" \
  "vfmul.vv " x ", " t ", " t "\n" \
  "vfmul.vv " x ", " x ", " x "\n" \
  "vfmul.vv " x ", " x ", " x "\n" \
  "vfmul.vv " x ", " x ", " x "\n" \
  "vfmul.vv " x ", " x ", " x "\n" \
  "vfmul.vv " x ", " x ", " x "\n"

#define SOFTMAX_EXP_UNROLL4(x0, x1, x2, x3, t0, t1, t2, t3) \
  "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
  "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
  "vfmax.vf " x2 ", " x2 ", %[transformer_lo]\n" \
  "vfmax.vf " x3 ", " x3 ", %[transformer_lo]\n" \
  "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
  "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
  "vfmul.vf " x2 ", " x2 ", %[transformer_scale]\n" \
  "vfmul.vf " x3 ", " x3 ", %[transformer_scale]\n" \
  "vfmul.vf " t0 ", " x0 ", %[transformer_c5]\n" \
  "vfmul.vf " t1 ", " x1 ", %[transformer_c5]\n" \
  "vfmul.vf " t2 ", " x2 ", %[transformer_c5]\n" \
  "vfmul.vf " t3 ", " x3 ", %[transformer_c5]\n" \
  "vfadd.vf " t0 ", " t0 ", %[transformer_c4]\n" \
  "vfadd.vf " t1 ", " t1 ", %[transformer_c4]\n" \
  "vfadd.vf " t2 ", " t2 ", %[transformer_c4]\n" \
  "vfadd.vf " t3 ", " t3 ", %[transformer_c4]\n" \
  "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
  "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
  "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
  "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
  "vfadd.vf " t0 ", " t0 ", %[transformer_c3]\n" \
  "vfadd.vf " t1 ", " t1 ", %[transformer_c3]\n" \
  "vfadd.vf " t2 ", " t2 ", %[transformer_c3]\n" \
  "vfadd.vf " t3 ", " t3 ", %[transformer_c3]\n" \
  "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
  "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
  "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
  "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
  "vfadd.vf " t0 ", " t0 ", %[transformer_c2]\n" \
  "vfadd.vf " t1 ", " t1 ", %[transformer_c2]\n" \
  "vfadd.vf " t2 ", " t2 ", %[transformer_c2]\n" \
  "vfadd.vf " t3 ", " t3 ", %[transformer_c2]\n" \
  "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
  "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
  "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
  "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
  "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
  "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
  "vfadd.vf " t2 ", " t2 ", %[transformer_one]\n" \
  "vfadd.vf " t3 ", " t3 ", %[transformer_one]\n" \
  "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
  "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
  "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
  "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
  "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
  "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
  "vfadd.vf " t2 ", " t2 ", %[transformer_one]\n" \
  "vfadd.vf " t3 ", " t3 ", %[transformer_one]\n" \
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

#ifndef benchmark_get_cycle
#define benchmark_get_cycle pipeline_cycle
#endif
#ifndef start_kernel
#define start_kernel() ((void)0)
#endif
#ifndef stop_kernel
#define stop_kernel() ((void)0)
#endif
#define PG_CFG32() asm volatile("msetmtypei 1, 2, 0\nmsettn zero, %0" :: "r"(16u) : "memory")
#define PG_LOAD(v,p) asm volatile("vle32.v " #v ", (%0)" :: "r"(p) : #v,"memory")
#define PG_MAC(t,a,b) asm volatile("vtfmm.tvv " #t ", " #a ", " #b ::: "memory")
// Packed pair-major operands: A[K/2][M], B[K/2][N]. 2x2 tiles.
// bias==NULL loads initial C; otherwise broadcast channel bias, fused ReLU.
#ifndef PG_GEMM_ATTRIBUTES
#define PG_GEMM_ATTRIBUTES static __attribute__((noinline))
#endif
#ifndef PG_USE_VTLE_SEED
#define PG_USE_VTLE_SEED 0
#endif
#ifndef PG_UNROLL_FIXED_PV
#define PG_UNROLL_FIXED_PV 0
#endif
#ifndef PG_MAC_BURST4
#define PG_MAC_BURST4 0
#endif
static __attribute__((always_inline)) inline void pipeline_gemm_body(const uint32_t *a,
 const uint32_t *b,float *c,unsigned m,unsigned k,unsigned n,const float *bias,
 unsigned fuse_relu,unsigned zero_seed) {
 asm volatile("vsetivli zero, 16, e32, m1, ta, ma" ::: "memory");
 for(unsigned i=0;i<m;i+=32) for(unsigned j=0;j<n;j+=32) {
  PG_CFG32();
  if(zero_seed) {
   asm volatile("vtzero mt0\nvtzero mt4\nvtzero mt8\nvtzero mt12" ::: "memory");
  } else for(unsigned t=0;t<4;++t) for(unsigned r=0;r<16;++r) {
   unsigned row=i+(t/2)*16+r; uintptr_t sel=t*0x20000000u+r;
   if(bias) {
    asm volatile("vfmv.v.f v8, %0\nvtmv.t.v %1, v8"
                 :: "f"(bias[row]), "r"(sel) : "v8", "memory");
   } else {
#if PG_USE_VTLE_SEED
    asm volatile("vtle32 %0, (%1)" :: "r"(sel),
                 "r"(c+row*n+j+(t%2)*16) : "memory");
#else
    PG_LOAD(v8,c+row*n+j+(t%2)*16);
    asm volatile("vtmv.t.v %0, v8" :: "r"(sel) : "memory");
#endif
   }
  }
  asm volatile("msetmtype %0, %1\nmsettn zero, %2\nmsettm zero, %2" :: "r"(16450u),"r"(0xc8u),"r"(16u):"memory");
  // Under matrix e16,m1, an e32 load has EMUL=2.  Keep every load base
  // register even; v0/v2 and v16/v18 are the current operand bank, while
  // v4/v6 and v20/v22 prefetch the next reduction pair.
  PG_LOAD(v0,a+i); PG_LOAD(v2,a+i+16);
  PG_LOAD(v16,b+j); PG_LOAD(v18,b+j+16);
#if PG_UNROLL_FIXED_PV
  // This FlashAttention variant always has BC=64.  Keep the standard operand
  // order and store schedule, but expose the fixed reduction count to LLVM.
  const unsigned reduction_pairs = 32;
#pragma clang loop unroll(full)
#else
  const unsigned reduction_pairs = k/2;
#endif
  for(unsigned p=0;p+2<reduction_pairs;p+=2) {
#if PG_MAC_BURST4
   PG_LOAD(v4,a+(p+1)*m+i); PG_LOAD(v6,a+(p+1)*m+i+16);
   PG_LOAD(v20,b+(p+1)*n+j); PG_LOAD(v22,b+(p+1)*n+j+16);
   PG_MAC(mt0,v0,v16); PG_MAC(mt4,v0,v18);
   PG_MAC(mt8,v2,v16); PG_MAC(mt12,v2,v18);
   PG_LOAD(v0,a+(p+2)*m+i); PG_LOAD(v2,a+(p+2)*m+i+16);
   PG_LOAD(v16,b+(p+2)*n+j); PG_LOAD(v18,b+(p+2)*n+j+16);
   PG_MAC(mt0,v4,v20); PG_MAC(mt4,v4,v22);
   PG_MAC(mt8,v6,v20); PG_MAC(mt12,v6,v22);
#else
   PG_MAC(mt0,v0,v16); PG_LOAD(v4,a+(p+1)*m+i);
   PG_MAC(mt4,v0,v18); PG_LOAD(v6,a+(p+1)*m+i+16);
   PG_MAC(mt8,v2,v16); PG_LOAD(v20,b+(p+1)*n+j);
   PG_MAC(mt12,v2,v18); PG_LOAD(v22,b+(p+1)*n+j+16);
   PG_MAC(mt0,v4,v20); PG_LOAD(v0,a+(p+2)*m+i);
   PG_MAC(mt4,v4,v22); PG_LOAD(v2,a+(p+2)*m+i+16);
   PG_MAC(mt8,v6,v20); PG_LOAD(v16,b+(p+2)*n+j);
   PG_MAC(mt12,v6,v22); PG_LOAD(v18,b+(p+2)*n+j+16);
#endif
  }
#if PG_MAC_BURST4
  PG_LOAD(v4,a+(reduction_pairs-1)*m+i);
  PG_LOAD(v6,a+(reduction_pairs-1)*m+i+16);
  PG_LOAD(v20,b+(reduction_pairs-1)*n+j);
  PG_LOAD(v22,b+(reduction_pairs-1)*n+j+16);
  PG_MAC(mt0,v0,v16); PG_MAC(mt4,v0,v18);
  PG_MAC(mt8,v2,v16); PG_MAC(mt12,v2,v18);
#else
  PG_MAC(mt0,v0,v16); PG_LOAD(v4,a+(reduction_pairs-1)*m+i);
  PG_MAC(mt4,v0,v18); PG_LOAD(v6,a+(reduction_pairs-1)*m+i+16);
  PG_MAC(mt8,v2,v16); PG_LOAD(v20,b+(reduction_pairs-1)*n+j);
  PG_MAC(mt12,v2,v18); PG_LOAD(v22,b+(reduction_pairs-1)*n+j+16);
#endif
  PG_MAC(mt0,v4,v20); PG_MAC(mt4,v4,v22);
  PG_MAC(mt8,v6,v20); PG_MAC(mt12,v6,v22);
  PG_CFG32();
  for(unsigned t=0;t<4;++t) for(unsigned r=0;r<16;++r) {
   uintptr_t sel=t*0x20000000u+r;
   float *dst=c+(i+(t/2)*16+r)*n+j+(t%2)*16;
   if(fuse_relu) asm volatile("vtmv.v.t v8, %0\n" RELU_ASM("v8")
      "lw zero, 0(%1)\nvse32.v v8, (%1)" :: "r"(sel),"r"(dst),RELU_INPUTS : "v8","memory");
   else asm volatile("vtse32 %0, (%1)" :: "r"(sel),"r"(dst):"memory");
  }
 }
 asm volatile("lw zero, 0(%0)" :: "r"(c):"memory");
}
PG_GEMM_ATTRIBUTES void pipeline_gemm(const uint32_t *a, const uint32_t *b,
 float *c,unsigned m,unsigned k,unsigned n,const float *bias) {
 pipeline_gemm_body(a,b,c,m,k,n,bias,bias != 0,0);
}
PG_GEMM_ATTRIBUTES void pipeline_gemm_relu_from_c(
 const uint32_t *a,const uint32_t *b,float *c,unsigned m,unsigned k,unsigned n) {
 pipeline_gemm_body(a,b,c,m,k,n,0,1,0);
}
