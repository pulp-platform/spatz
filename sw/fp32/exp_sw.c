/*  exp.c – Schraudolph deg-0 fast exp(x) for FP32 power evaluation
 *
 *  Algorithm:  exp(x) ≈ reinterpret_float( uint32( B + C·x ) )
 *    where C = 2^23 / ln2 ≈ 12102203.0,  B ≈ 1064866805.0
 *
 *  Compute instructions per element:  vfmv + vfmacc + vfcvt = 3
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

/* ---- constants ---- */
static const float SCH_C = 12102203.0f;
static const float SCH_B = 1064866805.0f;

/* ========================= LMUL = 8 ========================= */
static inline void vexp_m8(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v16, v8           \n\t"
            "vse32.v   v16, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(SCH_B), [C]"f"(SCH_C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vexp_m4(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v4,  %[B]               \n\t"
            "vfmacc.vf v4,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v8, v4            \n\t"
            "vse32.v   v8,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(SCH_B), [C]"f"(SCH_C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vexp_m2(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v2,  %[B]               \n\t"
            "vfmacc.vf v2,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v4, v2            \n\t"
            "vse32.v   v4,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(SCH_B), [C]"f"(SCH_C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vexp_m1(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v1,  %[B]               \n\t"
            "vfmacc.vf v1,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v2, v1            \n\t"
            "vse32.v   v2,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(SCH_B), [C]"f"(SCH_C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ---- selector ---- */
static inline void vexp_kernel(const float *inp, float *out, int N) {
#if   LMUL_MODE == 8
    vexp_m8(inp, out, N);
#elif LMUL_MODE == 4
    vexp_m4(inp, out, N);
#elif LMUL_MODE == 2
    vexp_m2(inp, out, N);
#elif LMUL_MODE == 1
    vexp_m1(inp, out, N);
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

/* ---- globals ---- */
static float *g_inp, *g_out;

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    if (cid == 0) {
        g_inp = (float *)snrt_l1alloc(N * sizeof(float));
        g_out = (float *)snrt_l1alloc(N * sizeof(float));
        snrt_dma_start_1d(g_inp, data_signed, N * sizeof(float));
        snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();

    const int start = (int)((int64_t)cid       * N / (int64_t)cores);
    const int end   = (int)((int64_t)(cid + 1) * N / (int64_t)cores);
    const int len   = end - start;

    snrt_cluster_hw_barrier();

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0) vexp_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[EXP LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_result(g_inp, g_out, outE, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
