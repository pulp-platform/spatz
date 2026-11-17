#pragma once
#include <stdint.h>
#include "runtime.h"

void ln_gemm_fused(const float *x, const uint32_t *weights, float *out,
                   uint32_t *packed, uint32_t M, uint32_t N, uint32_t K);
void layernorm_pack_non_fused(const float *x, uint32_t *packed,
                              uint32_t N, uint32_t K);
void gemm_after_ln_non_fused(const float *dead_x, const uint32_t *weights,
                             float *out, uint32_t *packed,
                             uint32_t M, uint32_t N, uint32_t K);
void ln_gemm_mt4x1_cache(const float *x, const uint32_t *weights, float *out,
                         uint32_t *packed, uint32_t M, uint32_t N,
                         uint32_t K);
extern uint32_t ln_cache_stats_cycles;
extern uint32_t ln_cache_producer_cycles;
extern uint32_t ln_cache_consumer_cycles;
extern uint32_t ln_cache_consumer_main_cycles;
extern uint32_t ln_cache_consumer_tail_cycles;
extern uint32_t ln_cache_tile_load_cycles;
extern uint32_t ln_cache_core_cycles;
extern uint32_t ln_cache_tile_store_cycles;

#include "transformer.h"

// FP32 LayerNorm, two-pass statistics over K channels for 64 tokens.
// On return v24 holds the mean and v8 holds the reciprocal standard
// deviation.  Keeping both vectors live avoids a TCDM round trip before the
// affine/packing pass.
static __attribute__((always_inline)) inline void ln_stats(
    const float *x, uint32_t N, uint32_t K, float inv_k) {

 // Two independent accumulators break the loop-carried FP add dependency.
 asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n"
              "vmv.v.i v24, 0\nvmv.v.i v28, 0"
   :: "r"(64):"memory");
 for(unsigned c=0;c<K;c+=2) {
  asm volatile("vle32.v v0, (%0)\nvle32.v v4, (%1)\n"
               "vfadd.vv v24, v24, v0\nvfadd.vv v28, v28, v4"
   ::"r"(x+c*N),"r"(x+(c+1)*N):"memory");
 }
 asm volatile("vfadd.vv v24, v24, v28\n"
              "vfmul.vf v24, v24, %0\n"
              "vmv.v.i v20, 0\nvmv.v.i v28, 0"
  ::"f"(inv_k):"memory");
 for(unsigned c=0;c<K;c+=2) {
  asm volatile("vle32.v v0, (%0)\nvle32.v v4, (%1)\n"
               "vfsub.vv v0, v0, v24\nvfsub.vv v4, v4, v24\n"
               "vfmul.vv v0, v0, v0\nvfmul.vv v4, v4, v4\n"
               "vfadd.vv v20, v20, v0\nvfadd.vv v28, v28, v4"
   ::"r"(x+c*N),"r"(x+(c+1)*N):"memory");
 }
 asm volatile("vfadd.vv v28, v20, v28\n"
              "vfmul.vf v28, v28, %[scale]\n"
              "vfadd.vf v28, v28, %[eps]\n"
  RSQRT_ASM("v28","v8","v12","v16")
  ::[scale]"f"(inv_k),[eps]"f"(1e-5f),RSQRT_INPUTS :"memory");
}

static __attribute__((always_inline)) inline void ln_stats_moment(
    const float *x, uint32_t N, uint32_t K, float inv_k) {
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma\n"
               "vmv.v.i v24, 0\nvmv.v.i v28, 0\n"
               "vmv.v.i v20, 0\nvmv.v.i v16, 0"
      :: "r"((uintptr_t)64) : "memory");
  for (unsigned c = 0; c < K; c += 2) {
    asm volatile("vle32.v v0, (%0)\n"
                 "vle32.v v4, (%1)\n"
                 "vfadd.vv v24, v24, v0\n"
                 "vfadd.vv v28, v28, v4\n"
                 "vfmacc.vv v20, v0, v0\n"
                 "vfmacc.vv v16, v4, v4"
        :: "r"(x + c * N), "r"(x + (c + 1) * N) : "memory");
  }
  asm volatile("vfadd.vv v24, v24, v28\n"
               "vfadd.vv v28, v20, v16\n"
               "vfmul.vf v24, v24, %[scale]\n"
               "vfmul.vf v28, v28, %[scale]\n"
               "vfmul.vv v20, v24, v24\n"
               "vfsub.vv v28, v28, v20\n"
               "vfadd.vf v28, v28, %[eps]\n"
      RSQRT_ASM("v28", "v8", "v12", "v16")
      :: [scale] "f"(inv_k), [eps] "f"(1e-5f), RSQRT_INPUTS
      : "memory");
}


