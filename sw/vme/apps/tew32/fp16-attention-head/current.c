#include <printf.h>
#include <snrt.h>
#include DATAHEADER
#include "runtime.h"
#include "kernel/attention.h"
#ifndef APP_FUSED
#define APP_FUSED 1
#endif
#define PG_GEMM_ATTRIBUTES static __attribute__((always_inline)) inline
static __attribute__((noinline)) void softmax_pack96(uint32_t *p, float *s) {
  for (unsigned q0 = 0, vl; q0 < 96; q0 += vl) {
    vl = vsetvl_e32_m4(96 - q0);
    asm volatile("vfmv.v.f v0,%0" :: "f"(-1e30f));
    for (unsigned r = 0; r < 96; r++) {
      asm volatile("vle32.v v4,(%0)" :: "r"(s + r * 96 + q0) : "memory");
      asm volatile("vfmax.vv v0,v0,v4");
    }
    asm volatile("vmv.v.i v28,0");
    for (unsigned r = 0; r < 96; r++) {
      asm volatile("vle32.v v4,(%0)\nvfsub.vv v4,v4,v0"
                   :: "r"(s + r * 96 + q0)
                   : "v4", "v5", "v6", "v7", "memory");
      asm volatile(SOFTMAX_EXP_ASM("v4", "v8")
                   :: EXP_INPUTS
                   : "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "memory");
      asm volatile("vse32.v v4,(%0)\nvfadd.vv v28,v28,v4"
                   :: "r"(s + r * 96 + q0) : "memory");
    }
    asm volatile(RECIP_ASM("v28", "v12", "v16")
                 :: RECIP_INPUTS
                 : "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19",
                   "v28", "v29", "v30", "v31", "memory");
    for (unsigned r = 0; r < 96; r++) {
      asm volatile("vle32.v v4,(%0)\nvfmul.vv v4,v4,v12"
                   :: "r"(s + r * 96 + q0)
                   : "v4", "v5", "v6", "v7", "memory");
      asm volatile("vsetvli zero,%0,e16,m2,ta,ma\n"
                   "vfncvt.f.f.w v24,v4\n"
                   "vsse16.v v24,(%1),%2"
                   :: "r"(vl),
                      "r"((uint16_t *)(p + (r / 2) * 96 + q0) + (r & 1)),
                      "r"(4)
                   : "v24", "v25", "memory");
      asm volatile("vsetvli zero,%0,e32,m4,ta,ma" :: "r"(vl) : "memory");
    }
  }
  wait_for_vector_store(p);
}
static __attribute__((noinline)) void attention(uint32_t*k,uint32_t*q,uint32_t*v,float*s,uint32_t*p,float*o){
  pipeline_gemm(k,q,s,96,64,96,0);
#if !APP_FUSED
  wait_for_vector_store(s);
#endif
  softmax_pack96(p,s);
#if !APP_FUSED
  wait_for_vector_store(p);
#endif
  pipeline_gemm(v,p,o,64,96,96,0);
}
int main(void){if(snrt_cluster_core_idx())return 0;enum{QB=12288,KB=12288,VB=12288,SB=36864,PB=18432,OB=24576,TOTAL=116736};uint8_t*m=snrt_l1alloc(TOTAL);uint32_t*q=(uint32_t*)m;m+=QB;uint32_t*k=(uint32_t*)m;m+=KB;uint32_t*v=(uint32_t*)m;m+=VB;float*s=(float*)m;m+=SB;uint32_t*p=(uint32_t*)m;m+=PB;float*o=(float*)m;snrt_dma_start_1d(q,d_q,QB);snrt_dma_start_1d(k,d_k,KB);snrt_dma_start_1d(v,d_v,VB);snrt_dma_start_1d(s,d_mask,SB);for(unsigned i=0;i<OB/4;i++)o[i]=0;snrt_dma_wait_all();unsigned st=get_cycle();attention(k,q,v,s,p,o);unsigned cycles=get_cycle()-st,err=0;for(unsigned i=0;i<64*96;i++){union{uint32_t u;float f;}r={d_golden[i]};float d=o[i]-r.f;if(d<0)d=-d;if(!(d<=.001f+.001f*(r.f<0?-r.f:r.f)))err++;}printf("P8 attention %s cycles=%u errors=%u %s\n",APP_FUSED?"fused":"non-fused",cycles,err,err?"FAIL":"PASS");return err?1:0;}
