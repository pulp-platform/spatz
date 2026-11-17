#include <printf.h>
#include <snrt.h>
#include DATAHEADER
#include "runtime.h"
#include "kernel/attention.h"
#define PG_GEMM_ATTRIBUTES static __attribute__((always_inline)) inline

enum { D = 64, T = 96, BC = 32 };
// One exact online-softmax score block.  pw receives unnormalised FP16
// exp(score-m_new), which is consumed immediately by the matching PV block.
static __attribute__((always_inline)) inline void online_update(
    float *score, uint32_t *pw, float *o, float *mrow, float *lrow) {
  for (unsigned q0 = 0, vl; q0 < T; q0 += vl) {
    vl = vsetvl_e32_m4(T - q0);
    asm volatile("vfmv.v.f v0,%0" :: "f"(-1e30f));
    for (unsigned r = 0; r < BC; ++r) {
      asm volatile("vle32.v v4,(%0)\nvfmax.vv v0,v0,v4"
                   :: "r"(score + r * T + q0) : "v4", "memory");
    }
    // m_new=max(m_old, block_max), alpha=exp(m_old-m_new).
    asm volatile("vle32.v v16,(%0)\nvfmax.vv v24,v16,v0\nvfsub.vv v0,v16,v24"
                 :: "r"(mrow + q0) : "v16", "v17", "v18", "v19", "v24", "v25", "v26", "v27", "memory");
    asm volatile(SOFTMAX_EXP_ASM("v0", "v8")
                 :: EXP_INPUTS
                 : "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11", "memory");
    // Rescale the previous unnormalised output and running denominator.
    for (unsigned d = 0; d < D; ++d) {
      asm volatile("vle32.v v4,(%0)\nvfmul.vv v4,v4,v0\nvse32.v v4,(%0)"
                   :: "r"(o + d * T + q0) : "v4", "v5", "v6", "v7", "memory");
    }
    asm volatile("vle32.v v20,(%0)\nvfmul.vv v20,v20,v0\nvmv.v.i v28,0"
                 :: "r"(lrow + q0)
                 : "v20", "v21", "v22", "v23", "v28", "v29", "v30", "v31", "memory");
    for (unsigned r = 0; r < BC; ++r) {
      asm volatile("vle32.v v4,(%0)\nvfsub.vv v4,v4,v24"
                   :: "r"(score + r * T + q0) : "v4", "v5", "v6", "v7", "memory");
      asm volatile(SOFTMAX_EXP_ASM("v4", "v8")
                   :: EXP_INPUTS
                   : "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "memory");
      asm volatile("vfadd.vv v28,v28,v4" ::: "v28", "v29", "v30", "v31", "memory");
      asm volatile("vsetvli zero,%0,e16,m2,ta,ma\n"
                   "vfncvt.f.f.w v12,v4\n"
                   "vsse16.v v12,(%1),%2"
                   :: "r"(vl), "r"((uint16_t *)(pw + (r / 2) * T + q0) + (r & 1)), "r"(4)
                   : "v12", "v13", "memory");
      asm volatile("vsetvli zero,%0,e32,m4,ta,ma" :: "r"(vl) : "memory");
    }
    asm volatile("vfadd.vv v20,v20,v28\nvse32.v v24,(%0)\nvse32.v v20,(%1)"
                 :: "r"(mrow + q0), "r"(lrow + q0)
                 : "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory");
  }
  wait_for_vector_store(pw);
}

static __attribute__((noinline)) void online_prepare(uint32_t *pw) {
  // P block 1 temporarily holds m/l before P block 0 has been consumed.
  float *mrow = (float *)(pw + (BC / 2) * T);
  float *lrow = mrow + T;
  for (unsigned i = 0; i < T; ++i) { mrow[i] = -1e30f; lrow[i] = 0.0f; }
}

static __attribute__((noinline)) void online_attention(
    uint32_t *q, uint32_t *k, uint32_t *v, float *s, uint32_t *pw, float *o) {
  const unsigned p_words = (BC / 2) * T;
  float *mrow = (float *)(pw + p_words);
  float *lrow = mrow + T;

  for (unsigned b = 0; b < T / BC; ++b) {
    uint32_t *kb = pw + b * p_words;
    // P occupies 6 KiB/block whereas packed K occupies 4 KiB/block.  Pack
    // only the current K panel, then overwrite that same P block after QK.
    for (unsigned kp = 0; kp < D / 2; ++kp)
      for (unsigned r = 0; r < BC; ++r)
        kb[kp * BC + r] = k[kp * T + b * BC + r];
    float *sb = s + b * BC * T;
    // C already contains the corresponding causal-mask rows from DMA.
    pipeline_gemm(kb, q, sb, BC, D, T, 0);
    online_update(sb, kb, o, mrow, lrow);
    // V is already pair-major [K/2][D], hence one K32 block is contiguous.
    pipeline_gemm(v + b * (BC / 2) * D, kb, o, D, BC, T, 0);
    if (b == 0) {
      // P0 is dead after PV0.  Move persistent state out of P1 before P1 is
      // used for K1/P1, still entirely within the existing p allocation.
      float *next_m = (float *)pw;
      float *next_l = next_m + T;
      for (unsigned i = 0; i < T; ++i) { next_m[i] = mrow[i]; next_l[i] = lrow[i]; }
      mrow = next_m;
      lrow = next_l;
    }
  }

  for (unsigned q0 = 0, vl; q0 < T; q0 += vl) {
    vl = vsetvl_e32_m4(T - q0);
    asm volatile("vle32.v v8,(%0)" :: "r"(lrow + q0) : "v8", "v9", "v10", "v11", "memory");
    asm volatile(RECIP_ASM("v8", "v12", "v16")
                 :: RECIP_INPUTS
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "memory");
    for (unsigned d = 0; d < D; ++d)
      asm volatile("vle32.v v4,(%0)\nvfmul.vv v4,v4,v12\nvse32.v v4,(%0)"
                   :: "r"(o + d * T + q0) : "v4", "v5", "v6", "v7", "memory");
  }
}

int main(void) {
  if (snrt_cluster_core_idx()) return 0;
  enum { QB = 12288, KB = 12288, VB = 12288, SB = 36864, PB = 18432, OB = 24576, TOTAL = 116736 };
  uint8_t *mem = snrt_l1alloc(TOTAL);
  uint32_t *q = (uint32_t *)mem; mem += QB;
  uint32_t *k = (uint32_t *)mem; mem += KB;
  uint32_t *v = (uint32_t *)mem; mem += VB;
  float *s = (float *)mem; mem += SB;
  uint32_t *pw = (uint32_t *)mem; mem += PB;
  float *o = (float *)mem;
  snrt_dma_start_1d(q, d_q, QB); snrt_dma_start_1d(k, d_k, KB);
  snrt_dma_start_1d(v, d_v, VB); snrt_dma_start_1d(s, d_mask, SB);
  for (unsigned i = 0; i < OB / 4; ++i) o[i] = 0;
  snrt_dma_wait_all();
  // Offline layout conversion and online-state initialization are outside
  // the timed kernel, matching the established application methodology.
  online_prepare(pw);
  unsigned start = get_cycle();
  online_attention(q, k, v, s, pw, o);
  unsigned cycles = get_cycle() - start;
  unsigned errors = transformer_check_fp32(o, (const float *)d_online_golden,
                                    D * T, 1e-3f, 1e-3f);
  printf("P8 attention online-fused cycles=%u errors=%u %s\n", cycles, errors, errors ? "FAIL" : "PASS");
  return errors ? 1 : 0;
}
