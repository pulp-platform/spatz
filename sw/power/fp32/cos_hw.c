/*  cos_hw.c – Hardware cos via vfcos.v burst pattern
 *
 *  Algorithm:  4-way unrolled load → vfcos.v → store
 *  The HW instruction computes cos(x) in the non-linear pipeline.
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

/* ========================= LMUL = 8 =========================
 * Groups: v0, v8, v16, v24   (4 groups of 8 = 32 regs)
 */
static inline void vcos_hw_m8(const float *inp, float *out, int N) {
    size_t vl;
    asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(vl));
    int iters = N / (int)(4u * vl);
    for (int i = 0; i < iters; i++) {
        asm volatile("vle32.v v0,  (%0)" :: "r"(inp)            : "memory");
        asm volatile("vle32.v v8,  (%0)" :: "r"(inp +     vl)   : "memory");
        asm volatile("vle32.v v16, (%0)" :: "r"(inp + 2u*vl)    : "memory");
        asm volatile("vle32.v v24, (%0)" :: "r"(inp + 3u*vl)    : "memory");
        asm volatile("" ::: "memory");
        asm volatile("vfcos.v v0,  v0");
        asm volatile("vfcos.v v8,  v8");
        asm volatile("vfcos.v v16, v16");
        asm volatile("vfcos.v v24, v24");
        asm volatile("vse32.v v0,  (%0)" :: "r"(out)            : "memory");
        asm volatile("vse32.v v8,  (%0)" :: "r"(out +     vl)   : "memory");
        asm volatile("vse32.v v16, (%0)" :: "r"(out + 2u*vl)    : "memory");
        asm volatile("vse32.v v24, (%0)" :: "r"(out + 3u*vl)    : "memory");
        inp += 4u * vl; out += 4u * vl;
    }
    int rem = N - iters * (int)(4u * vl);
    while (rem > 0) {
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile("vle32.v v0, (%0)" :: "r"(inp) : "memory");
        asm volatile("vfcos.v v0, v0");
        asm volatile("vse32.v v0, (%0)" :: "r"(out) : "memory");
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vcos_hw_m4(const float *inp, float *out, int N) {
    size_t vl;
    asm volatile("vsetvli %0, zero, e32, m4, ta, ma" : "=r"(vl));
    int iters = N / (int)(4u * vl);
    for (int i = 0; i < iters; i++) {
        asm volatile("vle32.v v0,  (%0)" :: "r"(inp)           : "memory");
        asm volatile("vle32.v v4,  (%0)" :: "r"(inp +    vl)   : "memory");
        asm volatile("vle32.v v8,  (%0)" :: "r"(inp + 2u*vl)   : "memory");
        asm volatile("vle32.v v12, (%0)" :: "r"(inp + 3u*vl)   : "memory");
        asm volatile("" ::: "memory");
        asm volatile("vfcos.v v0,  v0");
        asm volatile("vfcos.v v4,  v4");
        asm volatile("vfcos.v v8,  v8");
        asm volatile("vfcos.v v12, v12");
        asm volatile("vse32.v v0,  (%0)" :: "r"(out)           : "memory");
        asm volatile("vse32.v v4,  (%0)" :: "r"(out +    vl)   : "memory");
        asm volatile("vse32.v v8,  (%0)" :: "r"(out + 2u*vl)   : "memory");
        asm volatile("vse32.v v12, (%0)" :: "r"(out + 3u*vl)   : "memory");
        inp += 4u * vl; out += 4u * vl;
    }
    int rem = N - iters * (int)(4u * vl);
    while (rem > 0) {
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile("vle32.v v0, (%0)" :: "r"(inp) : "memory");
        asm volatile("vfcos.v v0, v0");
        asm volatile("vse32.v v0, (%0)" :: "r"(out) : "memory");
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vcos_hw_m2(const float *inp, float *out, int N) {
    size_t vl;
    asm volatile("vsetvli %0, zero, e32, m2, ta, ma" : "=r"(vl));
    int iters = N / (int)(4u * vl);
    for (int i = 0; i < iters; i++) {
        asm volatile("vle32.v v0,  (%0)" :: "r"(inp)           : "memory");
        asm volatile("vle32.v v2,  (%0)" :: "r"(inp +    vl)   : "memory");
        asm volatile("vle32.v v4,  (%0)" :: "r"(inp + 2u*vl)   : "memory");
        asm volatile("vle32.v v6,  (%0)" :: "r"(inp + 3u*vl)   : "memory");
        asm volatile("" ::: "memory");
        asm volatile("vfcos.v v0,  v0");
        asm volatile("vfcos.v v2,  v2");
        asm volatile("vfcos.v v4,  v4");
        asm volatile("vfcos.v v6,  v6");
        asm volatile("vse32.v v0,  (%0)" :: "r"(out)           : "memory");
        asm volatile("vse32.v v2,  (%0)" :: "r"(out +    vl)   : "memory");
        asm volatile("vse32.v v4,  (%0)" :: "r"(out + 2u*vl)   : "memory");
        asm volatile("vse32.v v6,  (%0)" :: "r"(out + 3u*vl)   : "memory");
        inp += 4u * vl; out += 4u * vl;
    }
    int rem = N - iters * (int)(4u * vl);
    while (rem > 0) {
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile("vle32.v v0, (%0)" :: "r"(inp) : "memory");
        asm volatile("vfcos.v v0, v0");
        asm volatile("vse32.v v0, (%0)" :: "r"(out) : "memory");
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vcos_hw_m1(const float *inp, float *out, int N) {
    size_t vl;
    asm volatile("vsetvli %0, zero, e32, m1, ta, ma" : "=r"(vl));
    int iters = N / (int)(4u * vl);
    for (int i = 0; i < iters; i++) {
        asm volatile("vle32.v v0, (%0)" :: "r"(inp)           : "memory");
        asm volatile("vle32.v v1, (%0)" :: "r"(inp +    vl)   : "memory");
        asm volatile("vle32.v v2, (%0)" :: "r"(inp + 2u*vl)   : "memory");
        asm volatile("vle32.v v3, (%0)" :: "r"(inp + 3u*vl)   : "memory");
        asm volatile("" ::: "memory");
        asm volatile("vfcos.v v0, v0");
        asm volatile("vfcos.v v1, v1");
        asm volatile("vfcos.v v2, v2");
        asm volatile("vfcos.v v3, v3");
        asm volatile("vse32.v v0, (%0)" :: "r"(out)           : "memory");
        asm volatile("vse32.v v1, (%0)" :: "r"(out +    vl)   : "memory");
        asm volatile("vse32.v v2, (%0)" :: "r"(out + 2u*vl)   : "memory");
        asm volatile("vse32.v v3, (%0)" :: "r"(out + 3u*vl)   : "memory");
        inp += 4u * vl; out += 4u * vl;
    }
    int rem = N - iters * (int)(4u * vl);
    while (rem > 0) {
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile("vle32.v v0, (%0)" :: "r"(inp) : "memory");
        asm volatile("vfcos.v v0, v0");
        asm volatile("vse32.v v0, (%0)" :: "r"(out) : "memory");
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vcos_hw_kernel(const float *inp, float *out, int N) {
#if   LMUL_MODE == 8
    vcos_hw_m8(inp, out, N);
#elif LMUL_MODE == 4
    vcos_hw_m4(inp, out, N);
#elif LMUL_MODE == 2
    vcos_hw_m2(inp, out, N);
#elif LMUL_MODE == 1
    vcos_hw_m1(inp, out, N);
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

    if (len > 0) vcos_hw_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[COS_HW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_result(g_inp, g_out, outCos, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
