/*  log_sw.c – "Cheapest" fast ln(x) for FP16 power evaluation
 *
 *  Algorithm:  ln(x) ≈ ( int16(x) - FP16_ONE ) * SCALE
 *    FP16_ONE = 0x3C00 (fp16 1.0 as integer = 15<<10 = 15360)
 *    SCALE    = ln(2) / 2^10 ≈ 0.000677
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

static const unsigned int MAGIC_BIAS   = 0x3C00u;        /* FP16 1.0 as integer */
#define FP16_SCALE  0x118Cu   /* ln(2) / 2^10 ≈ 0.000677  */

/* ========================= LMUL = 8 ========================= */
static inline void vlog_m8(const uint16_t *inp, uint16_t *out, int N) {
    float sc = bf16_scalar(FP16_SCALE);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v         v0,  (%[pin])     \n\t"
            "vsub.vx         v8,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v16, v8            \n\t"
            "vfmul.vf        v24, v16, %[sc]    \n\t"
            "vse16.v         v24, (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [bias]"r"(MAGIC_BIAS), [sc]"f"(sc)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vlog_m4(const uint16_t *inp, uint16_t *out, int N) {
    float sc = bf16_scalar(FP16_SCALE);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v         v0,  (%[pin])     \n\t"
            "vsub.vx         v4,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v8,  v4            \n\t"
            "vfmul.vf        v12, v8,  %[sc]    \n\t"
            "vse16.v         v12, (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [bias]"r"(MAGIC_BIAS), [sc]"f"(sc)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vlog_m2(const uint16_t *inp, uint16_t *out, int N) {
    float sc = bf16_scalar(FP16_SCALE);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v         v0,  (%[pin])     \n\t"
            "vsub.vx         v2,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v4,  v2            \n\t"
            "vfmul.vf        v6,  v4,  %[sc]    \n\t"
            "vse16.v         v6,  (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [bias]"r"(MAGIC_BIAS), [sc]"f"(sc)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vlog_m1(const uint16_t *inp, uint16_t *out, int N) {
    float sc = bf16_scalar(FP16_SCALE);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v         v0,  (%[pin])     \n\t"
            "vsub.vx         v1,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v2,  v1            \n\t"
            "vfmul.vf        v3,  v2,  %[sc]    \n\t"
            "vse16.v         v3,  (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [bias]"r"(MAGIC_BIAS), [sc]"f"(sc)
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
        printf("[LOG_FP16 LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_fp16(g_out, outL, N, "LOG_FP16");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
