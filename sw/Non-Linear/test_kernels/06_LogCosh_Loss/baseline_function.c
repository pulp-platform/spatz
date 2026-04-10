/*  LogCosh Loss baseline – using SW cosh and SW ln (RV64GCV)
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
#include "../data/data.h"
#endif
#include "../benchmark/benchmark.c"

#include <math.h>
#include <stdint.h>

/* ---- Schraudolph exp constants ---- */
static const float SCH_C = 12102203.0f;
static const float SCH_B = 1064866805.0f;

/* ---- Fast ln constants ---- */
static const unsigned int LN_BIAS  = 0x3F800000u;
static const float        LN_SCALE = 8.26295829e-8f;

static void logcosh_baseline(const float *pred, const float *truth,
                              float *out, int N) {
    float half = 0.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            /* 1. d = pred - true */
            "vle32.v    v0,  (%[pp])              \n\t"
            "vle32.v    v4,  (%[pt])              \n\t"
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

            "vse32.v    v20, (%[po])               \n\t"
            :
            : [pp]"r"(pred), [pt]"r"(truth), [po]"r"(out),
              [schB]"f"(SCH_B), [schC]"f"(SCH_C),
              [half]"f"(half),
              [lnB]"r"(LN_BIAS), [lnS]"f"(LN_SCALE)
            : "memory"
        );
        pred += vl; truth += vl; out += vl; rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const float *x, int r) {
    float sum = 0.0f;
    for (int i = 0; i < r; i++) sum += x[i];
    { unsigned us; __builtin_memcpy(&us, &sum, 4);
      printf("  sum_loss = 0x%08X  n=%d\n", us, r); }
    for (int i = 0; i < 8 && i < r; i++) {
        unsigned u; __builtin_memcpy(&u, &x[i], 4);
        printf("  loss[%d] = 0x%08X\n", i, u);
    }
}

static float *g_pred, *g_true, *g_out;

int main(void) {
    const unsigned cid   = snrt_cluster_core_idx();
    const unsigned cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int N = B_Size * T_Size * C_Size;

    if (cid == 0) {
        g_pred = (float *)snrt_l1alloc(N * sizeof(float));
        g_true = (float *)snrt_l1alloc(N * sizeof(float));
        g_out  = (float *)snrt_l1alloc(N * sizeof(float));

        /* pred = data_signed[0..N-1], true = shifted version */
        snrt_dma_start_1d(g_pred, data_signed, N * sizeof(float));
        snrt_dma_wait_all();
        for (int i = 0; i < N; i++)
            g_true[i] = data_signed[(i + N / 4) % N] * 0.8f;
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
        printf("[LOGCOSH BASELINE] cycles=%u  cores=%u  N=%d\n",
               cyc, cores, N);
        check_result(g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
