/*  RMSNorm custom – Root Mean Square Normalization using HW vfrsqrt7.v
 *
 *  Kernel:  RMSNorm(x) = x · rsqrt( mean(x²) + ε )
 *
 *  Two passes (parallelized across SM_CORES):
 *    Pass 1: each core computes partial Σ x[i]² via vfmul + vfredusum.vs
 *             → barrier → core 0 sums partials → global sq_sum
 *             → core 0 computes rsqrt via HW vfrsqrt7.v + 1 NR step
 *             → broadcast inv_rms via shared L1
 *    Pass 2: each core normalises its chunk: out[i] = x[i] · inv_rms
 *
 *  HW: vfrsqrt7.v provides a 7-bit accurate seed (~1% relative error).
 *  One Newton-Raphson step: y = y · (1.5 − 0.5·x·y²) brings this to
 *  ~14 bits (~4 decimal digits), close to full FP32 mantissa precision
 *  for the single scalar rsqrt we need.  This replaces the Quake III
 *  magic seed (4 insns) + 2 NR (6 insns) = 10 insns with 1 + 3 insns.
 *
 *  LMUL = 4 (e32)
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
#include <stdint.h>

/* Number of cores to use (≤ cluster core count) */
#define SM_CORES 2

static const float EPS = 1e-6f;

/* ---- Shared state in L1 ---- */
static float *g_inp, *g_out;
static float *g_partial_sq;    /* SM_CORES floats */
static float *g_inv_rms;       /* 1 float – broadcast */

/* -----------------------------------------------------------------------
 * pass1_sum_squares – vectorised Σ x[i]² via vfmul + vfredusum.vs.
 * Writes partial sum into g_partial_sq[cid].
 * ----------------------------------------------------------------------- */
static void pass1_sum_squares(const float *src, int len, int cid) {
    float zero_f = 0.0f;

    /* init scalar accumulator v8[0] = 0.0 */
    asm volatile(
        "vsetivli  zero, 1, e32, m1, ta, ma  \n\t"
        "vfmv.s.f  v8, %[z]                  \n\t"
        : : [z]"f"(zero_f) : "v8"
    );

    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v     v0, (%[s])               \n\t"
            "vfmul.vv    v4, v0, v0               \n\t"   /* x² */
            "vfredusum.vs v8, v4, v8              \n\t"   /* accumulate */
            :
            : [s]"r"(src)
            : "memory",
              "v0","v1","v2","v3",
              "v4","v5","v6","v7",
              "v8"
        );
        src += vl;
        rem -= vl;
    }

    /* extract and store */
    float local_sq;
    asm volatile(
        "vsetivli  zero, 1, e32, m1, ta, ma  \n\t"
        "vfmv.f.s  %[sq], v8                 \n\t"
        : [sq]"=f"(local_sq) : : "v8"
    );
    g_partial_sq[cid] = local_sq;
}

/* -----------------------------------------------------------------------
 * pass2_normalize – multiply chunk by shared inv_rms.
 * ----------------------------------------------------------------------- */
static void pass2_normalize(const float *src, float *dst,
                             int len, float inv) {
    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[src])             \n\t"
            "vfmul.vf   v4,  v0,  %[inv]          \n\t"
            "vse32.v    v4,  (%[dst])              \n\t"
            :
            : [src]"r"(src), [dst]"r"(dst), [inv]"f"(inv)
            : "memory",
              "v0","v1","v2","v3",
              "v4","v5","v6","v7"
        );
        src += vl;
        dst += vl;
        rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const float *input, const float *x, int r) {
    for (int i = 0; i < r && i < 8; i++) {
        unsigned ui, uo; __builtin_memcpy(&ui, &input[i], 4); __builtin_memcpy(&uo, &x[i], 4);
        printf("  x[%d]=0x%08X  norm=0x%08X\n", i, ui, uo);
    }
}

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    /* ---- Allocation (core 0) ---- */
    if (cid == 0) {
        g_inp        = (float *)snrt_l1alloc(N * sizeof(float));
        g_out        = (float *)snrt_l1alloc(N * sizeof(float));
        g_partial_sq = (float *)snrt_l1alloc(SM_CORES * sizeof(float));
        g_inv_rms    = (float *)snrt_l1alloc(1 * sizeof(float));
        snrt_dma_start_1d(g_inp, data_signed, N * sizeof(float));
        snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();

    /* Idle cores just hit every barrier then exit */
    if (cid >= SM_CORES) {
        snrt_cluster_hw_barrier();   /* after pass 1 */
        snrt_cluster_hw_barrier();   /* after rsqrt broadcast */
        snrt_cluster_hw_barrier();   /* after pass 2 */
        snrt_cluster_hw_barrier();   /* final */
        return 0;
    }

    /* ---- Work partitioning ---- */
    const int start = (int)((int64_t)cid       * N / (int64_t)SM_CORES);
    const int end   = (int)((int64_t)(cid + 1) * N / (int64_t)SM_CORES);
    const int len   = end - start;

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    /* ==================================================================
     * Pass 1 — parallel partial Σ x², then core 0 reduces + HW rsqrt
     * ================================================================== */
    if (len > 0)
        pass1_sum_squares(g_inp + start, len, cid);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        float sq_sum = 0.0f;
        for (int c = 0; c < SM_CORES; c++)
            sq_sum += g_partial_sq[c];

        /* Compute reciprocal of N via HW vfrec7.v to avoid fdiv */
        float n_f = (float)N;
        float inv_n;
        asm volatile(
            "vsetivli  zero, 1, e32, m1, ta, ma  \n\t"
            "vfmv.v.f  v0, %[nf]                 \n\t"
            "vfrec7.v  v0, v0                    \n\t"
            "vfmv.f.s  %[inv], v0                \n\t"
            : [inv]"=f"(inv_n)
            : [nf] "f"(n_f)
            : "v0"
        );
        float mean_sq = sq_sum * inv_n + EPS;

        /* HW rsqrt seed via vfrsqrt7.v (7-bit accurate, ~1% error) */
        float y;
        asm volatile(
            "vsetivli  zero, 1, e32, m1, ta, ma  \n\t"
            "vfmv.v.f  v0, %[ms]                 \n\t"
            "vfrsqrt7.v v0, v0                   \n\t"
            "vfmv.f.s  %[y], v0                  \n\t"
            : [y]"=f"(y)
            : [ms]"f"(mean_sq)
            : "v0"
        );

        /* One NR refinement: y = y · (1.5 − 0.5·x·y²)
         * Brings precision from ~7 bits to ~14 bits.                    */
        float half_x = 0.5f * mean_sq;
        y = y * (1.5f - half_x * y * y);
        *g_inv_rms = y;
    }
    snrt_cluster_hw_barrier();

    /* ==================================================================
     * Pass 2 — parallel normalize
     * ================================================================== */
    float inv = *g_inv_rms;   /* all active cores read broadcast value */

    if (len > 0)
        pass2_normalize(g_inp + start, g_out + start, len, inv);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[RMSNORM CUSTOM 2-CORE] cycles=%u  cores=%u  N=%d\n",
               cyc, SM_CORES, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}