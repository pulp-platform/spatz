// Copyright 2025-2026 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>
#include "runtime.h"
#include "transformer.h"
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
// Softmax inputs are x-max <= 0, so the positive upper clamp is redundant.
// Keep the same degree-5 Taylor seed and six squarings as optimized O-POPE;
// fused kernels may change scheduling, but never the numerical operation set.
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

#define SOFTMAX_EXP_PAIR_ASM(x0, x1, t0, t1) \
    "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
    "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
    "vfmv.v.f " t0 ", %[transformer_c5]\n" \
    "vfmv.v.f " t1 ", %[transformer_c5]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfmul.vv " x0 ", " t0 ", " t0 "\n" \
    "vfmul.vv " x1 ", " t1 ", " t1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n"

// Same pairwise exp operation order, with four independent memory operations
// placed in dependency gaps. The caller uses these slots to store the
// previous ping-pong pair and load the following pair.
#define SOFTMAX_EXP_PAIR_IO_ASM(x0, x1, t0, t1, op0, op1, op2, op3) \
    "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
    op0 \
    "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
    op1 \
    "vfmv.v.f " t0 ", %[transformer_c5]\n" \
    "vfmv.v.f " t1 ", %[transformer_c5]\n" \
    op2 \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    op3 \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfmul.vv " x0 ", " t0 ", " t0 "\n" \
    "vfmul.vv " x1 ", " t1 ", " t1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n"

#define SOFTMAX_EXP_TRIPLE_ASM(x0, x1, x2, t0, t1, t2) \
    "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
    "vfmax.vf " x2 ", " x2 ", %[transformer_lo]\n" \
    "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
    "vfmul.vf " x2 ", " x2 ", %[transformer_scale]\n" \
    "vfmv.v.f " t0 ", %[transformer_c5]\n" \
    "vfmv.v.f " t1 ", %[transformer_c5]\n" \
    "vfmv.v.f " t2 ", %[transformer_c5]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c4]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c4]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c4]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c3]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c3]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c3]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_c2]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_c2]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_c2]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_one]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfadd.vf " t0 ", " t0 ", %[transformer_one]\n" \
    "vfadd.vf " t1 ", " t1 ", %[transformer_one]\n" \
    "vfadd.vf " t2 ", " t2 ", %[transformer_one]\n" \
    "vfmul.vv " x0 ", " t0 ", " t0 "\n" \
    "vfmul.vv " x1 ", " t1 ", " t1 "\n" \
    "vfmul.vv " x2 ", " t2 ", " t2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n"

// Four independent Horner/squaring chains.  This consumes all eight LMUL=4
// register groups, so callers must finish using their max/sum vectors before
// entering the macro.  It is intended for fixed-size kernels that can keep
// per-quartet sums in scratch memory and combine them after packing.
#if defined(TRANSFORMER_EXP_SCALE32) && TRANSFORMER_EXP_SCALE32
#define SOFTMAX_EXP_QUAD_LAST_SQUARE(x0, x1, x2, x3)
#else
#define SOFTMAX_EXP_QUAD_LAST_SQUARE(x0, x1, x2, x3) \
    "vfmul.vv " x0 ", " x0 ", " x0 "\n" \
    "vfmul.vv " x1 ", " x1 ", " x1 "\n" \
    "vfmul.vv " x2 ", " x2 ", " x2 "\n" \
    "vfmul.vv " x3 ", " x3 ", " x3 "\n"