// Statistics for one 32-token half of the P1 activation.  Rows remain
// separated by the full token stride (64), while each vector operation only
// touches the selected half.  The arithmetic order matches ln_stats(): two
// channel accumulators followed by the same three-step reciprocal square root.
// On return v24 holds mean[32] and v8 holds inv_std[32].
static __attribute__((always_inline)) inline void ln_stats32(
    const float *x, uint32_t row_stride, uint32_t K, float inv_k) {
  asm volatile("vsetvli zero, %0, e32, m2, ta, ma\n"
               "vmv.v.i v24, 0\nvmv.v.i v28, 0"
      :: "r"((uintptr_t)32) : "memory");
  for (unsigned c = 0; c < K; c += 2) {
    asm volatile("vle32.v v0, (%0)\nvle32.v v4, (%1)\n"
                 "vfadd.vv v24, v24, v0\nvfadd.vv v28, v28, v4"
        :: "r"(x + c * row_stride),
           "r"(x + (c + 1) * row_stride) : "memory");
  }
  asm volatile("vfadd.vv v24, v24, v28\n"
               "vfmul.vf v24, v24, %0\n"
               "vmv.v.i v20, 0\nvmv.v.i v28, 0"
      :: "f"(inv_k) : "memory");
  for (unsigned c = 0; c < K; c += 2) {
    asm volatile("vle32.v v0, (%0)\nvle32.v v4, (%1)\n"
                 "vfsub.vv v0, v0, v24\nvfsub.vv v4, v4, v24\n"
                 "vfmul.vv v0, v0, v0\nvfmul.vv v4, v4, v4\n"
                 "vfadd.vv v20, v20, v0\nvfadd.vv v28, v28, v4"
        :: "r"(x + c * row_stride),
           "r"(x + (c + 1) * row_stride) : "memory");
  }
  asm volatile("vfadd.vv v28, v20, v28\n"
               "vfmul.vf v28, v28, %[scale]\n"
               "vfadd.vf v28, v28, %[eps]\n"
      RSQRT_ASM("v28", "v8", "v12", "v16")
      :: [scale] "f"(inv_k), [eps] "f"(1e-5f), RSQRT_INPUTS
      : "memory");
}

uint32_t ln_cache_stats_cycles;
uint32_t ln_cache_producer_cycles;
uint32_t ln_cache_consumer_cycles;
uint32_t ln_cache_consumer_main_cycles;
uint32_t ln_cache_consumer_tail_cycles;
uint32_t ln_cache_tile_load_cycles;
uint32_t ln_cache_core_cycles;
uint32_t ln_cache_tile_store_cycles;

static inline __attribute__((always_inline)) void ln_cache_tile_io(
    float *z, uint32_t N, uint32_t panel, int store) {
  const uintptr_t mt = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t vt = 0xC8, tn = TE, stride = N * sizeof(float);
  uintptr_t p0 = (uintptr_t)(z + panel * TE);
  uintptr_t p1 = (uintptr_t)(z + 16 * N + panel * TE);
  uintptr_t p2 = (uintptr_t)(z + 32 * N + panel * TE);
  uintptr_t p3 = (uintptr_t)(z + 48 * N + panel * TE);
  uintptr_t t0 = 0, t1 = 0x20000000u, t2 = 0x40000000u, t3 = 0x60000000u;
  for (unsigned r = 0; r < TE; ++r) {
    if (!store) {
      asm volatile("msetmtype %[mt], %[vt]\n"
                   "msettn zero, %[tn]\n"
                   "vtle32 %[t0], (%[p0])\nvtle32 %[t1], (%[p1])\n"
                   "vtle32 %[t2], (%[p2])\nvtle32 %[t3], (%[p3])"
          :: [p0] "r"(p0), [p1] "r"(p1), [p2] "r"(p2), [p3] "r"(p3),
             [t0] "r"(t0), [t1] "r"(t1), [t2] "r"(t2), [t3] "r"(t3),
             [mt] "r"(mt), [vt] "r"(vt), [tn] "r"(tn) : "memory");
    } else {
      asm volatile("msetmtype %[mt], %[vt]\n"
                   "msettn zero, %[tn]\n"
                   "vtse32 %[t0], (%[p0])\nvtse32 %[t1], (%[p1])\n"
                   "vtse32 %[t2], (%[p2])\nvtse32 %[t3], (%[p3])"
          :: [p0] "r"(p0), [p1] "r"(p1), [p2] "r"(p2), [p3] "r"(p3),
             [t0] "r"(t0), [t1] "r"(t1), [t2] "r"(t2), [t3] "r"(t3),
             [mt] "r"(mt), [vt] "r"(vt), [tn] "r"(tn) : "memory");
    }
    p0 += stride; p1 += stride; p2 += stride; p3 += stride;
    ++t0; ++t1; ++t2; ++t3;
  }
}

