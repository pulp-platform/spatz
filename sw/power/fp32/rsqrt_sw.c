/*  rsqrt_sw.c – Quake III fast inverse sqrt + 2 Newton-Raphson iterations
 *
 *  Algorithm:
 *    seed  = reinterpret_float( MAGIC - (reinterpret_int(x) >> 1) )
 *    y = seed;  NR step:  y = y · (1.5 - 0.5·x·y²)     (repeated 2×)
 *
 *  MAGIC = 0x5F3759DFu
 *
 *  Compile with -DLMUL_MODE={1,2,4,8}
 */

#include "snrt.h"
#include "printf.h"
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"

#include <math.h>

#ifndef LMUL_MODE
#define LMUL_MODE 8
#endif

static const unsigned int RSQRT_MAGIC = 0x5F3759DFu;

/* ========================= LMUL = 8 =========================
 * v0  = x (kept),  v8  = y (seed→result),
 * v16 = temp,      v24 = half_x = 0.5·x (kept across NR)
 */
static inline void vrsqrt_sw_m8(const float *inp, float *out, int N) {
    float half = 0.5f, three_half = 1.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            /* seed */
            "vsrl.vi    v8,  v0,  1            \n\t"
            "vrsub.vx   v8,  v8,  %[M]         \n\t"
            /* half_x = 0.5 · x */
            "vfmul.vf   v24, v0,  %[H]         \n\t"
            /* NR iter 1 */
            "vfmul.vv   v16, v8,  v8            \n\t"   /* y² */
            "vfmul.vv   v16, v24, v16           \n\t"   /* 0.5·x·y² */
            "vfrsub.vf  v16, v16, %[T]          \n\t"   /* 1.5 - … */
            "vfmul.vv   v8,  v8,  v16           \n\t"   /* y *= factor */
            /* NR iter 2 */
            "vfmul.vv   v16, v8,  v8            \n\t"
            "vfmul.vv   v16, v24, v16           \n\t"
            "vfrsub.vf  v16, v16, %[T]          \n\t"
            "vfmul.vv   v8,  v8,  v16           \n\t"
            "vse32.v    v8,  (%[po])            \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(RSQRT_MAGIC), [H]"f"(half), [T]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vrsqrt_sw_m4(const float *inp, float *out, int N) {
    float half = 0.5f, three_half = 1.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            "vsrl.vi    v4,  v0,  1            \n\t"
            "vrsub.vx   v4,  v4,  %[M]         \n\t"
            "vfmul.vf   v12, v0,  %[H]         \n\t"
            /* NR 1 */
            "vfmul.vv   v8,  v4,  v4            \n\t"
            "vfmul.vv   v8,  v12, v8            \n\t"
            "vfrsub.vf  v8,  v8,  %[T]          \n\t"
            "vfmul.vv   v4,  v4,  v8            \n\t"
            /* NR 2 */
            "vfmul.vv   v8,  v4,  v4            \n\t"
            "vfmul.vv   v8,  v12, v8            \n\t"
            "vfrsub.vf  v8,  v8,  %[T]          \n\t"
            "vfmul.vv   v4,  v4,  v8            \n\t"
            "vse32.v    v4,  (%[po])            \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(RSQRT_MAGIC), [H]"f"(half), [T]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vrsqrt_sw_m2(const float *inp, float *out, int N) {
    float half = 0.5f, three_half = 1.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            "vsrl.vi    v2,  v0,  1            \n\t"
            "vrsub.vx   v2,  v2,  %[M]         \n\t"
            "vfmul.vf   v6,  v0,  %[H]         \n\t"
            /* NR 1 */
            "vfmul.vv   v4,  v2,  v2            \n\t"
            "vfmul.vv   v4,  v6,  v4            \n\t"
            "vfrsub.vf  v4,  v4,  %[T]          \n\t"
            "vfmul.vv   v2,  v2,  v4            \n\t"
            /* NR 2 */
            "vfmul.vv   v4,  v2,  v2            \n\t"
            "vfmul.vv   v4,  v6,  v4            \n\t"
            "vfrsub.vf  v4,  v4,  %[T]          \n\t"
            "vfmul.vv   v2,  v2,  v4            \n\t"
            "vse32.v    v2,  (%[po])            \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(RSQRT_MAGIC), [H]"f"(half), [T]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vrsqrt_sw_m1(const float *inp, float *out, int N) {
    float half = 0.5f, three_half = 1.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            "vsrl.vi    v1,  v0,  1            \n\t"
            "vrsub.vx   v1,  v1,  %[M]         \n\t"
            "vfmul.vf   v3,  v0,  %[H]         \n\t"
            /* NR 1 */
            "vfmul.vv   v2,  v1,  v1            \n\t"
            "vfmul.vv   v2,  v3,  v2            \n\t"
            "vfrsub.vf  v2,  v2,  %[T]          \n\t"
            "vfmul.vv   v1,  v1,  v2            \n\t"
            /* NR 2 */
            "vfmul.vv   v2,  v1,  v1            \n\t"
            "vfmul.vv   v2,  v3,  v2            \n\t"
            "vfrsub.vf  v2,  v2,  %[T]          \n\t"
            "vfmul.vv   v1,  v1,  v2            \n\t"
            "vse32.v    v1,  (%[po])            \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(RSQRT_MAGIC), [H]"f"(half), [T]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vrsqrt_sw_kernel(const float *inp, float *out, int N) {
#if   LMUL_MODE == 8
    vrsqrt_sw_m8(inp, out, N);
#elif LMUL_MODE == 4
    vrsqrt_sw_m4(inp, out, N);
#elif LMUL_MODE == 2
    vrsqrt_sw_m2(inp, out, N);
#elif LMUL_MODE == 1
    vrsqrt_sw_m1(inp, out, N);
#else
#   error "LMUL_MODE must be 1, 2, 4 or 8"
#endif
}

static void check_result(const float *input, const float *x, const float *ref, int r) {
    for (int i = 0; i < r; i++) {
        float diff = fabsf(x[i] - ref[i]);
        printf("At index %d:\t, value %f\t expected %f\t real %f\t error %f\n",
               i, input[i], ref[i], x[i], diff);
    }
}

static float *g_inp, *g_out;

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    if (cid == 0) {
        g_inp = (float *)snrt_l1alloc(N * sizeof(float));
        g_out = (float *)snrt_l1alloc(N * sizeof(float));
        snrt_dma_start_1d(g_inp, data_positive, N * sizeof(float));
        snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();

    const int start = (int)((int64_t)cid       * N / (int64_t)cores);
    const int end   = (int)((int64_t)(cid + 1) * N / (int64_t)cores);
    const int len   = end - start;

    snrt_cluster_hw_barrier();

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0) vrsqrt_sw_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[RSQRT_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_result(g_inp, g_out, outR, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
