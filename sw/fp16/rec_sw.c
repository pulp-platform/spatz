/*  rec_sw.c – Software reciprocal via Newton-Raphson (FP16)
 *
 *  Algorithm: seed = M − x, then 2 NR iterations: acc=2; acc-=a*x; x*=acc
 *  Compile with -DLMUL_MODE={1,2,4,8}
 */

#include "snrt.h"
#include "printf.h"
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"
#include "sanity_check.h"

#include <stdint.h>

#ifndef LMUL_MODE
#define LMUL_MODE 8
#endif

/* bf16_scalar: NaN-box BF16 bit pattern into FP register bits[15:0] */
static inline float bf16_scalar(uint32_t bf16_hex) {
    float f;
    uint32_t nanboxed = 0xFFFF0000u | bf16_hex;
    asm volatile("fmv.w.x %0, %1" : "=f"(f) : "r"(nanboxed));
    return f;
}

/* FP16 constants */
#define FP16_M    0x77FFu   /* magic constant for reciprocal */
#define FP16_TWO  0x4000u   /* 2.0 in bf16 */

/* ========================= LMUL = 8 ========================= */

static inline void vrec_m8(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    float two_val = bf16_scalar(FP16_TWO);
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            /* load a */
            "vle16.v    v0,  (%[pin])          \n\t"
            /* x0 = M - as_int(a) (bit trick initial guess) */
            "li         t0,  0x77FF            \n\t"
            "vrsub.vx   v8,  v0,  t0           \n\t"
            /* NR iteration 1: x1 = x0 * (2 - a * x0) */
            "vfmul.vv   v16, v0,  v8           \n\t"  /* a * x0 */
            "vfrsub.vf  v16, v16, %[two]       \n\t"  /* 2 - a*x0 */
            "vfmul.vv   v8,  v8,  v16          \n\t"  /* x0 * (2-a*x0) */
            /* NR iteration 2: x2 = x1 * (2 - a * x1) */
            "vfmul.vv   v24, v0,  v8           \n\t"
            "vfrsub.vf  v24, v24, %[two]       \n\t"
            "vfmul.vv   v8,  v8,  v24          \n\t"
            "vse16.v    v8,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [two]"f"(two_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vrec_m4(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    float two_val = bf16_scalar(FP16_TWO);
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            /* load a */
            "vle16.v    v0,  (%[pin])          \n\t"
            /* x0 = M - as_int(a) (bit trick initial guess) */
            "li         t0,  0x77FF            \n\t"
            "vrsub.vx   v8,  v0,  t0           \n\t"
            /* NR iteration 1: x1 = x0 * (2 - a * x0) */
            "vfmul.vv   v16, v0,  v8           \n\t"  /* a * x0 */
            "vfrsub.vf  v16, v16, %[two]       \n\t"  /* 2 - a*x0 */
            "vfmul.vv   v8,  v8,  v16          \n\t"  /* x0 * (2-a*x0) */
            /* NR iteration 2: x2 = x1 * (2 - a * x1) */
            "vfmul.vv   v24, v0,  v8           \n\t"
            "vfrsub.vf  v24, v24, %[two]       \n\t"
            "vfmul.vv   v8,  v8,  v24          \n\t"
            "vse16.v    v8,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [two]"f"(two_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vrec_m2(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    float two_val = bf16_scalar(FP16_TWO);
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            /* load a */
            "vle16.v    v0,  (%[pin])          \n\t"
            /* x0 = M - as_int(a) (bit trick initial guess) */
            "li         t0,  0x77FF            \n\t"
            "vrsub.vx   v8,  v0,  t0           \n\t"
            /* NR iteration 1: x1 = x0 * (2 - a * x0) */
            "vfmul.vv   v16, v0,  v8           \n\t"  /* a * x0 */
            "vfrsub.vf  v16, v16, %[two]       \n\t"  /* 2 - a*x0 */
            "vfmul.vv   v8,  v8,  v16          \n\t"  /* x0 * (2-a*x0) */
            /* NR iteration 2: x2 = x1 * (2 - a * x1) */
            "vfmul.vv   v24, v0,  v8           \n\t"
            "vfrsub.vf  v24, v24, %[two]       \n\t"
            "vfmul.vv   v8,  v8,  v24          \n\t"
            "vse16.v    v8,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [two]"f"(two_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vrec_m1(const uint16_t *inp, uint16_t *out, int N) {
    int rem = N;
    float two_val = bf16_scalar(FP16_TWO);
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            /* load a */
            "vle16.v    v0,  (%[pin])          \n\t"
            /* x0 = M - as_int(a) (bit trick initial guess) */
            "li         t0,  0x77FF            \n\t"
            "vrsub.vx   v8,  v0,  t0           \n\t"
            /* NR iteration 1: x1 = x0 * (2 - a * x0) */
            "vfmul.vv   v16, v0,  v8           \n\t"  /* a * x0 */
            "vfrsub.vf  v16, v16, %[two]       \n\t"  /* 2 - a*x0 */
            "vfmul.vv   v8,  v8,  v16          \n\t"  /* x0 * (2-a*x0) */
            /* NR iteration 2: x2 = x1 * (2 - a * x1) */
            "vfmul.vv   v24, v0,  v8           \n\t"
            "vfrsub.vf  v24, v24, %[two]       \n\t"
            "vfmul.vv   v8,  v8,  v24          \n\t"
            "vse16.v    v8,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [two]"f"(two_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vrec_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vrec_m8(inp, out, N);
#elif LMUL_MODE == 4
    vrec_m4(inp, out, N);
#elif LMUL_MODE == 2
    vrec_m2(inp, out, N);
#elif LMUL_MODE == 1
    vrec_m1(inp, out, N);
#else
#   error "LMUL_MODE must be 1, 2, 4 or 8"
#endif
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
        snrt_dma_start_1d(g_inp, data_positive, N * sizeof(uint16_t));
        snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();

    const int start = (int)((int64_t)cid       * N / (int64_t)cores);
    const int end   = (int)((int64_t)(cid + 1) * N / (int64_t)cores);
    const int len   = end - start;

    snrt_cluster_hw_barrier();

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0) vrec_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[REC_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_fp16(g_out, outRec, N, "REC_SW");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
