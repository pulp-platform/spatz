/*  LogCosh Loss custom – using HW vfcoshf.v and vflog.v instructions
 *
 *  Kernel:  L = (1/N) · Σ ln( cosh( y_pred[i] - y_true[i] ) )
 *
 *  Per-element decomposition:
 *    1. d   = y_pred - y_true                            [vfsub]
 *    2. c   = vfcoshf.v(d)                               [1 HW instruction]
 *    3. l   = vflog.v(c)                                 [1 HW instruction]
 *    4. accumulate l for mean reduction
 *
 *  HW: 2 custom instructions replace ~12 SW instructions (Schraudolph ×2 +
 *      add + mul + bit-sub + cvt + mul).
 *
 *  LMUL = 4 (e32)
 *
 *  Register plan:
 *   v0  : pred      v4  : true / diff
 *   v8  : cosh(d)   v12 : ln(cosh(d))
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

static void logcosh_custom(const float *pred, const float *truth,
                            float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pp])              \n\t"   /* pred */
            "vle32.v    v4,  (%[pt])              \n\t"   /* true */
            "vfsub.vv   v4,  v0,  v4              \n\t"   /* d = pred-true */
            "vfcoshf.v  v8,  v4                   \n\t"   /* cosh(d) HW */
            "vflog.v    v12, v8                   \n\t"   /* ln(cosh(d)) HW */
            "vse32.v    v12, (%[po])              \n\t"
            :
            : [pp]"r"(pred), [pt]"r"(truth), [po]"r"(out)
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
        logcosh_custom(g_pred + start, g_true + start,
                       g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[LOGCOSH CUSTOM] cycles=%u  cores=%u  N=%d\n",
               cyc, cores, N);
        check_result(g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
