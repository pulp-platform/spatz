/*  Mish custom – Mish activation using HW exp, ln, tanh instructions
 *
 *  Kernel:  Mish(x) = x · tanh( ln(1 + exp(x)) )
 *
 *  Decomposition into HW primitives:
 *    1. e   = vfexpf.v(x)         [1 HW instruction]
 *    2. sp  = 1 + e               [vfadd.vf]
 *    3. sp  = vflog.v(sp)         [1 HW instruction]
 *    4. t   = vftanhf.v(sp)       [1 HW instruction]
 *    5. out = x · t               [vfmul.vv]
 *
 *  Only 3 custom instructions replace ~17 baseline instructions.
 *
 *  Register plan (LMUL=4):
 *   v0  : x (kept)   v4  : exp(x) → 1+exp
 *   v8  : softplus    v12 : tanh(sp)   v16 : result
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

static void mish_custom(const float *inp, float *out, int N) {
    float one = 1.0f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])             \n\t"   /* x */
            "vfexpf.v   v4,  v0                   \n\t"   /* exp(x) */
            "vfadd.vf   v8,  v4,  %[one]          \n\t"   /* 1 + exp(x) */
            "vflog.v    v8,  v8                   \n\t"   /* softplus */
            "vftanhf.v  v12, v8                   \n\t"   /* tanh(sp) */
            "vfmul.vv   v16, v0,  v12             \n\t"   /* x · tanh(sp) */
            "vse32.v    v16, (%[po])              \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out), [one]"f"(one)
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

    if (len > 0) mish_custom(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[MISH CUSTOM] cycles=%u  cores=%u  N=%d\n", cyc, cores, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
