/*  tanh_sw.c – Approximate polynomial clamped tanh(x) for BF16 power evaluation
 *
 *  Algorithm:  y = x * (C + x^2 * (B + A*x^2)),  clamped to [-1, +1]
 *    A = 0.02661 (BF16: 0x3CDA),  B = -0.22852 (BF16: 0xBE6A),  C = 0.98047 (BF16: 0x3F7B)
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

#define BF16_A   0x3CDAu  /*  0.026611328125 */
#define BF16_B   0xBE6Au  /* -0.228515625    */
#define BF16_C   0x3F7Bu  /*  0.98046875     */
#define BF16_P1  0x3F80u  /*  1.0            */
#define BF16_N1  0xBF80u  /* -1.0            */

/* ========================= LMUL = 8 ========================= */
static inline void vtanh_m8(const uint16_t *inp, uint16_t *out, int N) {
    float A = bf16_scalar(BF16_A);
    float B = bf16_scalar(BF16_B);
    float C = bf16_scalar(BF16_C);
    float P1 = bf16_scalar(BF16_P1);
    float N1 = bf16_scalar(BF16_N1);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v   v0,  (%[pin])           \n\t"
            "vfmul.vv  v8,  v0,  v0            \n\t"
            "vfmv.v.f  v16, %[B]               \n\t"
            "vfmacc.vf v16, %[A], v8            \n\t"
            "vfmv.v.f  v24, %[C]               \n\t"
            "vfmacc.vv v24, v8,  v16            \n\t"
            "vfmul.vv  v24, v0,  v24            \n\t"
            "vfmax.vf  v24, v24, %[N1]          \n\t"
            "vfmin.vf  v24, v24, %[P1]          \n\t"
            "vse16.v   v24, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [A]"f"(A), [B]"f"(B), [C]"f"(C),
                [P1]"f"(P1), [N1]"f"(N1)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vtanh_m4(const uint16_t *inp, uint16_t *out, int N) {
    float A = bf16_scalar(BF16_A);
    float B = bf16_scalar(BF16_B);
    float C = bf16_scalar(BF16_C);
    float P1 = bf16_scalar(BF16_P1);
    float N1 = bf16_scalar(BF16_N1);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v   v0,  (%[pin])           \n\t"
            "vfmul.vv  v4,  v0,  v0            \n\t"
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[A], v4            \n\t"
            "vfmv.v.f  v12, %[C]               \n\t"
            "vfmacc.vv v12, v4,  v8             \n\t"
            "vfmul.vv  v12, v0,  v12            \n\t"
            "vfmax.vf  v12, v12, %[N1]          \n\t"
            "vfmin.vf  v12, v12, %[P1]          \n\t"
            "vse16.v   v12, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [A]"f"(A), [B]"f"(B), [C]"f"(C),
                [P1]"f"(P1), [N1]"f"(N1)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vtanh_m2(const uint16_t *inp, uint16_t *out, int N) {
    float A = bf16_scalar(BF16_A);
    float B = bf16_scalar(BF16_B);
    float C = bf16_scalar(BF16_C);
    float P1 = bf16_scalar(BF16_P1);
    float N1 = bf16_scalar(BF16_N1);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v   v0,  (%[pin])           \n\t"
            "vfmul.vv  v2,  v0,  v0            \n\t"
            "vfmv.v.f  v4,  %[B]               \n\t"
            "vfmacc.vf v4,  %[A], v2            \n\t"
            "vfmv.v.f  v6,  %[C]               \n\t"
            "vfmacc.vv v6,  v2,  v4             \n\t"
            "vfmul.vv  v6,  v0,  v6             \n\t"
            "vfmax.vf  v6,  v6,  %[N1]          \n\t"
            "vfmin.vf  v6,  v6,  %[P1]          \n\t"
            "vse16.v   v6,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [A]"f"(A), [B]"f"(B), [C]"f"(C),
                [P1]"f"(P1), [N1]"f"(N1)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vtanh_m1(const uint16_t *inp, uint16_t *out, int N) {
    float A = bf16_scalar(BF16_A);
    float B = bf16_scalar(BF16_B);
    float C = bf16_scalar(BF16_C);
    float P1 = bf16_scalar(BF16_P1);
    float N1 = bf16_scalar(BF16_N1);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v   v0,  (%[pin])           \n\t"
            "vfmul.vv  v1,  v0,  v0            \n\t"
            "vfmv.v.f  v2,  %[B]               \n\t"
            "vfmacc.vf v2,  %[A], v1            \n\t"
            "vfmv.v.f  v3,  %[C]               \n\t"
            "vfmacc.vv v3,  v1,  v2             \n\t"
            "vfmul.vv  v3,  v0,  v3             \n\t"
            "vfmax.vf  v3,  v3,  %[N1]          \n\t"
            "vfmin.vf  v3,  v3,  %[P1]          \n\t"
            "vse16.v   v3,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [A]"f"(A), [B]"f"(B), [C]"f"(C),
                [P1]"f"(P1), [N1]"f"(N1)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vtanh_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vtanh_m8(inp, out, N);
#elif LMUL_MODE == 4
    vtanh_m4(inp, out, N);
#elif LMUL_MODE == 2
    vtanh_m2(inp, out, N);
#elif LMUL_MODE == 1
    vtanh_m1(inp, out, N);
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
    asm volatile("csrwi 0x800, 3" ::: "memory");
    if (len > 0) vtanh_kernel(g_inp + start, g_out + start, len);
    asm volatile("csrwi 0x800, 0" ::: "memory");
    snrt_cluster_hw_barrier();
    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[TANH_BF16 LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_bf16(g_out, outT, N, "TANH_BF16");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
