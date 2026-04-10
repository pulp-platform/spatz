/*  RMSNorm baseline – Root Mean Square Normalization using SW rsqrt (BF16)
 *
 *  Kernel:  RMSNorm(x) = x · rsqrt( mean(x²) + ε )
 *
 *  Two passes (parallelized across SM_CORES):
 *    Pass 1: each core computes partial Σ x[i]² via vfmul + vfredusum.vs
 *             → barrier → core 0 sums partials → global sq_sum
 *             → core 0 computes rsqrt(sq_sum/N + ε) via Quake III + 2 NR
 *             → broadcast inv_rms via shared L1
 *    Pass 2: each core normalises its chunk: out[i] = x[i] · inv_rms
 *
 *  SW rsqrt: Quake III magic seed (0x5F3759DF) + 2 Newton-Raphson iterations
 *    seed = float( MAGIC - (int(x) >> 1) )
 *    NR:  y = y · (1.5 - 0.5·x·y²)   ×2
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
#include "../data/data_bf16_128.h"
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


/* Number of cores to use (≤ cluster core count) */
#define SM_CORES 2

/* ---- Quake III rsqrt constant ---- */
#define H_RSQRT_MAGIC 0x5F37u
#define H_EPS 0x3586u

/* ---- Shared state in L1 ---- */
static uint16_t *g_inp, *g_out;
static uint16_t *g_partial_sq;    /* SM_CORES floats */
static uint16_t *g_inv_rms;       /* 1 float – broadcast */

/* -----------------------------------------------------------------------
 * pass1_sum_squares – vectorised Σ x[i]² via vfmul + vfredusum.vs.
 * Writes partial sum into g_partial_sq[cid].
 * ----------------------------------------------------------------------- */
static void pass1_sum_squares(const uint16_t *src, int len, int cid) {
    float zero_f = h_scalar(0x0000u);

    /* init scalar accumulator v8[0] = 0.0 */
    asm volatile(
        "vsetivli  zero, 1, e16, m1, ta, ma  \n\t"
        "vfmv.s.f  v8, %[z]                  \n\t"
        : : [z]"f"(zero_f) : "v8"
    );

    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v     v0, (%[s])               \n\t"
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
    /* store 16-bit sq-sum directly from vector register */
    asm volatile(
        "vsetivli  zero, 1, e16, m1, ta, ma \n\t"
        "vse16.v   v8, (%[dst]) \n\t"
        : : [dst]"r"(&g_partial_sq[cid]) : "memory"
    );
}

/* -----------------------------------------------------------------------
 * pass2_normalize – multiply chunk by shared inv_rms.
 * ----------------------------------------------------------------------- */
