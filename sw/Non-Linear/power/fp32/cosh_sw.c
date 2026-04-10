/*  cosh.c – Schraudolph-based fast cosh(x) for FP32 power evaluation
 *
 *  Algorithm:  cosh(x) = 0.5 * (exp(x) + exp(-x))
 *    where exp(x) ≈ reinterpret_float( uint32( B + C·x ) )
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

static const float SCH_C = 12102203.0f;
static const float SCH_B = 1064866805.0f;

/* ========================= LMUL = 8 =========================
 * v0  = x                   v8  = B+C*x (→ exp+ bits)
 * v16 = -x → B+C*(-x) (→ exp- bits)   v24 = sum → result
 */
static inline void vcosh_m8(const float *inp, float *out, int N) {
    float half = 0.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            /* exp(+x) */
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v8,  v8           \n\t"
            /* exp(-x) : negate x first */
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v16, %[B]               \n\t"
            "vfmacc.vf v16, %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v16, v16          \n\t"
            /* cosh = 0.5*(exp+ + exp-) */
            "vfadd.vv  v24, v8, v16             \n\t"
            "vfmul.vf  v24, v24, %[H]           \n\t"
            "vse32.v   v24, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(SCH_B), [C]"f"(SCH_C), [H]"f"(half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vcosh_m4(const float *inp, float *out, int N) {
    float half = 0.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v4,  %[B]               \n\t"
            "vfmacc.vf v4,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v4, v4            \n\t"
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v8, v8            \n\t"
            "vfadd.vv  v12, v4, v8              \n\t"
            "vfmul.vf  v12, v12, %[H]           \n\t"
            "vse32.v   v12, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(SCH_B), [C]"f"(SCH_C), [H]"f"(half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vcosh_m2(const float *inp, float *out, int N) {
    float half = 0.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v2,  %[B]               \n\t"
            "vfmacc.vf v2,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v2, v2            \n\t"
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v4,  %[B]               \n\t"
            "vfmacc.vf v4,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v4, v4            \n\t"
            "vfadd.vv  v6,  v2, v4              \n\t"
            "vfmul.vf  v6,  v6,  %[H]           \n\t"
            "vse32.v   v6,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(SCH_B), [C]"f"(SCH_C), [H]"f"(half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vcosh_m1(const float *inp, float *out, int N) {
    float half = 0.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v1,  %[B]               \n\t"
            "vfmacc.vf v1,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v1, v1            \n\t"
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v2,  %[B]               \n\t"
            "vfmacc.vf v2,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v2, v2            \n\t"
            "vfadd.vv  v3,  v1, v2              \n\t"
            "vfmul.vf  v3,  v3,  %[H]           \n\t"
            "vse32.v   v3,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(SCH_B), [C]"f"(SCH_C), [H]"f"(half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ---- selector ---- */
static inline void vcosh_kernel(const float *inp, float *out, int N) {
#if   LMUL_MODE == 8
    vcosh_m8(inp, out, N);
#elif LMUL_MODE == 4
    vcosh_m4(inp, out, N);
#elif LMUL_MODE == 2
    vcosh_m2(inp, out, N);
#elif LMUL_MODE == 1
    vcosh_m1(inp, out, N);
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

    if (len > 0) vcosh_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[COSH LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_result(g_inp, g_out, outC, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