static inline __attribute__((always_inline)) void ln_pack_pair16(
    const float *xe, const float *xo, const float *mean,
    const float *inv_std, uint32_t *dst) {
  const uintptr_t vl = 16, stride = sizeof(uint32_t);
  asm volatile(
      "vsetvli zero, %[vl], e32, m2, ta, ma\n"
      "vle32.v v20, (%[xe])\nvle32.v v24, (%[xo])\n"
      "vle32.v v28, (%[mean])\nvle32.v v30, (%[inv])\n"
      "vfsub.vv v20, v20, v28\nvfsub.vv v24, v24, v28\n"
      "vfmul.vv v20, v20, v30\nvfmul.vv v24, v24, v30\n"
      "vsetvli zero, %[vl], e16, m1, ta, ma\n"
      "vfncvt.f.f.w v22, v20\nvfncvt.f.f.w v26, v24\n"
      "vsse16.v v22, (%[dst]), %[stride]\n"
      "addi t0, %[dst], 2\nvsse16.v v26, (t0), %[stride]"
      :: [vl] "r"(vl), [xe] "r"(xe), [xo] "r"(xo),
         [mean] "r"(mean), [inv] "r"(inv_std), [dst] "r"(dst),
         [stride] "r"(stride)
      : "t0", "v20", "v21", "v22", "v24", "v25", "v26",
        "v28", "v29", "v30", "v31", "memory");
}

static inline __attribute__((always_inline)) void ln_load_panel_stats(
    const float *mean, const float *inv_std) {
  asm volatile(
      "vsetvli zero, %[vl], e32, m1, ta, ma\n"
      "vle32.v v28, (%[mean])\n"
      "vle32.v v30, (%[inv])\n"
      "vfmul.vv v28, v28, v30\n"
      "vfsgnjn.vv v28, v28, v28"
      :: [vl] "r"((uintptr_t)16), [mean] "r"(mean), [inv] "r"(inv_std)
      : "v28", "v30", "memory");
}

// v28 and v30 keep the panel statistics resident while the four K pairs are
// normalized.  This removes six redundant statistics loads per K=8 group.
static inline __attribute__((always_inline)) void ln_pack_pair16_resident(
    const float *xe, const float *xo, uint32_t *dst) {
  asm volatile(
      "vsetvli zero, %[vl], e32, m2, ta, ma\n"
      "vle32.v v20, (%[xe])\n"
      "vle32.v v24, (%[xo])\n"
      "vfmadd.vv v20, v30, v28\n"
      "vfmadd.vv v24, v30, v28\n"
      "vsetvli zero, %[vl], e16, m1, ta, ma\n"
      "vfncvt.f.f.w v22, v20\n"
      "vfncvt.f.f.w v26, v24\n"
      "vsse16.v v22, (%[dst]), %[stride]\n"
      "addi t0, %[dst], 2\n"
      "vsse16.v v26, (t0), %[stride]"
      :: [vl] "r"((uintptr_t)16), [xe] "r"(xe), [xo] "r"(xo),
         [dst] "r"(dst), [stride] "r"((uintptr_t)sizeof(uint32_t))
      : "t0", "v20", "v21", "v22", "v24", "v25", "v26", "memory");
}

