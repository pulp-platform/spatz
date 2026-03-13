/*  log_sw.c – Software log via bit trick (FP16)
 *
 *  Algorithm: ln(x) ≈ float(int(x) − BIAS) * SC
 *    BIAS = 0x3C00 (FP16 bias), SC = 0.000677130
 *
 *  Compile with -DLMUL_MODE={1,2,4,8}
 */

#include "snrt.h"
#include "printf.h"
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"
#include "sanity_check.h"

#include <stdint.h>

#ifndef LMUL_MODE
#define LMUL_MODE 8
#endif

static const unsigned int BIAS = 0x3C00u;
static const float SC = 0.000677130f;

/* ========================= LMUL = 8 ========================= */
static inline void vlog_m8(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v         v0,  (%[p])       \n\t"
            "vsub.vx         v8,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v16, v8            \n\t"
            "vfmul.vf        v24, v16, %[sc]    \n\t"
            "vse16.v         v24, (%[o])        \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [bias]"r"(BIAS), [sc]"f"(SC)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vlog_m4(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v         v0,  (%[p])       \n\t"
            "vsub.vx         v4,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v8,  v4            \n\t"
            "vfmul.vf        v12, v8,  %[sc]    \n\t"
            "vse16.v         v12, (%[o])        \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [bias]"r"(BIAS), [sc]"f"(SC)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vlog_m2(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v         v0,  (%[p])       \n\t"
            "vsub.vx         v2,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v4,  v2            \n\t"
            "vfmul.vf        v6,  v4,  %[sc]    \n\t"
            "vse16.v         v6,  (%[o])        \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [bias]"r"(BIAS), [sc]"f"(SC)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vlog_m1(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v         v0,  (%[p])       \n\t"
            "vsub.vx         v1,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v2,  v1            \n\t"
            "vfmul.vf        v3,  v2,  %[sc]    \n\t"
            "vse16.v         v3,  (%[o])        \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [bias]"r"(BIAS), [sc]"f"(SC)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vlog_kernel(const uint16_t *inp, uint16_t *out, int N) {
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

static uint16_t *g_inp, *g_out;

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    if (cid == 0) {
        g_inp = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_out = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        snrt_dma_start_1d(g_inp, data_positive, N * sizeof(uint16_t));
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
        printf("[LOG_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_fp16(g_out, outL, N, "LOG_SW");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
