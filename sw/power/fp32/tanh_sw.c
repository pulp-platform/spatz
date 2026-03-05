/*  tanh.c – Approximate polynomial clamped tanh(x) for FP32 power evaluation
 *
 *  Algorithm:  y = x · (C + x² · (B + A·x²)),  clamped to [-1, +1]
 *    A = 0.02655122f,  B = -0.22830456f,  C = 0.97858769f
 *
 *  Compute instructions per element:  ~7  (vfmul, vfmv, vfmacc ×2, vfmul, vfmax, vfmin)
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

static const float COEFF_A =  0.02655122f;
static const float COEFF_B = -0.22830456f;
static const float COEFF_C =  0.97858769f;
static const float POS_ONE =  1.0f;
static const float NEG_ONE = -1.0f;

/* ========================= LMUL = 8 =========================
 * v0 = x,  v8 = z = x²,  v16 = poly acc,  v24 = result
 */
static inline void vtanh_m8(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            /* z = x² */
            "vfmul.vv  v8,  v0,  v0            \n\t"
            /* t = B + A·z */
            "vfmv.v.f  v16, %[B]               \n\t"
            "vfmacc.vf v16, %[A], v8            \n\t"
            /* t = C + z·t */
            "vfmv.v.f  v24, %[C]               \n\t"
            "vfmacc.vv v24, v8,  v16            \n\t"
            /* y = x · t */
            "vfmul.vv  v24, v0,  v24            \n\t"
            /* clamp [-1, +1] */
            "vfmax.vf  v24, v24, %[N1]          \n\t"
            "vfmin.vf  v24, v24, %[P1]          \n\t"
            "vse32.v   v24, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [A]"f"(COEFF_A), [B]"f"(COEFF_B), [C]"f"(COEFF_C),
                [P1]"f"(POS_ONE), [N1]"f"(NEG_ONE)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vtanh_m4(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmul.vv  v4,  v0,  v0            \n\t"
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[A], v4            \n\t"
            "vfmv.v.f  v12, %[C]               \n\t"
            "vfmacc.vv v12, v4,  v8             \n\t"
            "vfmul.vv  v12, v0,  v12            \n\t"
            "vfmax.vf  v12, v12, %[N1]          \n\t"
            "vfmin.vf  v12, v12, %[P1]          \n\t"
            "vse32.v   v12, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [A]"f"(COEFF_A), [B]"f"(COEFF_B), [C]"f"(COEFF_C),
                [P1]"f"(POS_ONE), [N1]"f"(NEG_ONE)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vtanh_m2(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmul.vv  v2,  v0,  v0            \n\t"
            "vfmv.v.f  v4,  %[B]               \n\t"
            "vfmacc.vf v4,  %[A], v2            \n\t"
            "vfmv.v.f  v6,  %[C]               \n\t"
            "vfmacc.vv v6,  v2,  v4             \n\t"
            "vfmul.vv  v6,  v0,  v6             \n\t"
            "vfmax.vf  v6,  v6,  %[N1]          \n\t"
            "vfmin.vf  v6,  v6,  %[P1]          \n\t"
            "vse32.v   v6,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [A]"f"(COEFF_A), [B]"f"(COEFF_B), [C]"f"(COEFF_C),
                [P1]"f"(POS_ONE), [N1]"f"(NEG_ONE)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vtanh_m1(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmul.vv  v1,  v0,  v0            \n\t"
            "vfmv.v.f  v2,  %[B]               \n\t"
            "vfmacc.vf v2,  %[A], v1            \n\t"
            "vfmv.v.f  v3,  %[C]               \n\t"
            "vfmacc.vv v3,  v1,  v2             \n\t"
            "vfmul.vv  v3,  v0,  v3             \n\t"
            "vfmax.vf  v3,  v3,  %[N1]          \n\t"
            "vfmin.vf  v3,  v3,  %[P1]          \n\t"
            "vse32.v   v3,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [A]"f"(COEFF_A), [B]"f"(COEFF_B), [C]"f"(COEFF_C),
                [P1]"f"(POS_ONE), [N1]"f"(NEG_ONE)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vtanh_kernel(const float *inp, float *out, int N) {
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

    if (len > 0) vtanh_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[TANH LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_result(g_inp, g_out, outT, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