static inline __attribute__((always_inline)) void ln_cache_load_operands(
    const uint32_t *w, uint32_t K, uint32_t pair, const uint32_t *bp) {
  const uint32_t *w0 = w + (0 * (K / 2) + pair) * TE;
  const uint32_t *w1 = w + (1 * (K / 2) + pair) * TE;
  const uint32_t *w2 = w + (2 * (K / 2) + pair) * TE;
  const uint32_t *w3 = w + (3 * (K / 2) + pair) * TE;
  const uint32_t *b1 = bp + TE;
  const uint32_t *b2 = bp + 2 * TE;
  const uint32_t *b3 = bp + 3 * TE;
  asm volatile(
      "vsetvli zero, %[gvl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[w0])\n"
      "vle32.v v4, (%[w1])\n"
      "vle32.v v8, (%[w2])\n"
      "vle32.v v12, (%[w3])\n"
      "vsetvli zero, %[vl], e32, m1, ta, ma\n"
      "vle32.v v16, (%[bp])\n"
      "vle32.v v17, (%[b1])\n"
      "vle32.v v18, (%[b2])\n"
      "vle32.v v19, (%[b3])"
      :: [gvl] "r"((uintptr_t)64), [vl] "r"((uintptr_t)16),
         [w0] "r"(w0), [w1] "r"(w1), [w2] "r"(w2), [w3] "r"(w3),
         [bp] "r"(bp), [b1] "r"(b1), [b2] "r"(b2), [b3] "r"(b3)
      : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
        "v16", "v17", "v18", "v19", "memory");
}

static inline __attribute__((always_inline)) void ln_cache_mac_tile(
    unsigned tile) {
  const uintptr_t mt = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t vt = 0xC8, tn = TE;
  switch (tile) {
    case 0:
      asm volatile("msetmtype %[mt], %[vt]\nmsettn zero, %[tn]\n"
                   "vtfmm.tvv mt0, v0, v16\nvtfmm.tvv mt0, v1, v17\n"
                   "vtfmm.tvv mt0, v2, v18\nvtfmm.tvv mt0, v3, v19"
          :: [mt] "r"(mt), [vt] "r"(vt), [tn] "r"(tn) : "memory");
      break;
    case 1:
      asm volatile("msetmtype %[mt], %[vt]\nmsettn zero, %[tn]\n"
                   "vtfmm.tvv mt4, v4, v16\nvtfmm.tvv mt4, v5, v17\n"
                   "vtfmm.tvv mt4, v6, v18\nvtfmm.tvv mt4, v7, v19"
          :: [mt] "r"(mt), [vt] "r"(vt), [tn] "r"(tn) : "memory");
      break;
    case 2:
      asm volatile("msetmtype %[mt], %[vt]\nmsettn zero, %[tn]\n"
                   "vtfmm.tvv mt8, v8, v16\nvtfmm.tvv mt8, v9, v17\n"
                   "vtfmm.tvv mt8, v10, v18\nvtfmm.tvv mt8, v11, v19"
          :: [mt] "r"(mt), [vt] "r"(vt), [tn] "r"(tn) : "memory");
      break;
    default:
      asm volatile("msetmtype %[mt], %[vt]\nmsettn zero, %[tn]\n"
                   "vtfmm.tvv mt12, v12, v16\nvtfmm.tvv mt12, v13, v17\n"
                   "vtfmm.tvv mt12, v14, v18\nvtfmm.tvv mt12, v15, v19"
          :: [mt] "r"(mt), [vt] "r"(vt), [tn] "r"(tn) : "memory");
      break;
  }
}

