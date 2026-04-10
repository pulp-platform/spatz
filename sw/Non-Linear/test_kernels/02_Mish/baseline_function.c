/*  Mish baseline – Mish activation using SW exp, ln, tanh (RV64GCV)
 *
 *  Kernel:  Mish(x) = x · tanh( softplus(x) )
 *                    = x · tanh( ln(1 + exp(x)) )
 *
 *  Decomposition into SW primitives (all vectorized, LMUL=4):
 *    1. e   = exp_sw(x)           Schraudolph: float(uint(B + C·x))
 *    2. sp  = 1 + e               vector add
 *    3. sp  = ln_sw(sp)           bit trick:  float(int(sp) - BIAS) · SCALE
 *    4. t   = tanh_sw(sp)         polynomial: sp·(C + sp²·(B + A·sp²)), clamp
 *    5. out = x · t               vector multiply
 *
 *  Register plan (LMUL=4, 8 groups):
 *   v0  : x (kept)        v4  : exp(x) → 1+exp → ln arg → sp²
 *   v8  : sp (softplus)   v12 : poly acc / tanh result
 *   v16 : temp             v20 : temp
 *   v24 : temp             v28 : result
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

/* ---- Schraudolph exp constants ---- */
static const float SCH_C = 12102203.0f;
static const float SCH_B = 1064866805.0f;

/* ---- Fast ln constants ---- */
static const unsigned int LN_BIAS  = 0x3F800000u;
static const float        LN_SCALE = 8.26295829e-8f;

/* ---- Tanh polynomial constants ---- */
static const float TANH_A =  0.02655122f;
static const float TANH_B = -0.22830456f;
static const float TANH_C =  0.97858769f;

static void mish_baseline(const float *inp, float *out, int N) {
    float pos_one =  1.0f;
    float neg_one = -1.0f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            /* load x → v0 (preserved throughout) */
            "vle32.v    v0,  (%[pin])             \n\t"

            /* ---- 1. exp(x) via Schraudolph ---- */
            "vfmv.v.f   v4,  %[schB]              \n\t"
            "vfmacc.vf  v4,  %[schC], v0           \n\t"   /* B + C·x */
            "vfcvt.rtz.xu.f.v v4,  v4              \n\t"   /* exp(x) ≈ v4 */

            /* ---- 2. softplus arg: 1 + exp(x) ---- */
            "vfadd.vf   v4,  v4,  %[one]           \n\t"   /* 1+exp(x) → v4 */

            /* ---- 3. ln(1+exp(x)) via bit trick ---- */
            "vsub.vx    v8,  v4,  %[lnB]           \n\t"   /* int(val)-BIAS */
            "vfcvt.f.x.v v8,  v8                   \n\t"
            "vfmul.vf   v8,  v8,  %[lnS]           \n\t"   /* sp = ln(…) → v8 */

            /* ---- 4. tanh(sp) polynomial ---- */
            /* z = sp² */
            "vfmul.vv   v12, v8,  v8                \n\t"   /* sp² → v12 */
            /* t = B + A·z */
            "vfmv.v.f   v16, %[tB]                 \n\t"
            "vfmacc.vf  v16, %[tA], v12             \n\t"   /* B+A·z → v16 */
            /* t = C + z·t */
            "vfmv.v.f   v20, %[tC]                 \n\t"
            "vfmacc.vv  v20, v12, v16               \n\t"   /* C+z·t → v20 */
            /* tanh = sp · t */
            "vfmul.vv   v20, v8,  v20               \n\t"
            /* clamp [-1, +1] */
            "vfmax.vf   v20, v20, %[n1]             \n\t"
            "vfmin.vf   v20, v20, %[p1]             \n\t"   /* tanh(sp) → v20 */

            /* ---- 5. Mish = x · tanh(sp) ---- */
            "vfmul.vv   v24, v0,  v20               \n\t"
            "vse32.v    v24, (%[po])                \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [schB]"f"(SCH_B), [schC]"f"(SCH_C),
              [lnB]"r"(LN_BIAS), [lnS]"f"(LN_SCALE),
              [tA]"f"(TANH_A), [tB]"f"(TANH_B), [tC]"f"(TANH_C),
              [one]"f"(pos_one), [p1]"f"(pos_one), [n1]"f"(neg_one)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const float *input, const float *x, int r) {
    for (int i = 0; i < r && i < 8; i++) {
        unsigned ui, uo; __builtin_memcpy(&ui, &input[i], 4); __builtin_memcpy(&uo, &x[i], 4);
        printf("  x[%d]=0x%08X  mish=0x%08X\n", i, ui, uo);
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

    if (len > 0) mish_baseline(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[MISH BASELINE] cycles=%u  cores=%u  N=%d\n", cyc, cores, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
