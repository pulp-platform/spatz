// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "ope_transformer.h"
static inline unsigned pipeline_cycle(void) { unsigned c; asm volatile("csrr %0, mcycle" : "=r"(c) :: "memory"); return c; }
#ifndef benchmark_get_cycle
#define benchmark_get_cycle pipeline_cycle
#endif
#ifndef start_kernel
#define start_kernel() ((void)0)
#endif
#ifndef stop_kernel
#define stop_kernel() ((void)0)
#endif
#define PG_CFG32() asm volatile("msetmtypei 1, 2, 0\nmsettn zero, %0\nmsettm zero, %0" :: "r"(16u) : "memory")
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