#endif
#define SOFTMAX_EXP_UNROLL4(x0, x1, x2, x3, t0, t1, t2, t3) \
    "vfmax.vf " x0 ", " x0 ", %[transformer_lo]\n" \
    "vfmax.vf " x1 ", " x1 ", %[transformer_lo]\n" \
    "vfmax.vf " x2 ", " x2 ", %[transformer_lo]\n" \
    "vfmax.vf " x3 ", " x3 ", %[transformer_lo]\n" \
    "vfmul.vf " x0 ", " x0 ", %[transformer_scale]\n" \
    "vfmul.vf " x1 ", " x1 ", %[transformer_scale]\n" \
    "vfmul.vf " x2 ", " x2 ", %[transformer_scale]\n" \
    "vfmul.vf " x3 ", " x3 ", %[transformer_scale]\n" \
    "vfmv.v.f " t0 ", %[transformer_c5]\n" \
    "vfmv.v.f " t1 ", %[transformer_c5]\n" \
    "vfmv.v.f " t2 ", %[transformer_c5]\n" \
    "vfmv.v.f " t3 ", %[transformer_c5]\n" \
    "vfmul.vv " t0 ", " t0 ", " x0 "\n" \
    "vfmul.vv " t1 ", " t1 ", " x1 "\n" \
    "vfmul.vv " t2 ", " t2 ", " x2 "\n" \
    "vfmul.vv " t3 ", " t3 ", " x3 "\n" \
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
    SOFTMAX_EXP_QUAD_LAST_SQUARE(x0, x1, x2, x3)

