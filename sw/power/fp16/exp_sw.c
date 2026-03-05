/*  exp_sw.c – Schraudolph fast exp(x) for FP16 power evaluation
 *
 *  Algorithm:  exp(x) ≈ reinterpret_fp16( uint16( B + C·x ) )
 *    C = 2^10 / ln(2) ≈ 1477.32
 *    B = 15 × 2^10   = 15360
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

#define FP16_SCH_C  0x65C5u   /* 1477.0  (2^10 / ln(2))  */
#define FP16_SCH_B  0x7380u   /* 15360.0 (15 × 2^10)     */

/* ========================= LMUL = 8 ========================= */
static inline void vexp_m8(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        float B = bf16_scalar(FP16_SCH_B);
        float C = bf16_scalar(FP16_SCH_C);
        asm volatile(
            "vle16.v         v0,  (%[pin])     \n\t"
            "vfmv.v.f        v8,  %[B]         \n\t"
            "vfmacc.vf       v8,  %[C], v0      \n\t"
            "vfcvt.rtz.xu.f.v v16, v8           \n\t"
            "vse16.v         v16, (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vexp_m4(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        float B = bf16_scalar(FP16_SCH_B);
        float C = bf16_scalar(FP16_SCH_C);
        asm volatile(
            "vle16.v         v0,  (%[pin])     \n\t"
            "vfmv.v.f        v4,  %[B]         \n\t"
            "vfmacc.vf       v4,  %[C], v0      \n\t"
            "vfcvt.rtz.xu.f.v v8,  v4           \n\t"
            "vse16.v         v8,  (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vexp_m2(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        float B = bf16_scalar(FP16_SCH_B);
        float C = bf16_scalar(FP16_SCH_C);
        asm volatile(
            "vle16.v         v0,  (%[pin])     \n\t"
            "vfmv.v.f        v2,  %[B]         \n\t"
            "vfmacc.vf       v2,  %[C], v0      \n\t"
            "vfcvt.rtz.xu.f.v v4,  v2           \n\t"
            "vse16.v         v4,  (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vexp_m1(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        float B = bf16_scalar(FP16_SCH_B);
        float C = bf16_scalar(FP16_SCH_C);
        asm volatile(
            "vle16.v         v0,  (%[pin])     \n\t"
            "vfmv.v.f        v1,  %[B]         \n\t"
            "vfmacc.vf       v1,  %[C], v0      \n\t"
            "vfcvt.rtz.xu.f.v v2,  v1           \n\t"
            "vse16.v         v2,  (%[po])       \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vexp_kernel(const uint16_t *inp, uint16_t *out, int N) {
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
    /* FP16 is FMODE=0 (default), no CSR change needed */
    if (len > 0) vexp_kernel(g_inp + start, g_out + start, len);
    snrt_cluster_hw_barrier();
    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[EXP_FP16 LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_fp16(g_out, outE, N, "EXP_FP16");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
