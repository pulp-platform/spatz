/*  RoPE baseline – Rotary Positional Embedding using SW sin/cos (RV64GCV)
 *
 *  Kernel:
 *    cos_t = cos(theta),  sin_t = sin(theta)   [SW Cody-Waite polynomial]
 *    out_even = x_even * cos_t - x_odd * sin_t
 *    out_odd  = x_even * sin_t + x_odd * cos_t
 *
 *  SW sin/cos: Cody-Waite range reduction + 2-term polynomial
 *    r = x - round(x/(π/2))·(π/2)
 *    c0 = 1 + C2·r²,  s0 = r·(1 + S3·r²)
 *    Quadrant blend for sin and cos via maskless integer logic.
 *
 *  LMUL = 4 (e32) → 8 register groups: v0,v4,v8,v12,v16,v20,v24,v28
 */

#include "snrt.h"
#include "printf.h"
/* -- data include (overridable via -DDATA_HEADER_PATH=...) -- */
#if defined(DATA_HEADER_PATH)
#define _DATA_STR2(x) #x
#define _DATA_STR(x)  _DATA_STR2(x)
#include _DATA_STR(DATA_HEADER_PATH)
#else
#include "../data/data.h"
#endif
#include "../benchmark/benchmark.c"

#include <math.h>

/* ---- Cody-Waite constants ---- */
static const float CW_C2      = -0.49670f;
static const float CW_S3      = -0.16605f;
static const float CW_INVPIO2 =  0.63661977f;
static const float CW_PIO2    =  1.57079632679489661923f;

/* ---- RoPE frequency base ---- */
static const float ROPE_BASE = 10000.0f;

/*
 * rope_baseline – single-pass SW sin/cos + rotation at LMUL=4.
 *
 * Register plan (peak usage in asm block):
 *   v0  : theta / z / sign_temp      v4  : c0 → cos_theta
 *   v8  : kf / b0_f / temp           v12 : s0 → sin_theta
 *   v16 : k (int) / temp             v20 : b0 / temp
 *   v24 : r / s0_copy → x_even       v28 : b1 / temp → x_odd
 *
 * After sin/cos: v4 = cos, v12 = sin.
 * Rotation uses v0 = x_even, v8 = x_odd, v16 = out_even, v20 = out_odd.
 */
