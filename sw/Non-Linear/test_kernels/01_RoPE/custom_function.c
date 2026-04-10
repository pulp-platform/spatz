/*  RoPE custom – Rotary Positional Embedding using HW sin/cos instructions
 *
 *  Kernel:
 *    cos_t = vfcos.v(theta)    [single HW instruction]
 *    sin_t = vfsin.v(theta)    [single HW instruction]
 *    out_even = x_even * cos_t - x_odd * sin_t
 *    out_odd  = x_even * sin_t + x_odd * cos_t
 *
 *  LMUL = 4 (e32) → 8 register groups: v0,v4,v8,v12,v16,v20,v24,v28
 *  Uses only 7 groups — significantly lower register pressure than baseline.
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

/*
 * rope_custom – single-pass HW sin/cos + rotation at LMUL=4.
 *
 * Register plan:
 *   v0  : theta          v4  : cos(θ)   [vfcos.v]
 *   v8  : sin(θ)         v12 : x_even
 *   v16 : x_odd          v20 : out_even
 *   v24 : out_odd
 */
static void rope_custom(const float *theta, const float *x_even,
                         const float *x_odd, float *out_even,
                         float *out_odd, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            /* compute sin/cos via HW non-linear pipeline */
            "vle32.v    v0,  (%[th])              \n\t"
            "vfcos.v    v4,  v0                   \n\t"   /* cos(θ) */
            "vfsin.v    v8,  v0                   \n\t"   /* sin(θ) */
            /* load input pairs */
            "vle32.v    v12, (%[xe])              \n\t"   /* x_even */
            "vle32.v    v16, (%[xo])              \n\t"   /* x_odd  */
            /* out_even = x_even·cos - x_odd·sin */
            "vfmul.vv   v20, v12, v4              \n\t"
            "vfnmsac.vv v20, v16, v8              \n\t"
            /* out_odd  = x_even·sin + x_odd·cos */
            "vfmul.vv   v24, v12, v8              \n\t"
            "vfmacc.vv  v24, v16, v4              \n\t"
            "vse32.v    v20, (%[oe])              \n\t"
            "vse32.v    v24, (%[oo])              \n\t"
            :
            : [th]"r"(theta), [xe]"r"(x_even), [xo]"r"(x_odd),
              [oe]"r"(out_even), [oo]"r"(out_odd)
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
        rope_custom(g_theta + start, g_x_even + start, g_x_odd + start,
                    g_out_even + start, g_out_odd + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[RoPE CUSTOM] cycles=%u  cores=%u  N=%d\n", cyc, cores, N);
        check_result(g_out_even, half);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