static inline __attribute__((always_inline)) void ln_cache_load_weight_tile(
    const uint32_t *w, uint32_t K, uint32_t pair, unsigned tile) {
  const uint32_t *wp = w + (tile * (K / 2) + pair) * TE;
  switch (tile) {
    case 0:
      asm volatile("vsetvli zero, %[vl], e32, m4, ta, ma\n"
                   "vle32.v v0, (%[wp])"
          :: [vl] "r"((uintptr_t)64), [wp] "r"(wp)
          : "v0", "v1", "v2", "v3", "memory");
      break;
    case 1:
      asm volatile("vsetvli zero, %[vl], e32, m4, ta, ma\n"
                   "vle32.v v4, (%[wp])"
          :: [vl] "r"((uintptr_t)64), [wp] "r"(wp)
          : "v4", "v5", "v6", "v7", "memory");
      break;
    case 2:
      asm volatile("vsetvli zero, %[vl], e32, m4, ta, ma\n"
                   "vle32.v v8, (%[wp])"
          :: [vl] "r"((uintptr_t)64), [wp] "r"(wp)
          : "v8", "v9", "v10", "v11", "memory");
      break;
    default:
      asm volatile("vsetvli zero, %[vl], e32, m4, ta, ma\n"
                   "vle32.v v12, (%[wp])"
          :: [vl] "r"((uintptr_t)64), [wp] "r"(wp)
          : "v12", "v13", "v14", "v15", "memory");
      break;
  }
}

static inline __attribute__((always_inline)) void ln_cache_mac_group(
    const uint32_t *w, uint32_t K, uint32_t pair, const uint32_t *bp) {
  const uintptr_t mt = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t vt = 0xC8, tn = TE;
  const uint32_t *w0 = w + (0 * (K / 2) + pair) * TE;
  const uint32_t *w1 = w + (1 * (K / 2) + pair) * TE;
  const uint32_t *w2 = w + (2 * (K / 2) + pair) * TE;
  const uint32_t *w3 = w + (3 * (K / 2) + pair) * TE;
  const uint32_t *b1 = bp + TE;
  const uint32_t *b2 = bp + 2 * TE;
  const uint32_t *b3 = bp + 3 * TE;
  asm volatile(
      "vsetvli zero, %[gvl], e32, m4, ta, ma\n"
      "vle32.v v0, (%[w0])\nvle32.v v4, (%[w1])\n"
      "vle32.v v8, (%[w2])\nvle32.v v12, (%[w3])\n"
      "vsetvli zero, %[vl], e32, m1, ta, ma\n"
      "vle32.v v16, (%[bp])\nvle32.v v17, (%[b1])\n"
      "vle32.v v18, (%[b2])\nvle32.v v19, (%[b3])\n"
      "msetmtype %[mt], %[vt]\nmsettn zero, %[tn]\n"
      "vtfmm.tvv mt0, v0, v16\nvtfmm.tvv mt0, v1, v17\n"
      "vtfmm.tvv mt0, v2, v18\nvtfmm.tvv mt0, v3, v19\n"
      "vtfmm.tvv mt4, v4, v16\nvtfmm.tvv mt4, v5, v17\n"
      "vtfmm.tvv mt4, v6, v18\nvtfmm.tvv mt4, v7, v19\n"
      "vtfmm.tvv mt8, v8, v16\nvtfmm.tvv mt8, v9, v17\n"
      "vtfmm.tvv mt8, v10, v18\nvtfmm.tvv mt8, v11, v19\n"
      "vtfmm.tvv mt12, v12, v16\nvtfmm.tvv mt12, v13, v17\n"
      "vtfmm.tvv mt12, v14, v18\nvtfmm.tvv mt12, v15, v19"
      :: [gvl] "r"((uintptr_t)64), [vl] "r"((uintptr_t)16),
         [w0] "r"(w0), [w1] "r"(w1), [w2] "r"(w2), [w3] "r"(w3),
         [bp] "r"(bp), [b1] "r"(b1), [b2] "r"(b2), [b3] "r"(b3),
         [mt] "r"(mt), [vt] "r"(vt), [tn] "r"(tn)
      : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
        "v16", "v17", "v18", "v19", "memory");
}

