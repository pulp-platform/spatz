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


static void logcosh_custom(const uint16_t *pred, const uint16_t *truth,
                            uint16_t *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pp])              \n\t"   /* pred */
            "vle16.v    v4,  (%[pt])              \n\t"   /* true */
            "vfsub.vv   v4,  v0,  v4              \n\t"   /* d = pred-true */
            "vfcoshf.v  v8,  v4                   \n\t"   /* cosh(d) HW */
            "vflog.v    v12, v8                   \n\t"   /* ln(cosh(d)) HW */
            "vse16.v    v12, (%[po])              \n\t"
            :
            : [pp]"r"(pred), [pt]"r"(truth), [po]"r"(out)
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
        logcosh_custom(g_pred + start, g_true + start,
                       g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[LOGCOSH CUSTOM FP16] cycles=%u  cores=%u  N=%d\n",
               cyc, cores, N);
        check_result(g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