// buf[vocab][token=64], FP32. Each lane owns a token.
// Three passes: column max; exp + column sum; normalize before final store.
// m4 groups use v8/v12 as ping and v24/v28 as pong.
static inline __attribute__((always_inline)) void softmax_cols_rows(
    float *buf, uint32_t rows) {
  const uintptr_t pairs = rows / 2;
  const uintptr_t quartets = rows / 4;
  asm volatile(
      "vsetvli x0, %[pair_vl], e32, m8, ta, ma\n"
      "mv t0, %[buf]\n"
      "mv t1, %[pairs]\n"
      "vfmv.v.f v0, %[init]\n"
      "1:\n"
      "vle32.v v8, (t0)\n"
      "vfmax.vv v0, v0, v8\n"
      "addi t0, t0, 512\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      "2:\n"
      "li t1, 64\n"
      "vslidedown.vx v8, v0, t1\n"
      "vsetvli x0, %[vl], e32, m4, ta, ma\n"
      "vfmax.vv v0, v0, v8\n"
      "vmv.v.i v4, 0\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "3:\n"
      LOAD_ADVANCE_ASM("v24", "t0", "256")
      LOAD_ADVANCE_ASM("v28", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v4, v4, v12\n"
      STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "addi t1, t1, -1\n"
      "beqz t1, 4f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n"
      "vfadd.vv v4, v4, v28\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      "j 3b\n"
      "4:\n"
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n"
      "vfadd.vv v4, v4, v28\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      RECIP_ASM("v4", "v0", "v16")
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      "5:\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0")
      LOAD_ADVANCE_ASM("v16", "t0", "256")
      STORE_ADVANCE_ASM("v8", "t2", "256")
      NORMALIZE_ASM("v12", "v0")
      LOAD_ADVANCE_ASM("v20", "t0", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      NORMALIZE_ASM("v16", "v0")
      STORE_ADVANCE_ASM("v16", "t2", "256")
      NORMALIZE_ASM("v20", "v0")
      STORE_ADVANCE_ASM("v20", "t2", "256")
      "addi t1, t1, -1\n"
      "bnez t1, 5b\n"
      :
      : [buf] "r"(buf), [vl] "r"((uintptr_t)64),
        [pair_vl] "r"((uintptr_t)128),
        [init] "f"(-__builtin_inff()), [pairs] "r"(pairs),
        [quartets] "r"(quartets), EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

// Fused GEMM-softmax entry: column maxima were accumulated while matrix
// accumulators were drained, so the first read-only pass over buf is omitted.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax(
    float *buf, uint32_t rows, const float *colmax) {
  const uintptr_t quartets = rows / 4;
  asm volatile(
      "vsetvli x0, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v4, 0\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "3:\n"
      LOAD_ADVANCE_ASM("v24", "t0", "256")
      LOAD_ADVANCE_ASM("v28", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n"
      STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "vfadd.vv v4, v4, v12\n"
      "addi t1, t1, -1\n"
      "beqz t1, 4f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      "vfadd.vv v4, v4, v28\n"
      "j 3b\n"
      "4:\n"
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      "vfadd.vv v4, v4, v28\n"
      RECIP_ASM("v4", "v0", "v16")
      // Duplicate the per-token reciprocal into adjacent m4 groups, then
      // normalize/store two contiguous vocabulary rows as one e32,m8 group.
      // This preserves the exact value used by each row while halving the
      // normalization loop's vector instructions and pointer updates.
      "vmv.v.v v4, v0\n"
      "vsetvli zero, %[pair_vl], e32, m8, ta, ma\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "512")
      LOAD_ADVANCE_ASM("v16", "t0", "512")
      "5:\n"
      NORMALIZE_ASM("v8", "v0")
      STORE_ADVANCE_ASM("v8", "t2", "512")
      "addi t1, t1, -1\n"
      "beqz t1, 6f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "512")
      NORMALIZE_ASM("v16", "v0")
      STORE_ADVANCE_ASM("v16", "t2", "512")
      LOAD_ADVANCE_ASM("v16", "t0", "512")
      "j 5b\n"
      "6:\n"
      NORMALIZE_ASM("v16", "v0")
      "vse32.v v16, (t2)\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [vl] "r"((uintptr_t)64),
        [pair_vl] "r"((uintptr_t)128), [quartets] "r"(quartets),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

// P7 form: identical operation order, additionally preserves the pre-reciprocal
// column sum for the log-softmax cross-entropy identity.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_stats(
    float *buf, uint32_t rows, const float *colmax, float *colsum) {
  const uintptr_t pairs = rows / 2, quartets = rows / 4;
  asm volatile(
      "vsetvli x0, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\nvmv.v.i v4, 0\n"
      "mv t0, %[buf]\nmv t2, %[buf]\nmv t1, %[quartets]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "3:\n" LOAD_ADVANCE_ASM("v24", "t0", "256")
      LOAD_ADVANCE_ASM("v28", "t0", "256")
      "vfsub.vv v8, v8, v0\nvfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n" STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256") "vfadd.vv v4, v4, v12\n"
      "addi t1, t1, -1\nbeqz t1, 4f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "vfsub.vv v24, v24, v0\nvfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n" STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256") "vfadd.vv v4, v4, v28\nj 3b\n"
      "4:\nvfsub.vv v24, v24, v0\nvfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v24", "v28", "v16", "v20")
      "vfadd.vv v4, v4, v24\n" STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256") "vfadd.vv v4, v4, v28\n"
      "vse32.v v4, (%[sum])\n"
      RECIP_ASM("v4", "v0", "v16")
      "mv t0, %[buf]\nmv t2, %[buf]\nmv t1, %[pairs]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      "5:\n" LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0") STORE_ADVANCE_ASM("v8", "t2", "256")
      "addi t1, t1, -1\nbeqz t1, 6f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      NORMALIZE_ASM("v12", "v0") STORE_ADVANCE_ASM("v12", "t2", "256")
      "j 5b\n6:\n" NORMALIZE_ASM("v12", "v0") "vse32.v v12, (t2)\n"
      :: [buf] "r"(buf), [max] "r"(colmax), [sum] "r"(colsum),
         [vl] "r"((uintptr_t)64), [pairs] "r"(pairs), [quartets] "r"(quartets),
         EXP_INPUTS, RECIP_INPUTS
      : "t0","t1","t2","v0","v1","v2","v3","v4","v5","v6","v7",
        "v8","v9","v10","v11","v12","v13","v14","v15","v16","v17",
        "v18","v19","v20","v21","v22","v23","v24","v25","v26","v27",
        "v28","v29","v30","v31","memory");
}

// Same operations and row-order accumulation as optimized O-POPE, kept in
// one asm block so the loop carries only one pointer and one counter.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_single(
    float *buf, uint32_t rows, const float *colmax) {
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v12, 0\n"
      "mv t0, %[buf]\n"
      "mv t1, %[rows]\n"
      "1:\n"
      "vle32.v v4, (t0)\n"
      "vfsub.vv v4, v4, v0\n"
      SOFTMAX_EXP_ASM("v4", "v8")
      "vfadd.vv v12, v12, v4\n"
      "vse32.v v4, (t0)\n"
      "addi t0, t0, 256\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      RECIP_ASM("v12", "v16", "v20")
      "mv t0, %[buf]\n"
      "mv t1, %[rows]\n"
      "2:\n"
      "vle32.v v4, (t0)\n"
      "vfmul.vv v4, v4, v16\n"
      "vse32.v v4, (t0)\n"
      "addi t0, t0, 256\n"
      "addi t1, t1, -1\n"
      "bnez t1, 2b\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [rows] "r"((uintptr_t)rows),
        [vl] "r"((uintptr_t)64), EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
        "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
        "memory");
}

