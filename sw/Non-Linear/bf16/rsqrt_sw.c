/*  rsqrt_sw.c – Quake fast inverse sqrt for BF16
 *
 *  Algorithm: x0 = as_float(M - (as_int(a) >> 1)), then 2 NR iterations
 *  BF16 M = 0x5F37, half = 0.5, threehalfs = 1.5
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

/* BF16 constants */
#define BF16_M     0x5F37u   /* magic constant for rsqrt */
#define BF16_HALF  0x3F00u   /* 0.5 in bf16 */
#define BF16_P5    0x3FC0u   /* 1.5 in bf16 */

/* ========================= LMUL = 8 ========================= */
/*  m8 uses split asm blocks because we need v0..v24 and a spill buffer */
static inline void vrsqrt_m8(const uint16_t *inp, uint16_t *out, int N) {
    float half_val = bf16_scalar(BF16_HALF);
    float p5_val   = bf16_scalar(BF16_P5);
    uint16_t half_buf[256] __attribute__((aligned(64)));
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(rem));
        /* Stage 1: initial guess */
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            /* half_a = a * 0.5 */
            "vfmul.vf   v8,  v0,  %[half]     \n\t"
            /* spill half_a */
            "vse16.v    v8,  (%[hbuf])         \n\t"
            /* x0 = M - (as_int(a) >> 1) */
            "vsrl.vi    v16, v0,  1            \n\t"
            "li         t0,  0x5F37            \n\t"
            "vrsub.vx   v0,  v16, t0           \n\t"
            :
            : [pin]"r"(inp), [hbuf]"r"(half_buf),
              [half]"f"(half_val)
            : "memory", "t0"
        );
        /* Stage 2: NR iteration 1 */
        asm volatile(
            /* reload half_a */
            "vle16.v    v8,  (%[hbuf])         \n\t"
            /* t = half_a * x0 * x0 */
            "vfmul.vv   v16, v0,  v0           \n\t"
            "vfmul.vv   v16, v16, v8           \n\t"
            /* x1 = x0 * (1.5 - t) */
            "vfrsub.vf  v16, v16, %[p5]        \n\t"
            "vfmul.vv   v0,  v0,  v16          \n\t"
            :
            : [hbuf]"r"(half_buf), [p5]"f"(p5_val)
            : "memory"
        );
        /* Stage 3: NR iteration 2 */
        asm volatile(
            "vle16.v    v8,  (%[hbuf])         \n\t"
            "vfmul.vv   v24, v0,  v0           \n\t"
            "vfmul.vv   v24, v24, v8           \n\t"
            "vfrsub.vf  v24, v24, %[p5]        \n\t"
            "vfmul.vv   v0,  v0,  v24          \n\t"
            "vse16.v    v0,  (%[po])           \n\t"
            :
            : [hbuf]"r"(half_buf), [po]"r"(out),
              [p5]"f"(p5_val)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vrsqrt_m4(const uint16_t *inp, uint16_t *out, int N) {
    float half_val = bf16_scalar(BF16_HALF);
    float p5_val   = bf16_scalar(BF16_P5);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v4,  v0,  %[half]     \n\t"
            "vsrl.vi    v8,  v0,  1            \n\t"
            "li         t0,  0x5F37            \n\t"
            "vrsub.vx   v0,  v8,  t0           \n\t"
            /* NR 1 */
            "vfmul.vv   v8,  v0,  v0           \n\t"
            "vfmul.vv   v8,  v8,  v4           \n\t"
            "vfrsub.vf  v8,  v8,  %[p5]        \n\t"
            "vfmul.vv   v0,  v0,  v8           \n\t"
            /* NR 2 */
            "vfmul.vv   v12, v0,  v0           \n\t"
            "vfmul.vv   v12, v12, v4           \n\t"
            "vfrsub.vf  v12, v12, %[p5]        \n\t"
            "vfmul.vv   v0,  v0,  v12          \n\t"
            "vse16.v    v0,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [half]"f"(half_val), [p5]"f"(p5_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vrsqrt_m2(const uint16_t *inp, uint16_t *out, int N) {
    float half_val = bf16_scalar(BF16_HALF);
    float p5_val   = bf16_scalar(BF16_P5);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v2,  v0,  %[half]     \n\t"
            "vsrl.vi    v4,  v0,  1            \n\t"
            "li         t0,  0x5F37            \n\t"
            "vrsub.vx   v0,  v4,  t0           \n\t"
            /* NR 1 */
            "vfmul.vv   v4,  v0,  v0           \n\t"
            "vfmul.vv   v4,  v4,  v2           \n\t"
            "vfrsub.vf  v4,  v4,  %[p5]        \n\t"
            "vfmul.vv   v0,  v0,  v4           \n\t"
            /* NR 2 */
            "vfmul.vv   v6,  v0,  v0           \n\t"
            "vfmul.vv   v6,  v6,  v2           \n\t"
            "vfrsub.vf  v6,  v6,  %[p5]        \n\t"
            "vfmul.vv   v0,  v0,  v6           \n\t"
            "vse16.v    v0,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [half]"f"(half_val), [p5]"f"(p5_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vrsqrt_m1(const uint16_t *inp, uint16_t *out, int N) {
    float half_val = bf16_scalar(BF16_HALF);
    float p5_val   = bf16_scalar(BF16_P5);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v1,  v0,  %[half]     \n\t"
            "vsrl.vi    v2,  v0,  1            \n\t"
            "li         t0,  0x5F37            \n\t"
            "vrsub.vx   v0,  v2,  t0           \n\t"
            /* NR 1 */
            "vfmul.vv   v2,  v0,  v0           \n\t"
            "vfmul.vv   v2,  v2,  v1           \n\t"
            "vfrsub.vf  v2,  v2,  %[p5]        \n\t"
            "vfmul.vv   v0,  v0,  v2           \n\t"
            /* NR 2 */
            "vfmul.vv   v3,  v0,  v0           \n\t"
            "vfmul.vv   v3,  v3,  v1           \n\t"
            "vfrsub.vf  v3,  v3,  %[p5]        \n\t"
            "vfmul.vv   v0,  v0,  v3           \n\t"
            "vse16.v    v0,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [half]"f"(half_val), [p5]"f"(p5_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vrsqrt_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vrsqrt_m8(inp, out, N);
#elif LMUL_MODE == 4
    vrsqrt_m4(inp, out, N);
#elif LMUL_MODE == 2
    vrsqrt_m2(inp, out, N);
#elif LMUL_MODE == 1
    vrsqrt_m1(inp, out, N);
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

    asm volatile("csrwi 0x800, 3" ::: "memory");

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0) vrsqrt_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[RSQRT_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_bf16(g_out, outR, N, "RSQRT_SW");
    }

    asm volatile("csrwi 0x800, 0" ::: "memory");
    snrt_cluster_hw_barrier();
    return 0;
}