static void pass2_normalize(const uint16_t *src, uint16_t *dst,
                             int len, float inv) {
    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[src])             \n\t"
            "vfmul.vf   v4,  v0,  %[inv]          \n\t"
            "vse16.v    v4,  (%[dst])              \n\t"
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
static void check_result(const uint16_t *input, const uint16_t *x, int r) {
    for (int i = 0; i < r && i < 8; i++)
        printf("  x[%d]=0x%04X  norm=0x%04X\n", i, (unsigned)input[i], (unsigned)x[i]);
}

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    /* ---- Allocation (core 0) ---- */
    if (cid == 0) {
        g_inp        = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_out        = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_partial_sq = (uint16_t *)snrt_l1alloc(SM_CORES * sizeof(uint16_t));
        g_inv_rms    = (uint16_t *)snrt_l1alloc(1 * sizeof(uint16_t));
        snrt_dma_start_1d(g_inp, data_signed, N * sizeof(uint16_t));
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
     * Pass 1 — parallel partial Σ x², then core 0 reduces + rsqrt
     * ================================================================== */
    if (len > 0)
        pass1_sum_squares(g_inp + start, len, cid);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        /* reduce partial sq-sums via vector instruction */
        float sq_zero = h_scalar(0x0000u);
        float sq_sum;
        asm volatile(
            "vsetivli zero, 1, e16, m1, ta, ma \n\t"
            "vfmv.s.f v4, %[z] \n\t"
            "vsetvli zero, %[n], e16, m1, ta, ma \n\t"
            "vle16.v  v0, (%[src]) \n\t"
            "vfredusum.vs v4, v0, v4 \n\t"
            "vfmv.f.s %[s], v4 \n\t"
            : [s]"=f"(sq_sum)
            : [src]"r"(g_partial_sq), [n]"r"(SM_CORES), [z]"f"(sq_zero)
            : "v0", "v4"
        );

        /* compute mean_sq in 16-bit via vector ops */
        float mean_sq;
        {
            float eps_h = h_scalar(H_EPS);
            float n_recip;
            asm volatile(
                "vsetivli zero, 1, e16, m1, ta, ma \n\t"
                "vmv.v.x  v0, %[n] \n\t"
                "vfcvt.f.xu.v v0, v0 \n\t"
                "vfrec7.v v0, v0 \n\t"
                "vfmv.f.s %[r], v0 \n\t"
                : [r]"=f"(n_recip) : [n]"r"(N) : "v0"
            );
            asm volatile(
                "vsetivli zero, 1, e16, m1, ta, ma \n\t"
                "vfmv.v.f v0, %[sq] \n\t"
                "vfmul.vf v0, v0, %[inv] \n\t"
                "vfadd.vf v0, v0, %[eps] \n\t"
                "vfmv.f.s %[r], v0 \n\t"
                : [r]"=f"(mean_sq)
                : [sq]"f"(sq_sum), [inv]"f"(n_recip), [eps]"f"(eps_h)
                : "v0"
            );
        }

        /* Quake III rsqrt: magic seed + 2 Newton-Raphson iterations */
        /* HW rsqrt + 1 NR step via single-element vector ops */
        float y;
        asm volatile(
            "vsetivli zero, 1, e16, m1, ta, ma \n\t"
            "vfmv.v.f v0, %[ms] \n\t"
            "vfrsqrt7.v v0, v0 \n\t"
            "vfmv.f.s %[y], v0 \n\t"
            : [y]"=f"(y) : [ms]"f"(mean_sq) : "v0"
        );
        float threehalves = h_scalar(0x3FC0u);
        float half_h = h_scalar(0x3F00u);
        asm volatile(
            "vsetivli zero, 1, e16, m1, ta, ma \n\t"
            "vfmv.v.f v0, %[ms] \n\t"
            "vfmul.vf v0, v0, %[hx] \n\t"
            "vfmv.v.f v1, %[y] \n\t"
            "vfmul.vv v2, v1, v1 \n\t"
            "vfmul.vv v2, v0, v2 \n\t"
            "vfrsub.vf v2, v2, %[th] \n\t"
            "vfmul.vv v1, v1, v2 \n\t"
            "vfmv.f.s %[r], v1 \n\t"
            : [r]"=f"(y)
            : [ms]"f"(mean_sq), [y]"f"(y), [hx]"f"(half_h), [th]"f"(threehalves)
            : "v0", "v1", "v2"
        );
        asm volatile(
            "vsetivli zero, 1, e16, m1, ta, ma \n\t"
            "vfmv.v.f v0, %[v] \n\t"
            "vse16.v  v0, (%[dst]) \n\t"
            : : [v]"f"(y), [dst]"r"(g_inv_rms) : "v0", "memory"
        );
    }
    snrt_cluster_hw_barrier();

    /* ==================================================================
     * Pass 2 — parallel normalize
     * ================================================================== */
    float inv;
    asm volatile(
        "vsetivli zero, 1, e16, m1, ta, ma \n\t"
        "vle16.v  v0, (%[src]) \n\t"
        "vfmv.f.s %[v], v0 \n\t"
        : [v]"=f"(inv) : [src]"r"(g_inv_rms) : "v0"
    );

    if (len > 0)
        pass2_normalize(g_inp + start, g_out + start, len, inv);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[RMSNORM 2-CORE BF16] cycles=%u  cores=%u  N=%d\n",
               cyc, SM_CORES, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}