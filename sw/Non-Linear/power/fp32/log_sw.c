/*  log.c – "Cheapest" fast ln(x) for FP32 power evaluation
 *
 *  Algorithm:  ln(x) ≈ float( int(x) - MAGIC_BIAS ) * SCALE_FACTOR
 *    MAGIC_BIAS   = 0x3F800000  (float 1.0 as integer)
 *    SCALE_FACTOR = ln(2) / 2^23 ≈ 8.26295829e-8
 *
 *  Compute instructions per element:  4  (vsub, vfcvt, vfmul, store)
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

static const unsigned int MAGIC_BIAS  = 0x3F800000u;
static const float        SCALE_FACTOR = 8.26295829e-8f;

/* ========================= LMUL = 8 =========================
 * v0 = load x (bits),  v8 = int(x) - bias,  v16 = float(v8),  v24 = result
 */
static inline void vlog_m8(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v         v0,  (%[pin])     \n\t"
            "vsub.vx         v8,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v16, v8            \n\t"
            "vfmul.vf        v24, v16, %[sc]    \n\t"
            "vse32.v         v24, (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [bias]"r"(MAGIC_BIAS), [sc]"f"(SCALE_FACTOR)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vlog_m4(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v         v0,  (%[pin])     \n\t"
            "vsub.vx         v4,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v8,  v4            \n\t"
            "vfmul.vf        v12, v8,  %[sc]    \n\t"
            "vse32.v         v12, (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [bias]"r"(MAGIC_BIAS), [sc]"f"(SCALE_FACTOR)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vlog_m2(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v         v0,  (%[pin])     \n\t"
            "vsub.vx         v2,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v4,  v2            \n\t"
            "vfmul.vf        v6,  v4,  %[sc]    \n\t"
            "vse32.v         v6,  (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [bias]"r"(MAGIC_BIAS), [sc]"f"(SCALE_FACTOR)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vlog_m1(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v         v0,  (%[pin])     \n\t"
            "vsub.vx         v1,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v2,  v1            \n\t"
            "vfmul.vf        v3,  v2,  %[sc]    \n\t"
            "vse32.v         v3,  (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [bias]"r"(MAGIC_BIAS), [sc]"f"(SCALE_FACTOR)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vlog_kernel(const float *inp, float *out, int N) {
#if   LMUL_MODE == 8
    vlog_m8(inp, out, N);
#elif LMUL_MODE == 4
    vlog_m4(inp, out, N);
#elif LMUL_MODE == 2
    vlog_m2(inp, out, N);
#elif LMUL_MODE == 1
    vlog_m1(inp, out, N);
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

    if (len > 0) vlog_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[LOG LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_result(g_inp, g_out, outL, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