__attribute__((noinline, aligned(64))) void ln_gemm_mt4x1_cache(
    const float *x, const uint32_t *w, float *z, uint32_t *packed,
    uint32_t M, uint32_t N, uint32_t K) {
  (void)M;
  uint32_t stamp = get_cycle();
  ln_stats_moment(x, N, K, 0.010416666666666666f);
  float *stats = (float *)(packed + (K / 2) * N - 2 * N);
  asm volatile("vsetvli zero, %[vl], e32, m4, ta, ma\n"
               "vse32.v v24, (%[mean])\nvse32.v v8, (%[inv])"
      :: [vl] "r"((uintptr_t)64), [mean] "r"(stats),
         [inv] "r"(stats + N) : "memory");
  asm volatile("lw zero, 0(%0)" :: "r"(stats) : "memory");
  ln_cache_stats_cycles = get_cycle() - stamp;

  stamp = get_cycle();
  ln_cache_tile_load_cycles = 0;
  ln_cache_core_cycles = 0;
  ln_cache_tile_store_cycles = 0;
  for (uint32_t panel = 0; panel < 4; ++panel) {
    const float *panel_mean = stats + panel * TE;
    const float *panel_inv = stats + N + panel * TE;
    if (panel != 0) {
      // Panel-0 lanes are dead after its producer finishes.  Reuse two of
      // those channel rows so panel 3 cannot overwrite its live statistics
      // when the packed cache reaches the end of the workspace.
      float *dead_mean = (float *)x;
      float *dead_inv = (float *)x + N;
      asm volatile("vsetvli zero, %[vl], e32, m1, ta, ma\n"
                   "vle32.v v28, (%[mean])\nvle32.v v30, (%[inv])\n"
                   "vse32.v v28, (%[dead_mean])\nvse32.v v30, (%[dead_inv])"
          :: [vl] "r"((uintptr_t)16), [mean] "r"(panel_mean),
             [inv] "r"(panel_inv), [dead_mean] "r"(dead_mean),
             [dead_inv] "r"(dead_inv) : "v28", "v30", "memory");
      asm volatile("lw zero, 0(%0)" :: "r"(dead_mean) : "memory");
      panel_mean = dead_mean;
      panel_inv = dead_inv;
    }
    ln_load_panel_stats(panel_mean, panel_inv);
    uint32_t panel_stamp = get_cycle();
    ln_cache_tile_io(z, N, panel, 0);
    uint32_t core_stamp = get_cycle();
    ln_cache_tile_load_cycles += core_stamp - panel_stamp;
    uint32_t *bp = packed + panel * (K / 2) * TE;
    for (uint32_t p = 0; p < 4; ++p)
      ln_pack_pair16_resident(x + (2 * p) * N + panel * TE,
                              x + (2 * p + 1) * N + panel * TE,
                              bp + p * TE);
    ln_cache_load_operands(w, K, 0, bp);

    for (uint32_t kg = 0; kg < K; kg += 8) {
      const int have_next = kg + 8 < K;
      uint32_t *next_bp = bp + 4 * TE;
      for (uint32_t tile = 0; tile < 4; ++tile) {
        ln_cache_mac_tile(tile);
        if (have_next) {
          ln_pack_pair16_resident(
              x + (kg + 8 + 2 * tile) * N + panel * TE,
              x + (kg + 8 + 2 * tile + 1) * N + panel * TE,
              next_bp + tile * TE);
        }
      }
      if (have_next) {
        // VTFMM issue is decoupled from its eventual VRF read.  Keep the
        // current v0-v15 weights intact until all four tiles have issued;
        // the four normalization chains above provide the read-distance.
        ln_cache_load_operands(w, K, (kg + 8) / 2, next_bp);
        bp = next_bp;
      }
    }
    uint32_t store_stamp = get_cycle();
    ln_cache_core_cycles += store_stamp - core_stamp;
    ln_cache_tile_io(z, N, panel, 1);
    uint32_t panel_done = get_cycle();
    ln_cache_tile_store_cycles += panel_done - store_stamp;
  }
  asm volatile("lw zero, 0(%0)" :: "r"(z + 63 * N + 63) : "memory");
  ln_cache_producer_cycles = get_cycle() - stamp;

  stamp = get_cycle();
  gemm_fp16_fp32_unit_crossblock_ldc(
      (const __fp16 *)(w + 4 * (K / 2) * TE),
      (const __fp16 *)packed, z + 64 * N, 96, N, K, N);
  asm volatile("lw zero, 0(%0)" :: "r"(z + 159 * N + 63) : "memory");
  uint32_t tail_stamp = get_cycle();
  ln_cache_consumer_main_cycles = tail_stamp - stamp;
  matmul_tail_1x4((const __fp16 *)(w + 10 * (K / 2) * TE),
                  (const __fp16 *)packed, z + 160 * N,
                  z + 160 * N, N, K);
  uint32_t done = get_cycle();
  ln_cache_consumer_tail_cycles = done - tail_stamp;
  ln_cache_consumer_cycles = done - stamp;
}
