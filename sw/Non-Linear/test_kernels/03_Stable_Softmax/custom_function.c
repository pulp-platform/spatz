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
#include "../data/data.h"
#endif
#include "../benchmark/benchmark.c"

#include <math.h>
#include <stdint.h>

/* Number of cores to use (≤ cluster core count) */
#define SM_CORES 2

/* ---- Newton-Raphson reciprocal seed ---- */
static const unsigned int REC_MAGIC = 0x7EFFFFFFu;

/* ---- Shared state in L1 (allocated by core 0) ---- */
static float *g_inp, *g_exp, *g_out;
static float *g_partial_max;   /* SM_CORES floats */
static float *g_partial_sum;   /* SM_CORES floats */
static float *g_inv_sum;       /* 1 float – broadcast */

/* -----------------------------------------------------------------------
 * pass1_local_max – vectorised max reduction over a contiguous chunk.
 * Writes the chunk maximum into g_partial_max[cid].
 * ----------------------------------------------------------------------- */
static void pass1_local_max(const float *src, int len, int cid) {
    float neg_inf = -__builtin_inff();

    /* init scalar accumulator v8[0] = -∞ */
    asm volatile(
        "vsetivli  zero, 1, e32, m1, ta, ma  \n\t"
        "vfmv.s.f  v8, %[ni]                 \n\t"
        : : [ni]"f"(neg_inf) : "v8"
    );

    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v     v0, (%[s])           \n\t"
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
        "vsetivli  zero, 1, e32, m1, ta, ma  \n\t"
        "vfmv.f.s  %[mx], v8                 \n\t"
        : [mx]"=f"(local_max) : : "v8"
    );
    g_partial_max[cid] = local_max;
}

/* -----------------------------------------------------------------------
 * pass2_exp_and_sum – HW vfexpf.v exp(x-max) + vfredusum local sum.
 * Stores exp results into exp_buf and partial sum into g_partial_sum[cid].
 * ----------------------------------------------------------------------- */
static void pass2_exp_and_sum(const float *src, float *dst,
                               int len, float max_val, int cid) {
    float zero_f = 0.0f;

    /* init scalar sum accumulator v12[0] = 0 */
    asm volatile(
        "vsetivli  zero, 1, e32, m1, ta, ma  \n\t"
        "vfmv.s.f  v12, %[z]                 \n\t"
        : : [z]"f"(zero_f) : "v12"
    );

    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[src])             \n\t"
            "vfsub.vf   v0,  v0,  %[mx]           \n\t"   /* x - max */
            "vfexpf.v   v4,  v0                   \n\t"   /* exp(x-max) HW */
            "vse32.v    v4,  (%[dst])              \n\t"
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
        "vsetivli  zero, 1, e32, m1, ta, ma  \n\t"
        "vfmv.f.s  %[s], v12                 \n\t"
        : [s]"=f"(local_sum) : : "v12"
    );
    g_partial_sum[cid] = local_sum;
}

/* -----------------------------------------------------------------------
 * pass3_normalize – multiply chunk by shared inv_sum.
 * ----------------------------------------------------------------------- */
static void pass3_normalize(const float *src, float *dst,
                             int len, float inv) {
    int rem = len;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[src])             \n\t"
            "vfmul.vf   v0,  v0,  %[inv]          \n\t"
            "vse32.v    v0,  (%[dst])              \n\t"
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
static void check_result(const float *input, const float *x, int r) {
    float sum = 0.0f;
    for (int i = 0; i < r; i++) sum += x[i];
    { unsigned us; __builtin_memcpy(&us, &sum, 4);
      printf("  sum(softmax) = 0x%08X  (should be ≈1.0)\n", us); }
    for (int i = 0; i < 8 && i < r; i++) {
        unsigned ui, uo; __builtin_memcpy(&ui, &input[i], 4); __builtin_memcpy(&uo, &x[i], 4);
        printf("  x[%d]=0x%08X  sm=0x%08X\n", i, ui, uo);
    }
}

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    /* ---- Allocation (core 0) ---- */
    if (cid == 0) {
        g_inp         = (float *)snrt_l1alloc(N * sizeof(float));
        g_exp         = (float *)snrt_l1alloc(N * sizeof(float));
        g_out         = (float *)snrt_l1alloc(N * sizeof(float));
        g_partial_max = (float *)snrt_l1alloc(SM_CORES * sizeof(float));
        g_partial_sum = (float *)snrt_l1alloc(SM_CORES * sizeof(float));
        g_inv_sum     = (float *)snrt_l1alloc(1 * sizeof(float));
        snrt_dma_start_1d(g_inp, data_signed, N * sizeof(float));
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
        global_max = g_partial_max[0];
        for (int c = 1; c < SM_CORES; c++)
            if (g_partial_max[c] > global_max)
                global_max = g_partial_max[c];
        g_partial_max[0] = global_max;   /* broadcast slot */
    }
    snrt_cluster_hw_barrier();
    global_max = g_partial_max[0];

    /* ==================================================================
     * Pass 2 — parallel HW exp + local sum, then core 0 reduces
     * ================================================================== */
    if (len > 0)
        pass2_exp_and_sum(g_inp + start, g_exp + start,
                          len, global_max, cid);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        float total_sum = 0.0f;
        for (int c = 0; c < SM_CORES; c++)
            total_sum += g_partial_sum[c];

        /* Scalar Newton-Raphson reciprocal (magic seed, 2 steps → ~28-bit) */
        union { float f; uint32_t u; } seed, tmp;
        tmp.f = total_sum;
        seed.u = REC_MAGIC - tmp.u;
        float y = seed.f;
        y = y * (2.0f - total_sum * y);   /* NR step 1 → ~14 bits */
        y = y * (2.0f - total_sum * y);   /* NR step 2 → ~28 bits */
        *g_inv_sum = y;
    }
    snrt_cluster_hw_barrier();

    /* ==================================================================
     * Pass 3 — parallel normalize
     * ================================================================== */
    float inv = *g_inv_sum;

    if (len > 0)
        pass3_normalize(g_exp + start, g_out + start, len, inv);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[SOFTMAX CUSTOM 2-CORE] cycles=%u  cores=%u  N=%d\n",
               cyc, SM_CORES, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}