/*  Stable Softmax custom – using HW vfexpf.v instruction
 *
 *  Kernel:  softmax_i = exp(x_i - max) / Σ exp(x_j - max)
 *
 *  Three passes (parallelized across SM_CORES):
 *    Pass 1: each core finds local max via vfredmax.vs
 *             → barrier → core 0 reduces partial maxes → global max
 *    Pass 2: each core computes exp(x-max) via HW vfexpf.v and
 *             accumulates local sum via vfredusum.vs
 *             → barrier → core 0 reduces partial sums → global sum
 *             → core 0 computes inv_sum via scalar Newton-Raphson (2 steps)
 *    Pass 3: → barrier → each core multiplies its chunk by inv_sum
 *
 *  HW instructions: vfexpf.v replaces Schraudolph (3→1 insn)
 *  Reciprocal:      scalar Newton-Raphson (magic seed, 2 NR steps → ~28-bit)
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


/* Number of cores to use (≤ cluster core count) */
#define SM_CORES 2

/* ---- Newton-Raphson reciprocal seed ---- */
#define H_REC_MAGIC 0x7BFFu

/* ---- Shared state in L1 (allocated by core 0) ---- */
static uint16_t *g_inp, *g_exp, *g_out;
static uint16_t *g_partial_max;   /* SM_CORES floats */
static uint16_t *g_partial_sum;   /* SM_CORES floats */
static uint16_t *g_inv_sum;       /* 1 float – broadcast */

/* -----------------------------------------------------------------------
 * pass1_local_max – vectorised max reduction over a contiguous chunk.
 * Writes the chunk maximum into g_partial_max[cid].
 * ----------------------------------------------------------------------- */
static void pass1_local_max(const uint16_t *src, int len, int cid) {
    float neg_inf = h_scalar(0xFC00u);

    /* init scalar accumulator v8[0] = -∞ */
    asm volatile(
        "vsetivli  zero, 1, e16, m1, ta, ma  \n\t"
        "vfmv.s.f  v8, %[ni]                 \n\t"
        : : [ni]"f"(neg_inf) : "v8"
    );

    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v     v0, (%[s])           \n\t"
            "vfredmax.vs v8, v0, v8           \n\t"
            : : [s]"r"(src)
            : "memory", "v0","v1","v2","v3", "v8"
        );
        src += vl;
        rem -= vl;
    }

    /* extract and store */
    float local_max;
    asm volatile(
        "vsetivli  zero, 1, e16, m1, ta, ma \n\t"
        "vfmv.f.s  %[mx], v8 \n\t"
        : [mx]"=f"(local_max) : :
    );
    uint32_t mx_bits;
    asm volatile("fmv.x.w %0, %1" : "=r"(mx_bits) : "f"(local_max));
    g_partial_max[cid] = (uint16_t)(mx_bits & 0xFFFFu);
}

/* -----------------------------------------------------------------------
 * pass2_exp_and_sum – HW vfexpf.v exp(x-max) + vfredusum local sum.
 * Stores exp results into exp_buf and partial sum into g_partial_sum[cid].
 * ----------------------------------------------------------------------- */
static void pass2_exp_and_sum(const uint16_t *src, uint16_t *dst,
                               int len, float max_val, int cid) {
    float zero_f = h_scalar(0x0000u);

    /* init scalar sum accumulator v12[0] = 0 */
    asm volatile(
        "vsetivli  zero, 1, e16, m1, ta, ma  \n\t"
        "vfmv.s.f  v12, %[z]                 \n\t"
        : : [z]"f"(zero_f) : "v12"
    );

    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[src])             \n\t"
            "vfsub.vf   v0,  v0,  %[mx]           \n\t"   /* x - max */
            "vfexpf.v   v4,  v0                   \n\t"   /* exp(x-max) HW */
            "vse16.v    v4,  (%[dst])              \n\t"
            "vfredusum.vs v12, v4, v12             \n\t"   /* accumulate sum */
            :
            : [src]"r"(src), [dst]"r"(dst), [mx]"f"(max_val)
            : "memory",
              "v0","v1","v2","v3",
              "v4","v5","v6","v7",
              "v12"
        );
        src += vl;
        dst += vl;
        rem -= vl;
    }

    /* extract and store */
    float local_sum;
    asm volatile(
        "vsetivli  zero, 1, e16, m1, ta, ma \n\t"
        "vfmv.f.s  %[s], v12 \n\t"
        : [s]"=f"(local_sum) : :
    );
    uint32_t sum_bits;
    asm volatile("fmv.x.w %0, %1" : "=r"(sum_bits) : "f"(local_sum));
    g_partial_sum[cid] = (uint16_t)(sum_bits & 0xFFFFu);
}

/* -----------------------------------------------------------------------
 * pass3_normalize – multiply chunk by shared inv_sum.
 * ----------------------------------------------------------------------- */