// Pairwise exp with L1 stores and future loads inserted into the independent
// FP dependency gaps. Row order, accumulation order and approximation are
// identical to softmax_cols_rows_premax.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_io(
    float *buf, uint32_t rows, const float *colmax) {
  const uintptr_t steady = rows / 4 - 1;
  const uintptr_t pairs = rows / 2;
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v4, 0\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      LOAD_ADVANCE_ASM("v24", "t0", "256")
      LOAD_ADVANCE_ASM("v28", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v4, v4, v12\n"
      "mv t1, %[steady]\n"
      "1:\n"
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_IO_ASM(
          "v24", "v28", "v16", "v20",
          STORE_ADVANCE_ASM("v8", "t2", "256"),
          STORE_ADVANCE_ASM("v12", "t2", "256"),
          LOAD_ADVANCE_ASM("v8", "t0", "256"),
          LOAD_ADVANCE_ASM("v12", "t0", "256"))
      "vfadd.vv v4, v4, v24\n"
      "vfadd.vv v4, v4, v28\n"
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_IO_ASM(
          "v8", "v12", "v16", "v20",
          STORE_ADVANCE_ASM("v24", "t2", "256"),
          STORE_ADVANCE_ASM("v28", "t2", "256"),
          LOAD_ADVANCE_ASM("v24", "t0", "256"),
          LOAD_ADVANCE_ASM("v28", "t0", "256"))
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v4, v4, v12\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      // Final odd pair: store the preceding even pair in its dependency gaps.
      "vfsub.vv v24, v24, v0\n"
      "vfsub.vv v28, v28, v0\n"
      SOFTMAX_EXP_PAIR_IO_ASM(
          "v24", "v28", "v16", "v20",
          STORE_ADVANCE_ASM("v8", "t2", "256"),
          STORE_ADVANCE_ASM("v12", "t2", "256"), "", "")
      "vfadd.vv v4, v4, v24\n"
      "vfadd.vv v4, v4, v28\n"
      STORE_ADVANCE_ASM("v24", "t2", "256")
      STORE_ADVANCE_ASM("v28", "t2", "256")
      RECIP_ASM("v4", "v0", "v16")
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[pairs]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      "2:\n"
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0")
      STORE_ADVANCE_ASM("v8", "t2", "256")
      "addi t1, t1, -1\n"
      "beqz t1, 3f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      NORMALIZE_ASM("v12", "v0")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "j 2b\n"
      "3:\n"
      NORMALIZE_ASM("v12", "v0")
      "vse32.v v12, (t2)\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [steady] "r"(steady),
        [pairs] "r"(pairs), [vl] "r"((uintptr_t)64),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

// Two independent column-sum chains remove the loop-carried add dependency.
// The extra final add combines even- and odd-row partial sums before RECIP.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_2sum(
    float *buf, uint32_t rows, const float *colmax) {
  const uintptr_t pairs = rows / 2;
  const uintptr_t quartets = rows / 4;
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v4, 0\n"
      "vmv.v.i v24, 0\n"
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[pairs]\n"
      "1:\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      SOFTMAX_EXP_PAIR_ASM("v8", "v12", "v16", "v20")
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v24, v24, v12\n"
      STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      "vfadd.vv v4, v4, v24\n"
      RECIP_ASM("v4", "v0", "v16")
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[quartets]\n"
      "2:\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0")
      LOAD_ADVANCE_ASM("v16", "t0", "256")
      STORE_ADVANCE_ASM("v8", "t2", "256")
      NORMALIZE_ASM("v12", "v0")
      LOAD_ADVANCE_ASM("v20", "t0", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      NORMALIZE_ASM("v16", "v0")
      STORE_ADVANCE_ASM("v16", "t2", "256")
      NORMALIZE_ASM("v20", "v0")
      STORE_ADVANCE_ASM("v20", "t2", "256")
      "addi t1, t1, -1\n"
      "bnez t1, 2b\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [pairs] "r"(pairs),
        [quartets] "r"(quartets), [vl] "r"((uintptr_t)64),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "memory");
}

