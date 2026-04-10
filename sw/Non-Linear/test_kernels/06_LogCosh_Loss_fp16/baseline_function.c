/*  LogCosh Loss baseline – using SW cosh and SW ln (FP16)
 *
 *  Kernel:  L = (1/N) · Σ ln( cosh( y_pred[i] - y_true[i] ) )
 *
 *  Per-element decomposition:
 *    1. d    = y_pred - y_true                           [vfsub]
 *    2. c    = cosh_sw(d)   = 0.5·(exp(d) + exp(-d))    [Schraudolph ×2 + add]
 *    3. l    = ln_sw(c)                                  [bit trick]
 *    4. accumulate l for mean reduction
 *
 *  SW cosh: Schraudolph exp for both exp(+d) and exp(-d), average
 *  SW ln:   float(int(x) - 0x3F800000) · SCALE
 *
 *  LMUL = 4 (e32) — 8 register groups
 *
 *  Register plan:
 *   v0  : d (diff)          v4  : exp(+d) bits
 *   v8  : exp(-d) bits      v12 : cosh → ln arg
 *   v16 : ln result         v20 : temp
 *   v24 : temp
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

static void logcosh_baseline(const uint16_t *pred, const uint16_t *truth,
                              uint16_t *out, int N) {
    float half = h_scalar(0x3800u);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            /* 1. d = pred - true */
            "vle16.v    v0,  (%[pp])              \n\t"
            "vle16.v    v4,  (%[pt])              \n\t"
            "vfsub.vv   v0,  v0,  v4              \n\t"   /* d → v0 */

            /* 2. cosh(d) = 0.5·(exp(d) + exp(-d)) */
            /* exp(+d) via Schraudolph */
            "vfmv.v.f   v4,  %[schB]              \n\t"
            "vfmacc.vf  v4,  %[schC], v0           \n\t"
            "vfcvt.rtz.xu.f.v v4,  v4              \n\t"   /* exp(+d) → v4 */
            /* exp(-d): negate d first */
            "vfsgnjn.vv v8,  v0,  v0              \n\t"   /* -d → v8 */
            "vfmv.v.f   v12, %[schB]              \n\t"
            "vfmacc.vf  v12, %[schC], v8           \n\t"
            "vfcvt.rtz.xu.f.v v12, v12             \n\t"   /* exp(-d) → v12 */
            /* cosh = 0.5·(exp+ + exp-) */
            "vfadd.vv   v16, v4,  v12              \n\t"
            "vfmul.vf   v16, v16, %[half]          \n\t"   /* cosh(d) → v16 */

            /* 3. ln(cosh(d)) via bit trick */
            "vsub.vx    v20, v16, %[lnB]           \n\t"
            "vfcvt.f.x.v v20, v20                  \n\t"
            "vfmul.vf   v20, v20, %[lnS]           \n\t"   /* ln(cosh) → v20 */

            "vse16.v    v20, (%[po])               \n\t"
            :
            : [pp]"r"(pred), [pt]"r"(truth), [po]"r"(out),
              [schB]"f"(h_scalar(H_SCH_B)), [schC]"f"(h_scalar(H_SCH_C)),
              [half]"f"(half),
              [lnB]"r"(((unsigned int)H_LN_BIAS)), [lnS]"f"(h_scalar(H_LN_SCALE))
            : "memory"
        );
        pred += vl; truth += vl; out += vl; rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const uint16_t *x, int r) {
    /* sum check skipped for 16-bit mode */
    printf("  [16-bit mode]\n");
    for (int i = 0; i < 8 && i < r; i++)
        printf("  loss[%d] = 0x%04X\n", i, (unsigned)x[i]);
}

static uint16_t *g_pred, *g_true, *g_out;

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    if (cid == 0) {
        g_pred = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_true = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));
        g_out  = (uint16_t *)snrt_l1alloc(N * sizeof(uint16_t));

        /* pred = data_signed[0..N-1], true = shifted version */
        snrt_dma_start_1d(g_pred, data_signed, N * sizeof(uint16_t));
        snrt_dma_wait_all();
        for (int i = 0; i < N; i++)
            g_true[i] = data_signed[(i + N / 4) % N];
    }
    snrt_cluster_hw_barrier();

    const int start = (int)((int64_t)cid       * N / (int64_t)cores);
    const int end   = (int)((int64_t)(cid + 1) * N / (int64_t)cores);
    const int len   = end - start;

    snrt_cluster_hw_barrier();

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0)
        logcosh_baseline(g_pred + start, g_true + start,
                         g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[LOGCOSH BASELINE FP16] cycles=%u  cores=%u  N=%d\n",
               cyc, cores, N);
        check_result(g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