static void pass3_normalize(const uint16_t *src, uint16_t *dst,
                             int len, float inv) {
    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[src])             \n\t"
            "vfmul.vf   v0,  v0,  %[inv]          \n\t"
            "vse16.v    v0,  (%[dst])              \n\t"
            :
            : [src]"r"(src), [dst]"r"(dst), [inv]"f"(inv)
            : "memory", "v0","v1","v2","v3"
        );
        src += vl;
        dst += vl;
        rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const uint16_t *input, const uint16_t *x, int r) {
    /* sum check skipped for 16-bit mode */
    printf("  [16-bit mode]\n");
    for (int i = 0; i < 8 && i < r; i++)
        printf("  x[%d]=0x%04X  sm=0x%04X\n", i, (unsigned)input[i], (unsigned)x[i]);
}

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    /* ---- Allocation (core 0) ---- */
    if (cid == 0) {
        g_inp         = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_exp         = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_out         = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_partial_max = (uint16_t *)snrt_l1alloc(SM_CORES * sizeof(uint16_t));
        g_partial_sum = (uint16_t *)snrt_l1alloc(SM_CORES * sizeof(uint16_t));
        g_inv_sum     = (uint16_t *)snrt_l1alloc(1 * sizeof(uint16_t));
        snrt_dma_start_1d(g_inp, data_signed, N * sizeof(uint16_t));
        snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();

    /* Idle cores hit every barrier then exit */
    if (cid >= SM_CORES) {
        snrt_cluster_hw_barrier();   /* after pass 1 */
        snrt_cluster_hw_barrier();   /* after global max */
        snrt_cluster_hw_barrier();   /* after pass 2 */
        snrt_cluster_hw_barrier();   /* after global sum + inv */
        snrt_cluster_hw_barrier();   /* after pass 3 */
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
     * Pass 1 — parallel local max, then core 0 reduces
     * ================================================================== */
    if (len > 0)
        pass1_local_max(g_inp + start, len, cid);

    snrt_cluster_hw_barrier();

    float global_max;
    if (cid == 0) {
        /* scalar max reduction – widen to FP32 for comparison */
        float gmax = h_scalar(g_partial_max[0]);
        float gmf;
        asm volatile("fcvt.s.h %0, %1" : "=f"(gmf) : "f"(gmax));
        for (int c = 1; c < SM_CORES; c++) {
            float cand = h_scalar(g_partial_max[c]);
            float cf;
            asm volatile("fcvt.s.h %0, %1" : "=f"(cf) : "f"(cand));
            if (cf > gmf) { gmax = cand; gmf = cf; }
        }
        /* store back to broadcast slot */
        uint32_t gm_bits;
        asm volatile("fmv.x.w %0, %1" : "=r"(gm_bits) : "f"(gmax));
        g_partial_max[0] = (uint16_t)(gm_bits & 0xFFFFu);
    }
    snrt_cluster_hw_barrier();
    global_max = h_scalar(g_partial_max[0]);   /* all active cores read broadcast */

    /* ==================================================================
     * Pass 2 — parallel HW exp + local sum, then core 0 reduces
     * ================================================================== */
    if (len > 0)
        pass2_exp_and_sum(g_inp + start, g_exp + start,
                          len, global_max, cid);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        /* scalar sum reduction – widen partials to FP32 and accumulate */
        float total_sum_f32 = 0.0f;
        for (int c = 0; c < SM_CORES; c++) {
            float ps = h_scalar(g_partial_sum[c]);
            float psf;
            asm volatile("fcvt.s.h %0, %1" : "=f"(psf) : "f"(ps));
            total_sum_f32 += psf;
        }

        /* Scalar Newton-Raphson reciprocal (magic seed, 2 steps → ~28-bit) */
        union { float f; uint32_t u; } seed, tmp;
        tmp.f = total_sum_f32;
        seed.u = 0x7EFFFFFFu - tmp.u;
        float y = seed.f;
        y = y * (2.0f - total_sum_f32 * y);   /* NR step 1 → ~14 bits */
        y = y * (2.0f - total_sum_f32 * y);   /* NR step 2 → ~28 bits */
        /* Narrow back to half and store */
        float inv_h;
        asm volatile("fcvt.h.s %0, %1" : "=f"(inv_h) : "f"(y));
        uint32_t inv_bits;
        asm volatile("fmv.x.w %0, %1" : "=r"(inv_bits) : "f"(inv_h));
        *g_inv_sum = (uint16_t)(inv_bits & 0xFFFFu);
    }
    snrt_cluster_hw_barrier();

    /* ==================================================================
     * Pass 3 — parallel normalize
     * ================================================================== */
    float inv = h_scalar(*g_inv_sum);   /* all active cores read broadcast */

    if (len > 0)
        pass3_normalize(g_exp + start, g_out + start, len, inv);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[SOFTMAX CUSTOM 2-CORE FP16] cycles=%u  cores=%u  N=%d\n",
               cyc, SM_CORES, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}