// Same numerical operations as softmax_cols_rows_premax, scheduled as one
// e32,m8 vector per two contiguous rows. v0 keeps two independent partial
// sums (lower/upper row positions); they are combined once after all pairs.
// This changes only vector-register layout and issue scheduling.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_m8(
    float *buf, uint32_t rows, float *colmax) {
  const uintptr_t pairs = rows / 2;
  asm volatile(
      // Duplicate the 64-lane maximum so an m8 row pair can subtract it.
      "vsetvli zero, %[half_vl], e32, m4, ta, ma\n"
      "vle32.v v24, (%[max])\n"
      "addi t2, %[max], 256\n"
      "vse32.v v24, (%[max])\n"
      "vse32.v v24, (t2)\n"
      "vsetvli zero, %[pair_vl], e32, m8, ta, ma\n"
      "vle32.v v24, (%[max])\n"
      "vmv.v.i v0, 0\n"
      "mv t0, %[buf]\n"
      "mv t1, %[pairs]\n"
      "1:\n"
      "vle32.v v8, (t0)\n"
      "vfsub.vv v8, v8, v24\n"
      SOFTMAX_EXP_ASM("v8", "v16")
      "vfadd.vv v0, v0, v8\n"
      "vse32.v v8, (t0)\n"
      "addi t0, t0, 512\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      // Add the upper 64-lane partial sum into the lower half.
      "li t1, 64\n"
      "vslidedown.vx v8, v0, t1\n"
      "vsetvli zero, %[half_vl], e32, m4, ta, ma\n"
      "vfadd.vv v0, v0, v8\n"
      RECIP_ASM("v0", "v4", "v8")
      // Duplicate reciprocal for the final m8 pairwise normalization pass.
      "vse32.v v4, (%[max])\n"
      "vse32.v v4, (t2)\n"
      "vsetvli zero, %[pair_vl], e32, m8, ta, ma\n"
      "vle32.v v24, (%[max])\n"
      "mv t0, %[buf]\n"
      "mv t1, %[pairs]\n"
      "2:\n"
      "vle32.v v8, (t0)\n"
      "vfmul.vv v8, v8, v24\n"
      "vse32.v v8, (t0)\n"
      "addi t0, t0, 512\n"
      "addi t1, t1, -1\n"
      "bnez t1, 2b\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [pairs] "r"(pairs),
        [half_vl] "r"((uintptr_t)64), [pair_vl] "r"((uintptr_t)128),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

// Degree-5 exp with three independent m4 dependency chains. This preserves
// row-order accumulation exactly while spacing every dependent FP operation
// by two unrelated vector instructions.
static inline __attribute__((always_inline)) void softmax_cols_rows_premax_m3(
    float *buf, uint32_t rows, const float *colmax) {
  const uintptr_t triples = rows / 3;
  const uintptr_t pairs = rows / 2;
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[max])\n"
      "vmv.v.i v4, 0\n"
      "mv t0, %[buf]\n"
      "mv t1, %[triples]\n"
      "1:\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      LOAD_ADVANCE_ASM("v16", "t0", "256")
      "vfsub.vv v8, v8, v0\n"
      "vfsub.vv v12, v12, v0\n"
      "vfsub.vv v16, v16, v0\n"
      SOFTMAX_EXP_TRIPLE_ASM("v8", "v12", "v16", "v20", "v24", "v28")
      "vfadd.vv v4, v4, v8\n"
      "vfadd.vv v4, v4, v12\n"
      "vfadd.vv v4, v4, v16\n"
      "addi t2, t0, -768\n"
      STORE_ADVANCE_ASM("v8", "t2", "256")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "vse32.v v16, (t2)\n"
      "addi t1, t1, -1\n"
      "bnez t1, 1b\n"
      RECIP_ASM("v4", "v0", "v20")
      "mv t0, %[buf]\n"
      "mv t2, %[buf]\n"
      "mv t1, %[pairs]\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      "2:\n"
      LOAD_ADVANCE_ASM("v12", "t0", "256")
      NORMALIZE_ASM("v8", "v0")
      STORE_ADVANCE_ASM("v8", "t2", "256")
      "addi t1, t1, -1\n"
      "beqz t1, 3f\n"
      LOAD_ADVANCE_ASM("v8", "t0", "256")
      NORMALIZE_ASM("v12", "v0")
      STORE_ADVANCE_ASM("v12", "t2", "256")
      "j 2b\n"
      "3:\n"
      NORMALIZE_ASM("v12", "v0")
      "vse32.v v12, (t2)\n"
      :
      : [buf] "r"(buf), [max] "r"(colmax), [triples] "r"(triples),
        [pairs] "r"(pairs), [vl] "r"((uintptr_t)64),
        EXP_INPUTS, RECIP_INPUTS
      : "t0", "t1", "t2", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
        "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31", "memory");
}