static void rope_baseline(const float *theta, const float *x_even,
                           const float *x_odd, float *out_even,
                           float *out_odd, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));

        /* === Phase 1: Cody-Waite range reduction === */
        asm volatile(
            "vle32.v    v0,  (%[th])             \n\t"   /* theta */
            "vfmul.vf   v8,  v0,  %[INV]         \n\t"
            "vfcvt.x.f.v v16, v8                  \n\t"   /* k (int) */
            "vfcvt.f.x.v v8,  v16                 \n\t"   /* kf */
            "vmv.v.v    v24, v0                   \n\t"
            "vfnmsac.vf v24, %[PIO2], v8          \n\t"   /* r = θ-kf·π/2 */
            /* z = r² */
            "vfmul.vv   v0,  v24, v24             \n\t"   /* z in v0 */
            /* c0 = 1 + C2·z */
            "vfmv.v.f   v4,  %[ONE]               \n\t"
            "vfmacc.vf  v4,  %[C2], v0            \n\t"   /* c0 in v4 */
            /* s0 = r·(1 + S3·z) */
            "vfmv.v.f   v12, %[ONE]               \n\t"
            "vfmacc.vf  v12, %[S3], v0            \n\t"
            "vfmul.vv   v12, v12, v24             \n\t"   /* s0 in v12 */

            /* === quadrant extraction === */
            "vand.vi    v8,  v16, 3               \n\t"   /* q */
            "vand.vi    v20, v8,  1               \n\t"   /* b0 (int) */
            "vsrl.vi    v28, v8,  1               \n\t"
            "vand.vi    v28, v28, 1               \n\t"   /* b1 (int) */

            /* === SIN decode (compute first, preserve c0/s0) ===
             * sign_s = 1 - 2·b1
             * sin = sign_s · (s0 + b0·(c0 - s0))                    */
            "vadd.vv    v0,  v28, v28             \n\t"
            "vrsub.vi   v0,  v0,  1               \n\t"   /* sign_s int */
            "vfcvt.f.x.v v0,  v0                  \n\t"   /* sign_s float */
            "vfcvt.f.x.v v8,  v20                 \n\t"   /* b0 float */
            "vfsub.vv   v16, v4,  v12             \n\t"   /* c0 - s0 */
            "vmv.v.v    v24, v12                  \n\t"   /* save s0 */
            "vfmacc.vv  v24, v16, v8              \n\t"   /* s0+b0·(c0-s0) */
            "vfmul.vv   v24, v24, v0              \n\t"   /* sin_θ → v24 */

            /* === COS decode ===
             * sign_c = 1 - 2·(b0⊕b1)
             * cos = sign_c · (c0 + b0·(s0 - c0))                    */
            "vxor.vv    v0,  v20, v28             \n\t"   /* b0^b1 */
            "vadd.vv    v0,  v0,  v0              \n\t"
            "vrsub.vi   v0,  v0,  1               \n\t"
            "vfcvt.f.x.v v0,  v0                  \n\t"   /* sign_c */
            "vfsgnjn.vv v16, v16, v16             \n\t"   /* s0-c0 = -(c0-s0) */
            "vfmacc.vv  v4,  v16, v8              \n\t"   /* c0+b0·(s0-c0) */
            "vfmul.vv   v4,  v4,  v0              \n\t"   /* cos_θ → v4 */

            /* === Phase 2: rotation ===
             * sin_θ in v24, cos_θ in v4                              */
            "vle32.v    v0,  (%[xe])              \n\t"   /* x_even */
            "vle32.v    v8,  (%[xo])              \n\t"   /* x_odd  */
            /* out_even = x_even·cos - x_odd·sin */
            "vfmul.vv   v12, v0,  v4              \n\t"
            "vfnmsac.vv v12, v8,  v24             \n\t"   /* v12 = out_even */
            /* out_odd  = x_even·sin + x_odd·cos */
            "vfmul.vv   v16, v0,  v24             \n\t"
            "vfmacc.vv  v16, v8,  v4              \n\t"   /* v16 = out_odd  */
            "vse32.v    v12, (%[oe])              \n\t"
            "vse32.v    v16, (%[oo])              \n\t"
            :
            : [th]"r"(theta), [xe]"r"(x_even), [xo]"r"(x_odd),
              [oe]"r"(out_even), [oo]"r"(out_odd),
              [INV]"f"(CW_INVPIO2), [PIO2]"f"(CW_PIO2),
              [S3]"f"(CW_S3), [C2]"f"(CW_C2), [ONE]"f"(1.0f)
            : "memory"
        );
        theta    += vl; x_even += vl; x_odd += vl;
        out_even += vl; out_odd += vl; rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const float *out, int N) {
    for (int i = 0; i < N && i < 8; i++) {
        unsigned u; __builtin_memcpy(&u, &out[i], 4);
        printf("  out[%d] = 0x%08X\n", i, u);
    }
}

/* ---- globals ---- */
static float *g_theta, *g_x_even, *g_x_odd, *g_out_even, *g_out_odd;

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N    = B_Size * T_Size * C_Size;
    const int half = N / 2;

    if (cid == 0) {
        g_theta    = (float *)snrt_l1alloc(half * sizeof(float));
        g_x_even   = (float *)snrt_l1alloc(half * sizeof(float));
        g_x_odd    = (float *)snrt_l1alloc(half * sizeof(float));
        g_out_even = (float *)snrt_l1alloc(half * sizeof(float));
        g_out_odd  = (float *)snrt_l1alloc(half * sizeof(float));

        /* DMA bulk copy: data_signed lives in DRAM — avoid slow scalar reads */
        snrt_dma_start_1d(g_x_even, data_signed, half * sizeof(float));
        snrt_dma_start_1d(g_x_odd, data_signed + half, half * sizeof(float));
        snrt_dma_wait_all();

        /* theta = x_even * 0.5  (fast: both buffers are in L1) */
        for (int i = 0; i < half; i++)
            g_theta[i] = g_x_even[i] * 0.5f;
    }
    snrt_cluster_hw_barrier();

    const int start = (int)((int64_t)cid       * half / (int64_t)cores);
    const int end   = (int)((int64_t)(cid + 1) * half / (int64_t)cores);
    const int len   = end - start;

    snrt_cluster_hw_barrier();

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0)
        rope_baseline(g_theta + start, g_x_even + start, g_x_odd + start,
                      g_out_even + start, g_out_odd + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[RoPE BASELINE] cycles=%u  cores=%u  N=%d\n", cyc, cores, N);
        check_result(g_out_even, half);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
