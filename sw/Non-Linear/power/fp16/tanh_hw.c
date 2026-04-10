/*  tanh_hw.c - Hardware tanh via vftanhf.v burst pattern (FP16)
 *  Compile with -DLMUL_MODE={1,2,4,8}
 */

#include "snrt.h"
#include "printf.h"
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"
#include <math.h>
#include <stdint.h>

#ifndef LMUL_MODE
#define LMUL_MODE 8
#endif

static inline void vtanh_hw_m8(const uint16_t *inp, uint16_t *out, int N) {
    size_t vl;
    asm volatile("vsetvli %0, zero, e16, m8, ta, ma" : "=r"(vl));
    int iters = N / (int)(4u * vl);
    for (int i = 0; i < iters; i++) {
        asm volatile("vle16.v  v0, (%0)" :: "r"(inp)           : "memory");
        asm volatile("vle16.v  v8, (%0)" :: "r"(inp +    vl)   : "memory");
        asm volatile("vle16.v  v16, (%0)" :: "r"(inp + 2u*vl)  : "memory");
        asm volatile("vle16.v  v24, (%0)" :: "r"(inp + 3u*vl)  : "memory");
        asm volatile("" ::: "memory");
        asm volatile("vftanhf.v v0, v0");
        asm volatile("vftanhf.v v8, v8");
        asm volatile("vftanhf.v v16, v16");
        asm volatile("vftanhf.v v24, v24");
        asm volatile("vse16.v  v0, (%0)" :: "r"(out)           : "memory");
        asm volatile("vse16.v  v8, (%0)" :: "r"(out +    vl)   : "memory");
        asm volatile("vse16.v  v16, (%0)" :: "r"(out + 2u*vl)  : "memory");
        asm volatile("vse16.v  v24, (%0)" :: "r"(out + 3u*vl)  : "memory");
        inp += 4u * vl; out += 4u * vl;
    }
    int rem = N - iters * (int)(4u * vl);
    while (rem > 0) {
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile("vle16.v  v0, (%0)" :: "r"(inp) : "memory");
        asm volatile("vftanhf.v v0, v0");
        asm volatile("vse16.v  v0, (%0)" :: "r"(out) : "memory");
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vtanh_hw_m4(const uint16_t *inp, uint16_t *out, int N) {
    size_t vl;
    asm volatile("vsetvli %0, zero, e16, m4, ta, ma" : "=r"(vl));
    int iters = N / (int)(4u * vl);
    for (int i = 0; i < iters; i++) {
        asm volatile("vle16.v  v0, (%0)" :: "r"(inp)           : "memory");
        asm volatile("vle16.v  v4, (%0)" :: "r"(inp +    vl)   : "memory");
        asm volatile("vle16.v  v8, (%0)" :: "r"(inp + 2u*vl)   : "memory");
        asm volatile("vle16.v  v12, (%0)" :: "r"(inp + 3u*vl)  : "memory");
        asm volatile("" ::: "memory");
        asm volatile("vftanhf.v v0, v0");
        asm volatile("vftanhf.v v4, v4");
        asm volatile("vftanhf.v v8, v8");
        asm volatile("vftanhf.v v12, v12");
        asm volatile("vse16.v  v0, (%0)" :: "r"(out)           : "memory");
        asm volatile("vse16.v  v4, (%0)" :: "r"(out +    vl)   : "memory");
        asm volatile("vse16.v  v8, (%0)" :: "r"(out + 2u*vl)   : "memory");
        asm volatile("vse16.v  v12, (%0)" :: "r"(out + 3u*vl)  : "memory");
        inp += 4u * vl; out += 4u * vl;
    }
    int rem = N - iters * (int)(4u * vl);
    while (rem > 0) {
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile("vle16.v  v0, (%0)" :: "r"(inp) : "memory");
        asm volatile("vftanhf.v v0, v0");
        asm volatile("vse16.v  v0, (%0)" :: "r"(out) : "memory");
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vtanh_hw_m2(const uint16_t *inp, uint16_t *out, int N) {
    size_t vl;
    asm volatile("vsetvli %0, zero, e16, m2, ta, ma" : "=r"(vl));
    int iters = N / (int)(4u * vl);
    for (int i = 0; i < iters; i++) {
        asm volatile("vle16.v  v0, (%0)" :: "r"(inp)           : "memory");
        asm volatile("vle16.v  v2, (%0)" :: "r"(inp +    vl)   : "memory");
        asm volatile("vle16.v  v4, (%0)" :: "r"(inp + 2u*vl)   : "memory");
        asm volatile("vle16.v  v6, (%0)" :: "r"(inp + 3u*vl)   : "memory");
        asm volatile("" ::: "memory");
        asm volatile("vftanhf.v v0, v0");
        asm volatile("vftanhf.v v2, v2");
        asm volatile("vftanhf.v v4, v4");
        asm volatile("vftanhf.v v6, v6");
        asm volatile("vse16.v  v0, (%0)" :: "r"(out)           : "memory");
        asm volatile("vse16.v  v2, (%0)" :: "r"(out +    vl)   : "memory");
        asm volatile("vse16.v  v4, (%0)" :: "r"(out + 2u*vl)   : "memory");
        asm volatile("vse16.v  v6, (%0)" :: "r"(out + 3u*vl)   : "memory");
        inp += 4u * vl; out += 4u * vl;
    }
    int rem = N - iters * (int)(4u * vl);
    while (rem > 0) {
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile("vle16.v  v0, (%0)" :: "r"(inp) : "memory");
        asm volatile("vftanhf.v v0, v0");
        asm volatile("vse16.v  v0, (%0)" :: "r"(out) : "memory");
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vtanh_hw_m1(const uint16_t *inp, uint16_t *out, int N) {
    size_t vl;
    asm volatile("vsetvli %0, zero, e16, m1, ta, ma" : "=r"(vl));
    int iters = N / (int)(4u * vl);
    for (int i = 0; i < iters; i++) {
        asm volatile("vle16.v  v0, (%0)" :: "r"(inp)           : "memory");
        asm volatile("vle16.v  v1, (%0)" :: "r"(inp +    vl)   : "memory");
        asm volatile("vle16.v  v2, (%0)" :: "r"(inp + 2u*vl)   : "memory");
        asm volatile("vle16.v  v3, (%0)" :: "r"(inp + 3u*vl)   : "memory");
        asm volatile("" ::: "memory");
        asm volatile("vftanhf.v v0, v0");
        asm volatile("vftanhf.v v1, v1");
        asm volatile("vftanhf.v v2, v2");
        asm volatile("vftanhf.v v3, v3");
        asm volatile("vse16.v  v0, (%0)" :: "r"(out)           : "memory");
        asm volatile("vse16.v  v1, (%0)" :: "r"(out +    vl)   : "memory");
        asm volatile("vse16.v  v2, (%0)" :: "r"(out + 2u*vl)   : "memory");
        asm volatile("vse16.v  v3, (%0)" :: "r"(out + 3u*vl)   : "memory");
        inp += 4u * vl; out += 4u * vl;
    }
    int rem = N - iters * (int)(4u * vl);
    while (rem > 0) {
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile("vle16.v  v0, (%0)" :: "r"(inp) : "memory");
        asm volatile("vftanhf.v v0, v0");
        asm volatile("vse16.v  v0, (%0)" :: "r"(out) : "memory");
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vtanh_hw_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vtanh_hw_m8(inp, out, N);
#elif LMUL_MODE == 4
    vtanh_hw_m4(inp, out, N);
#elif LMUL_MODE == 2
    vtanh_hw_m2(inp, out, N);
#elif LMUL_MODE == 1
    vtanh_hw_m1(inp, out, N);
#else
#   error "LMUL_MODE must be 1, 2, 4 or 8"
#endif
}

static uint16_t *g_inp, *g_out;

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();
    const int N = B_Size * T_Size * C_Size;
    if (cid == 0) {
        g_inp = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_out = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        snrt_dma_start_1d(g_inp, data_signed, N * sizeof(uint16_t));
        snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();
    const int start = (int)((int64_t)cid       * N / (int64_t)cores);
    const int end   = (int)((int64_t)(cid + 1) * N / (int64_t)cores);
    const int len   = end - start;
    snrt_cluster_hw_barrier();
    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }
    if (len > 0) vtanh_hw_kernel(g_inp + start, g_out + start, len);
    snrt_cluster_hw_barrier();
    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[TANH_HW_FP16 LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_fp16(g_out, outT, N, "TANH_HW_FP16");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