static inline __attribute__((always_inline)) void softmax_cols(float *buf) {
  softmax_cols_rows(buf, 64);
}

// Narrow the FP32 row held in the LMUL=4 group x (vl elements starting at column q0) to FP16 and store it into
// row `row` of tiled dstw[4][R/2][16]. tmp: free LMUL=2 group. vtype must be e32/m4 on entry/exit.
// Caller preloads 64 uint16 byte offsets in v26/v27; T must be 64.
// Each FP16 row goes straight to the next GEMM's tile layout. v26/v27
// are reserved across the producer and do not overlap math or narrow scratch.
static __attribute__((always_inline)) inline void transformer_load_tile_offsets(const uint16_t *offsets) {
  asm volatile("vsetvli zero, %0, e16, m2, ta, ma\nvle16.v v26, (%1)"
    ::"r"(64),"r"(offsets):"v26","v27","memory");
}

// Build the same 64 lane byte-offset vector without a TCDM lookup table.
// rows must be 64 or 128; each 16-column tile advances rows/2 words.
static __attribute__((always_inline)) inline void transformer_make_tile_offsets(
    unsigned int rows) {
  uintptr_t tile_shift = rows == 64 ? 11 : 12;
  asm volatile(
      "vsetvli zero, %[vl], e16, m2, ta, ma\n"
      "vid.v v24\n"
      "vand.vi v26, v24, 15\n"
      "vsll.vi v26, v26, 2\n"
      "vsrl.vi v24, v24, 4\n"
      "vsll.vx v24, v24, %[shift]\n"
      "vadd.vv v26, v26, v24"
      :: [vl] "r"((uintptr_t)64), [shift] "r"(tile_shift)
      : "v24", "v25", "v26", "v27", "memory");
}
#define NARROW_STORE(x, tmp, dstw, row, T, q0, vl) \
  do { \
    asm volatile("vsetvli zero, %0, e16, m2, ta, ma" ::"r"(vl)); \
    asm volatile("vfncvt.f.f.w " tmp ", " x); \
    asm volatile("vsuxei16.v " tmp ", (%0), v26" \
      ::"r"((uint16_t *)((dstw) + ((row) >> 1) * 16) + ((row) & 1)) : "memory"); \
    wait_for_vector_store(dstw); \
    asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(vl)); \
  } while (0)

