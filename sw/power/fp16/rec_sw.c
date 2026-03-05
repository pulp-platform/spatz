/*  rec_sw.c – Software reciprocal via magic seed + 2 Newton-Raphson iterations (FP16)
 *
 *  Algorithm:
 *    seed = reinterpret_fp16( MAGIC - reinterpret_int16(a) )
 *    x = seed;  for 2 NR steps:
 *      acc = 2.0;  acc -= a * x;  x *= acc;  (vfnmsac + vfmul)
 *
 *  MAGIC for FP16: 0x7BFF  (2 × bias × 2^mant - 1 = 2×15×1024 - 1)
 *
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

static inline float bf16_scalar(uint32_t bf16_hex) {
    float f;
    uint32_t nanboxed = 0xFFFF0000u | bf16_hex;
    asm volatile("fmv.w.x %0, %1" : "=f"(f) : "r"(nanboxed));
    return f;
}

static const unsigned int REC_MAGIC = 0x7BFFu;
#define FP16_TWO  0x4000u   /* 2.0 */

/* ========================= LMUL = 8 ========================= */
static inline void vrec_sw_m8(const uint16_t *inp, uint16_t *out, int N) {
    float two = bf16_scalar(FP16_TWO);
    uint32_t two_bits = 0x4000u;  /* FP16 bits for 2.0 */
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v8,  (%[pin])           \n\t"
            "vrsub.vx   v16, v8,  %[M]          \n\t"
            "vmv.v.x    v24, %[tb]              \n\t"
            "vfnmsac.vv v24, v8,  v16            \n\t"
            "vfmul.vv   v16, v16, v24            \n\t"
            "vfmv.v.f   v24, %[two]             \n\t"
            "vfnmsac.vv v24, v8,  v16            \n\t"
            "vfmul.vv   v16, v16, v24            \n\t"
            "vse16.v    v16, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(REC_MAGIC), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vrec_sw_m4(const uint16_t *inp, uint16_t *out, int N) {
    float two = bf16_scalar(FP16_TWO);
    uint32_t two_bits = 0x4000u;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vrsub.vx   v4,  v0,  %[M]          \n\t"
            "vmv.v.x    v8,  %[tb]              \n\t"
            "vfnmsac.vv v8,  v0,  v4             \n\t"
            "vfmul.vv   v4,  v4,  v8             \n\t"
            "vfmv.v.f   v8,  %[two]             \n\t"
            "vfnmsac.vv v8,  v0,  v4             \n\t"
            "vfmul.vv   v4,  v4,  v8             \n\t"
            "vse16.v    v4,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(REC_MAGIC), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vrec_sw_m2(const uint16_t *inp, uint16_t *out, int N) {
    float two = bf16_scalar(FP16_TWO);
    uint32_t two_bits = 0x4000u;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vrsub.vx   v2,  v0,  %[M]          \n\t"
            "vmv.v.x    v4,  %[tb]              \n\t"
            "vfnmsac.vv v4,  v0,  v2             \n\t"
            "vfmul.vv   v2,  v2,  v4             \n\t"
            "vfmv.v.f   v4,  %[two]             \n\t"
            "vfnmsac.vv v4,  v0,  v2             \n\t"
            "vfmul.vv   v2,  v2,  v4             \n\t"
            "vse16.v    v2,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(REC_MAGIC), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vrec_sw_m1(const uint16_t *inp, uint16_t *out, int N) {
    float two = bf16_scalar(FP16_TWO);
    uint32_t two_bits = 0x4000u;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vrsub.vx   v1,  v0,  %[M]          \n\t"
            "vmv.v.x    v2,  %[tb]              \n\t"
            "vfnmsac.vv v2,  v0,  v1             \n\t"
            "vfmul.vv   v1,  v1,  v2             \n\t"
            "vfmv.v.f   v2,  %[two]             \n\t"
            "vfnmsac.vv v2,  v0,  v1             \n\t"
            "vfmul.vv   v1,  v1,  v2             \n\t"
            "vse16.v    v1,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(REC_MAGIC), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vrec_sw_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vrec_sw_m8(inp, out, N);
#elif LMUL_MODE == 4
    vrec_sw_m4(inp, out, N);
#elif LMUL_MODE == 2
    vrec_sw_m2(inp, out, N);
#elif LMUL_MODE == 1
    vrec_sw_m1(inp, out, N);
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
    if (len > 0) vrec_sw_kernel(g_inp + start, g_out + start, len);
    snrt_cluster_hw_barrier();
    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[REC_FP16 LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_fp16(g_out, outRec, N, "REC_FP16");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
