/*  tanh_sw.c – Degree-2 polynomial tanh for BF16
 *
 *  Algorithm: y = clamp(A*x^2 + B*|x| + C, -1, 1) * sign(x)
 *  (Piecewise polynomial approximation)
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

/* bf16_scalar: NaN-box BF16 bit pattern into FP register bits[15:0] */
static inline float bf16_scalar(uint32_t bf16_hex) {
    float f;
    uint32_t nanboxed = 0xFFFF0000u | bf16_hex;
    asm volatile("fmv.w.x %0, %1" : "=f"(f) : "r"(nanboxed));
    return f;
}

/* BF16 constants for tanh polynomial */
#define BF16_A   0x3CD9u   /* 0.026611328125  */
#define BF16_B   0xBE6Au   /* -0.228515625    */
#define BF16_C   0x3F7Bu   /* 0.98046875      */
#define BF16_P1  0x3F80u   /*  1.0            */
#define BF16_N1  0xBF80u   /* -1.0            */

/* ========================= LMUL = 8 ========================= */
static inline void vtanh_m8(const uint16_t *inp, uint16_t *out, int N) {
    float A_val  = bf16_scalar(BF16_A);
    float B_val  = bf16_scalar(BF16_B);
    float C_val  = bf16_scalar(BF16_C);
    float P1_val = bf16_scalar(BF16_P1);
    float N1_val = bf16_scalar(BF16_N1);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfabs.v    v8,  v0                \n\t"   /* |x| */
            "vfmul.vv   v16, v8,  v8           \n\t"   /* x^2 */
            "vfmul.vf   v16, v16, %[A]         \n\t"   /* A*x^2 */
            "vfmacc.vf  v16, %[B], v8          \n\t"   /* A*x^2 + B*|x| */
            "vfadd.vf   v16, v16, %[C]         \n\t"   /* + C */
            "vfmin.vf   v16, v16, %[P1]        \n\t"   /* clamp max 1 */
            "vfmax.vf   v16, v16, %[N1]        \n\t"   /* clamp min -1 */
            "vfsgnj.vv  v16, v16, v0           \n\t"   /* copy sign(x) */
            "vse16.v    v16, (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [A]"f"(A_val), [B]"f"(B_val), [C]"f"(C_val),
              [P1]"f"(P1_val), [N1]"f"(N1_val)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vtanh_m4(const uint16_t *inp, uint16_t *out, int N) {
    float A_val  = bf16_scalar(BF16_A);
    float B_val  = bf16_scalar(BF16_B);
    float C_val  = bf16_scalar(BF16_C);
    float P1_val = bf16_scalar(BF16_P1);
    float N1_val = bf16_scalar(BF16_N1);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfabs.v    v4,  v0                \n\t"
            "vfmul.vv   v8,  v4,  v4           \n\t"
            "vfmul.vf   v8,  v8,  %[A]         \n\t"
            "vfmacc.vf  v8,  %[B], v4          \n\t"
            "vfadd.vf   v8,  v8,  %[C]         \n\t"
            "vfmin.vf   v8,  v8,  %[P1]        \n\t"
            "vfmax.vf   v8,  v8,  %[N1]        \n\t"
            "vfsgnj.vv  v8,  v8,  v0           \n\t"
            "vse16.v    v8,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [A]"f"(A_val), [B]"f"(B_val), [C]"f"(C_val),
              [P1]"f"(P1_val), [N1]"f"(N1_val)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vtanh_m2(const uint16_t *inp, uint16_t *out, int N) {
    float A_val  = bf16_scalar(BF16_A);
    float B_val  = bf16_scalar(BF16_B);
    float C_val  = bf16_scalar(BF16_C);
    float P1_val = bf16_scalar(BF16_P1);
    float N1_val = bf16_scalar(BF16_N1);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfabs.v    v2,  v0                \n\t"
            "vfmul.vv   v4,  v2,  v2           \n\t"
            "vfmul.vf   v4,  v4,  %[A]         \n\t"
            "vfmacc.vf  v4,  %[B], v2          \n\t"
            "vfadd.vf   v4,  v4,  %[C]         \n\t"
            "vfmin.vf   v4,  v4,  %[P1]        \n\t"
            "vfmax.vf   v4,  v4,  %[N1]        \n\t"
            "vfsgnj.vv  v4,  v4,  v0           \n\t"
            "vse16.v    v4,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [A]"f"(A_val), [B]"f"(B_val), [C]"f"(C_val),
              [P1]"f"(P1_val), [N1]"f"(N1_val)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vtanh_m1(const uint16_t *inp, uint16_t *out, int N) {
    float A_val  = bf16_scalar(BF16_A);
    float B_val  = bf16_scalar(BF16_B);
    float C_val  = bf16_scalar(BF16_C);
    float P1_val = bf16_scalar(BF16_P1);
    float N1_val = bf16_scalar(BF16_N1);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfabs.v    v1,  v0                \n\t"
            "vfmul.vv   v2,  v1,  v1           \n\t"
            "vfmul.vf   v2,  v2,  %[A]         \n\t"
            "vfmacc.vf  v2,  %[B], v1          \n\t"
            "vfadd.vf   v2,  v2,  %[C]         \n\t"
            "vfmin.vf   v2,  v2,  %[P1]        \n\t"
            "vfmax.vf   v2,  v2,  %[N1]        \n\t"
            "vfsgnj.vv  v2,  v2,  v0           \n\t"
            "vse16.v    v2,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [A]"f"(A_val), [B]"f"(B_val), [C]"f"(C_val),
              [P1]"f"(P1_val), [N1]"f"(N1_val)
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

    asm volatile("csrwi 0x800, 3" ::: "memory");

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0) vtanh_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[TANH_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_bf16(g_out, outT, N, "TANH_SW");
    }

    asm volatile("csrwi 0x800, 0" ::: "memory");
    snrt_cluster_hw_barrier();
    return 0;
}
