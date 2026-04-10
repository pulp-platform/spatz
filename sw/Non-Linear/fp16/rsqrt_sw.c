/*  rsqrt_sw.c – Software reciprocal sqrt (fast inverse sqrt + 2 NR) (FP16)
 *
 *  Algorithm: seed = MAGIC − (int(x) >> 1), then 2 NR: y *= (1.5 − 0.5·x·y²)
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

static const unsigned int RSQRT_M = 0x59B8u;
static const float half = 0.5f;
static const float three_half = 1.5f;

/* ========================= LMUL = 8 ========================= */
static inline void vrsqrt_m8(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        /* Block 1: seed + NR1 */
        asm volatile(
            "vle16.v    v8,  (%[pin])           \n\t"
            "vsrl.vi    v16, v8,  1              \n\t"
            "vrsub.vx   v16, v16, %[M]          \n\t"
            "vfmul.vf   v24, v8,  %[h]          \n\t"
            "vfmul.vv   v8,  v16, v16            \n\t"
            "vfmul.vv   v8,  v24, v8             \n\t"
            "vfmv.v.f   v0,  %[p]               \n\t"
            "vfsub.vv   v8,  v0,  v8             \n\t"
            "vfmul.vv   v16, v16, v8             \n\t"
            :: [pin]"r"(inp),
               [M]"r"(RSQRT_M), [h]"f"(half), [p]"f"(three_half)
            : "memory"
        );
        /* Block 2: NR2 + store */
        asm volatile(
            "vle16.v    v8,  (%[pin])           \n\t"
            "vfmul.vf   v24, v8,  %[h]          \n\t"
            "vfmul.vv   v8,  v16, v16            \n\t"
            "vfmul.vv   v8,  v24, v8             \n\t"
            "vfmv.v.f   v0,  %[p]               \n\t"
            "vfsub.vv   v8,  v0,  v8             \n\t"
            "vfmul.vv   v16, v16, v8             \n\t"
            "vse16.v    v16, (%[po])             \n\t"
            :: [pin]"r"(inp), [po]"r"(out),
               [h]"f"(half), [p]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vrsqrt_m4(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vsrl.vi    v4,  v0,  1              \n\t"
            "vrsub.vx   v4,  v4,  %[M]          \n\t"
            "vfmul.vf   v12, v0,  %[h]          \n\t"
            "vfmul.vv   v8,  v4,  v4             \n\t"
            "vfmul.vv   v8,  v12, v8             \n\t"
            "vfrsub.vf  v8,  v8,  %[p]           \n\t"
            "vfmul.vv   v4,  v4,  v8             \n\t"
            "vfmul.vv   v8,  v4,  v4             \n\t"
            "vfmul.vv   v8,  v12, v8             \n\t"
            "vfrsub.vf  v8,  v8,  %[p]           \n\t"
            "vfmul.vv   v4,  v4,  v8             \n\t"
            "vse16.v    v4,  (%[po])             \n\t"
            :: [pin]"r"(inp), [po]"r"(out),
               [M]"r"(RSQRT_M), [h]"f"(half), [p]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vrsqrt_m2(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vsrl.vi    v2,  v0,  1              \n\t"
            "vrsub.vx   v2,  v2,  %[M]          \n\t"
            "vfmul.vf   v6,  v0,  %[h]          \n\t"
            "vfmul.vv   v4,  v2,  v2             \n\t"
            "vfmul.vv   v4,  v6,  v4             \n\t"
            "vfrsub.vf  v4,  v4,  %[p]           \n\t"
            "vfmul.vv   v2,  v2,  v4             \n\t"
            "vfmul.vv   v4,  v2,  v2             \n\t"
            "vfmul.vv   v4,  v6,  v4             \n\t"
            "vfrsub.vf  v4,  v4,  %[p]           \n\t"
            "vfmul.vv   v2,  v2,  v4             \n\t"
            "vse16.v    v2,  (%[po])             \n\t"
            :: [pin]"r"(inp), [po]"r"(out),
               [M]"r"(RSQRT_M), [h]"f"(half), [p]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vrsqrt_m1(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vsrl.vi    v1,  v0,  1              \n\t"
            "vrsub.vx   v1,  v1,  %[M]          \n\t"
            "vfmul.vf   v3,  v0,  %[h]          \n\t"
            "vfmul.vv   v2,  v1,  v1             \n\t"
            "vfmul.vv   v2,  v3,  v2             \n\t"
            "vfrsub.vf  v2,  v2,  %[p]           \n\t"
            "vfmul.vv   v1,  v1,  v2             \n\t"
            "vfmul.vv   v2,  v1,  v1             \n\t"
            "vfmul.vv   v2,  v3,  v2             \n\t"
            "vfrsub.vf  v2,  v2,  %[p]           \n\t"
            "vfmul.vv   v1,  v1,  v2             \n\t"
            "vse16.v    v1,  (%[po])             \n\t"
            :: [pin]"r"(inp), [po]"r"(out),
               [M]"r"(RSQRT_M), [h]"f"(half), [p]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vrsqrt_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vrsqrt_m8(inp, out, N);
#elif LMUL_MODE == 4
    vrsqrt_m4(inp, out, N);
#elif LMUL_MODE == 2
    vrsqrt_m2(inp, out, N);
#elif LMUL_MODE == 1
    vrsqrt_m1(inp, out, N);
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

    if (len > 0) vrsqrt_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[RSQRT_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_fp16(g_out, outR, N, "RSQRT_SW");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
