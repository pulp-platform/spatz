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


static void mish_custom(const uint16_t *inp, uint16_t *out, int N) {
    float one  = h_scalar(0x3F80u);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])             \n\t"   /* x */
            "vfexpf.v   v4,  v0                   \n\t"   /* exp(x) */
            "vfadd.vf   v8,  v4,  %[one]          \n\t"   /* 1 + exp(x) */
            "vflog.v    v8,  v8                   \n\t"   /* softplus */
            "vftanhf.v  v12, v8                   \n\t"   /* tanh(sp) */
            "vfmul.vv   v16, v0,  v12             \n\t"   /* x · tanh(sp) */
            "vse16.v    v16, (%[po])              \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out), [one]"f"(one)
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

    if (len > 0) mish_custom(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[MISH CUSTOM BF16] cycles=%u  cores=%u  N=%d\n", cyc, cores, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
