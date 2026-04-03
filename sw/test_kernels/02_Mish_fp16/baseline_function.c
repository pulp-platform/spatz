/*  Mish baseline – Mish activation using SW exp, ln, tanh (FP16)
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
#include "../data/data_fp16_128.h"
#endif
#include "../benchmark/benchmark.c"

#include <math.h>
#include <stdint.h>

/* NaN-box a 16-bit FP constant into bits[15:0] of a float register.
 * Works for both FP16 and BF16; the Spatz VFU reads rs2[15:0] for e16. */
static inline float h_scalar(uint32_t hex16) {
    float f;
    uint32_t nanboxed = 0xFFFF0000u | hex16;
    asm volatile("fmv.w.x %0, %1" : "=f"(f) : "r"(nanboxed));
    return f;
}


/* ---- Schraudolph exp constants ---- */
#define H_SCH_C 0x65C5u
#define H_SCH_B 0x7380u

/* ---- Fast ln constants ---- */
#define H_LN_BIAS 0x3C00u
#define H_LN_SCALE 0x118Cu

/* ---- Tanh polynomial constants ---- */
#define H_TANH_A 0x26CCu
#define H_TANH_B 0xB34Eu
#define H_TANH_C 0x3BD4u

static void mish_baseline(const uint16_t *inp, uint16_t *out, int N) {
    float pos_one = h_scalar(0x3C00u);
    float neg_one = h_scalar(0xBC00u);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            /* load x → v0 (preserved throughout) */
            "vle16.v    v0,  (%[pin])             \n\t"

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
            "vse16.v    v24, (%[po])                \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [schB]"f"(h_scalar(H_SCH_B)), [schC]"f"(h_scalar(H_SCH_C)),
              [lnB]"r"(((unsigned int)H_LN_BIAS)), [lnS]"f"(h_scalar(H_LN_SCALE)),
              [tA]"f"(h_scalar(H_TANH_A)), [tB]"f"(h_scalar(H_TANH_B)), [tC]"f"(h_scalar(H_TANH_C)),
              [one]"f"(pos_one), [p1]"f"(pos_one), [n1]"f"(neg_one)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const uint16_t *input, const uint16_t *x, int r) {
    for (int i = 0; i < r && i < 8; i++)
        printf("  x[%d]=0x%04X  mish=0x%04X\n", i, (unsigned)input[i], (unsigned)x[i]);
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

    if (len > 0) mish_baseline(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[MISH BASELINE FP16] cycles=%u  cores=%u  N=%d\n", cyc, cores, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
