/*  GeLU (Tanh approx.) baseline – using SW tanh polynomial (RV64GCV)
 *
 *  Kernel:  GeLU(x) ≈ 0.5 · x · (1 + tanh( √(2/π) · (x + 0.044715·x³) ))
 *
 *  Decomposition:
 *    1. x3   = x · x · x                                [vfmul ×2]
 *    2. inner = √(2/π) · (x + 0.044715·x³)              [vfmacc + vfmul]
 *    3. t    = tanh_sw(inner)                            [SW polynomial ~7 insns]
 *    4. out  = 0.5 · x · (1 + t)                        [vfadd + vfmul ×2]
 *
 *  SW tanh: y = x·(C + x²·(B + A·x²)), clamped to [-1,+1]
 *
 *  LMUL = 4 (e32) — 8 register groups
 *
 *  Register plan:
 *   v0  : x (kept)         v4  : x² / inner² (z)
 *   v8  : x³ / inner       v12 : poly acc / tanh
 *   v16 : temp              v20 : temp
 *   v24 : result
 */

#include "snrt.h"
#include "printf.h"
/* -- data include (overridable via -DDATA_HEADER_PATH=...) -- */
#if defined(DATA_HEADER_PATH)
#define _DATA_STR2(x) #x
#define _DATA_STR(x)  _DATA_STR2(x)
#include _DATA_STR(DATA_HEADER_PATH)
#else
#include "../data/data.h"
#endif
#include "../benchmark/benchmark.c"

#include <math.h>

/* ---- GeLU constants ---- */
static const float GELU_COEFF = 0.044715f;
static const float SQRT_2_PI  = 0.7978845608f;   /* √(2/π) */

/* ---- Tanh polynomial constants ---- */
static const float TANH_A =  0.02655122f;
static const float TANH_B = -0.22830456f;
static const float TANH_C =  0.97858769f;

static void gelu_baseline(const float *inp, float *out, int N) {
    float half    =  0.5f;
    float pos_one =  1.0f;
    float neg_one = -1.0f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            /* load x → v0 (preserved) */
            "vle32.v    v0,  (%[pin])             \n\t"

            /* ---- 1-2. inner = √(2/π) · (x + 0.044715·x³) ---- */
            "vfmul.vv   v4,  v0,  v0              \n\t"   /* x² → v4 */
            "vfmul.vv   v8,  v4,  v0              \n\t"   /* x³ → v8 */
            /* inner_arg = x + 0.044715·x³ */
            "vmv.v.v    v12, v0                   \n\t"   /* copy x */
            "vfmacc.vf  v12, %[gc], v8            \n\t"   /* x + gc·x³ */
            /* inner = √(2/π) · inner_arg */
            "vfmul.vf   v8,  v12, %[sq]           \n\t"   /* inner → v8 */

            /* ---- 3. tanh(inner) via polynomial ---- */
            /* z = inner² */
            "vfmul.vv   v4,  v8,  v8              \n\t"   /* z = inner² → v4 */
            /* t = B + A·z */
            "vfmv.v.f   v12, %[tB]                \n\t"
            "vfmacc.vf  v12, %[tA], v4             \n\t"   /* B+A·z → v12 */
            /* t = C + z·t */
            "vfmv.v.f   v16, %[tC]                \n\t"
            "vfmacc.vv  v16, v4,  v12              \n\t"   /* C+z·(B+A·z) → v16 */
            /* tanh = inner · t */
            "vfmul.vv   v16, v8,  v16              \n\t"
            /* clamp [-1, +1] */
            "vfmax.vf   v16, v16, %[n1]            \n\t"
            "vfmin.vf   v16, v16, %[p1]            \n\t"   /* tanh → v16 */

            /* ---- 4. GeLU = 0.5 · x · (1 + tanh) ---- */
            "vfadd.vf   v16, v16, %[one]           \n\t"   /* 1 + tanh */
            "vfmul.vv   v20, v0,  v16              \n\t"   /* x · (1+tanh) */
            "vfmul.vf   v20, v20, %[half]          \n\t"   /* 0.5 · … */
            "vse32.v    v20, (%[po])               \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [gc]"f"(GELU_COEFF), [sq]"f"(SQRT_2_PI),
              [tA]"f"(TANH_A), [tB]"f"(TANH_B), [tC]"f"(TANH_C),
              [half]"f"(half), [one]"f"(pos_one),
              [p1]"f"(pos_one), [n1]"f"(neg_one)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const float *input, const float *x, int r) {
    for (int i = 0; i < r && i < 8; i++) {
        unsigned ui, uo; __builtin_memcpy(&ui, &input[i], 4); __builtin_memcpy(&uo, &x[i], 4);
        printf("  x[%d]=0x%08X  gelu=0x%08X\n", i, ui, uo);
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

    if (len > 0) gelu_baseline(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[GELU_TANH BASELINE] cycles=%u  cores=%u  N=%d\n",
               cyc, cores, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