// LayerNorm over the C rows of every column (FP32), output narrowed + packed: dstw[C/2][T]
static __attribute__((always_inline)) inline void transformer_layernorm_pack(uint32_t *dstw, const float *in, const float *gamma, const float *beta,
                              unsigned int C, float inv_c, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = vsetvl_e32_m4(T - q0);
    asm volatile("vmv.v.i v0, 0");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(in + c * T + q0) : "memory");
      asm volatile("vfadd.vv v0, v0, v4");
    }
    asm volatile("vfmul.vf v0, v0, %0" ::"f"(inv_c));
    asm volatile("vmv.v.i v8, 0");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(in + c * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      asm volatile("vfmul.vv v4, v4, v4");
      asm volatile("vfadd.vv v8, v8, v4");
    }
    asm volatile("vfmul.vf v8, v8, %0" ::"f"(inv_c));
    asm volatile("vfadd.vf v8, v8, %0" ::"f"(1e-5f));
    RSQRT("v8", "v12", "v16", "v20");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(in + c * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      asm volatile("vfmul.vv v4, v4, v12");
      asm volatile("vfmul.vf v4, v4, %0" ::"f"(gamma[c]));
      asm volatile("vfadd.vf v4, v4, %0" ::"f"(beta[c]));
      NARROW_STORE("v4", "v24", dstw, c, T, q0, vl);
    }
  }
}

// Non-affine LayerNorm for weights/biases with gamma and beta folded offline.
static __attribute__((always_inline)) inline void transformer_layernorm_na_pack(
    uint32_t *dstw, const float *in, unsigned int C, float inv_c,
    unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = vsetvl_e32_m4(T - q0);
    asm volatile("vmv.v.i v0, 0");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" :: "r"(in + c * T + q0) : "memory");
      asm volatile("vfadd.vv v0, v0, v4");
    }
    asm volatile("vfmul.vf v0, v0, %0" :: "f"(inv_c));
    asm volatile("vmv.v.i v8, 0");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" :: "r"(in + c * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0\nvfmul.vv v4, v4, v4\nvfadd.vv v8, v8, v4");
    }
    asm volatile("vfmul.vf v8, v8, %0" :: "f"(inv_c));
    asm volatile("vfadd.vf v8, v8, %0" :: "f"(1e-5f));
    RSQRT("v8", "v12", "v16", "v20");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" :: "r"(in + c * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0\nvfmul.vv v4, v4, v12");
      NARROW_STORE("v4", "v24", dstw, c, T, q0, vl);
    }
  }
}

// In-place-free softmax over the R rows of every column of buf[R][T] after multiplying by `scale`
// (FP32; optional causal mask: row r masked for the columns t < r). buf keeps the un-normalised
// exponentials; the normalised probabilities are narrowed + packed into dstw[R/2][T].
static __attribute__((always_inline)) inline void transformer_softmax_pack(uint32_t *dstw, float *buf, unsigned int R, unsigned int T, float scale,
                            int causal) {
  if (causal) {
    wait_for_vector_store(buf);
    for (unsigned int r = 1; r < R; r++)
      for (unsigned int t = 0; t < r && t < T; t++) buf[r * T + t] = -1e30f;
    wait_for_vector_store(buf);
  }
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = vsetvl_e32_m4(T - q0);
    asm volatile("vfmv.v.f v0, %0" ::"f"(-1e30f));
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmax.vv v0, v0, v4");
    }
    asm volatile("vmv.v.i v28, 0");
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      asm volatile("vfmul.vf v4, v4, %0" ::"f"(scale));
      EXP("v4", "v8");
      asm volatile("vse32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfadd.vv v28, v28, v4");
    }
    RECIP("v28", "v12", "v16");
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmul.vv v4, v4, v12");
      NARROW_STORE("v4", "v24", dstw, r, T, q0, vl);
    }
  }
}

#endif  // TILE_PRODUCERS_H
