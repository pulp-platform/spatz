/*  GeLU (Tanh approx.) custom – using HW vftanhf.v instruction
 *
 *  Kernel:  GeLU(x) ≈ 0.5 · x · (1 + tanh( √(2/π) · (x + 0.044715·x³) ))
 *
 *  Decomposition:
 *    1. x3    = x · x · x                               [vfmul ×2]
 *    2. inner = √(2/π) · (x + 0.044715·x³)              [vfmacc + vfmul]
 *    3. t     = vftanhf.v(inner)                         [1 HW instruction]
 *    4. out   = 0.5 · x · (1 + t)                       [vfadd + vfmul ×2]
 *
 *  HW tanh replaces 7-instruction SW polynomial → 1 instruction.
 *
 *  LMUL = 4 (e32)
 *
 *  Register plan:
 *   v0  : x (kept)    v4  : x² / inner
 *   v8  : x³          v12 : tanh(inner)
 *   v16 : result
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

/* ---- GeLU constants ---- */
static const float GELU_COEFF = 0.044715f;
static const float SQRT_2_PI  = 0.7978845608f;

static void gelu_custom(const float *inp, float *out, int N) {
    float half = 0.5f;
    float one  = 1.0f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])             \n\t"   /* x */

            /* inner = √(2/π) · (x + 0.044715·x³) */
            "vfmul.vv   v4,  v0,  v0              \n\t"   /* x² */
            "vfmul.vv   v8,  v4,  v0              \n\t"   /* x³ */
            "vmv.v.v    v12, v0                   \n\t"   /* copy x */
            "vfmacc.vf  v12, %[gc], v8            \n\t"   /* x+gc·x³ */
            "vfmul.vf   v4,  v12, %[sq]           \n\t"   /* inner → v4 */

            /* tanh via HW */
            "vftanhf.v  v8,  v4                   \n\t"   /* tanh(inner) → v8 */

            /* GeLU = 0.5 · x · (1 + tanh) */
            "vfadd.vf   v8,  v8,  %[one]          \n\t"   /* 1 + tanh */
            "vfmul.vv   v12, v0,  v8              \n\t"   /* x · (1+tanh) */
            "vfmul.vf   v12, v12, %[half]         \n\t"   /* 0.5 · … */
            "vse32.v    v12, (%[po])              \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [gc]"f"(GELU_COEFF), [sq]"f"(SQRT_2_PI),
              [half]"f"(half), [one]"f"(one)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ---- validation ---- */
static void check_result(const float *input, const float *x, int r) {
    for (int i = 0; i < r && i < 8; i++) {
        unsigned ui, uo; __builtin_memcpy(&ui, &input[i], 4); __builtin_memcpy(&uo, &x[i], 4);
        printf("  x[%d]=0x%08X  gelu=0x%08X\n", i, ui, uo);
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

    if (len > 0) gelu_custom(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[GELU_TANH CUSTOM] cycles=%u  cores=%u  N=%d\n",
               cyc, cores, N);
        check_result(g_inp, g_out, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
