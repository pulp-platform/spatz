/*  rec_sw.c – Software reciprocal via magic seed + 2 Newton-Raphson iterations
 *
 *  Algorithm:
 *    seed = reinterpret_float( MAGIC - reinterpret_int(a) )   (vrsub.vx)
 *    x = seed;  for 2 NR steps:
 *      acc = 2.0;  acc -= a * x;  x *= acc;                  (vfnmsac + vfmul)
 *
 *  NR uses vfnmsac.vv (fused multiply-negate-accumulate) for 2 FP ops/step.
 *  First 2.0 splat via vmv.v.x (integer bits), reset via vfmv.v.f.
 *
 *  MAGIC = 0x7EFFFFFFU
 *
 *  Compile with -DLMUL_MODE={1,2,4,8}
 */

#include "snrt.h"
#include "printf.h"
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"

#include <math.h>
#include <stdint.h>

#ifndef LMUL_MODE
#define LMUL_MODE 8
#endif

static const unsigned int REC_MAGIC = 0x7EFFFFFFu;

/* ========================= LMUL = 8 =========================
 * v8 = a (input, kept),  v16 = x (seed → result),  v24 = acc (2.0)
 */
static inline void vrec_sw_m8(const float *inp, float *out, int N) {
    float two = 2.0f;
    uint32_t two_bits = 0x40000000u;  /* IEEE-754 bits of 2.0f */
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v8,  (%[pin])           \n\t"
            /* seed */
            "vrsub.vx   v16, v8,  %[M]          \n\t"
            /* NR iter 1 */
            "vmv.v.x    v24, %[tb]              \n\t"
            "vfnmsac.vv v24, v8,  v16            \n\t"
            "vfmul.vv   v16, v16, v24            \n\t"
            /* NR iter 2 */
            "vfmv.v.f   v24, %[two]             \n\t"
            "vfnmsac.vv v24, v8,  v16            \n\t"
            "vfmul.vv   v16, v16, v24            \n\t"
            "vse32.v    v16, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(REC_MAGIC), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 =========================
 * v0 = a,  v4 = x,  v8 = acc
 */
static inline void vrec_sw_m4(const float *inp, float *out, int N) {
    float two = 2.0f;
    uint32_t two_bits = 0x40000000u;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])           \n\t"
            "vrsub.vx   v4,  v0,  %[M]          \n\t"
            "vmv.v.x    v8,  %[tb]              \n\t"
            "vfnmsac.vv v8,  v0,  v4             \n\t"
            "vfmul.vv   v4,  v4,  v8             \n\t"
            "vfmv.v.f   v8,  %[two]             \n\t"
            "vfnmsac.vv v8,  v0,  v4             \n\t"
            "vfmul.vv   v4,  v4,  v8             \n\t"
            "vse32.v    v4,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(REC_MAGIC), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 =========================
 * v0 = a,  v2 = x,  v4 = acc
 */
static inline void vrec_sw_m2(const float *inp, float *out, int N) {
    float two = 2.0f;
    uint32_t two_bits = 0x40000000u;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])           \n\t"
            "vrsub.vx   v2,  v0,  %[M]          \n\t"
            "vmv.v.x    v4,  %[tb]              \n\t"
            "vfnmsac.vv v4,  v0,  v2             \n\t"
            "vfmul.vv   v2,  v2,  v4             \n\t"
            "vfmv.v.f   v4,  %[two]             \n\t"
            "vfnmsac.vv v4,  v0,  v2             \n\t"
            "vfmul.vv   v2,  v2,  v4             \n\t"
            "vse32.v    v2,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(REC_MAGIC), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 =========================
 * v0 = a,  v1 = x,  v2 = acc
 */
static inline void vrec_sw_m1(const float *inp, float *out, int N) {
    float two = 2.0f;
    uint32_t two_bits = 0x40000000u;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])           \n\t"
            "vrsub.vx   v1,  v0,  %[M]          \n\t"
            "vmv.v.x    v2,  %[tb]              \n\t"
            "vfnmsac.vv v2,  v0,  v1             \n\t"
            "vfmul.vv   v1,  v1,  v2             \n\t"
            "vfmv.v.f   v2,  %[two]             \n\t"
            "vfnmsac.vv v2,  v0,  v1             \n\t"
            "vfmul.vv   v1,  v1,  v2             \n\t"
            "vse32.v    v1,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(REC_MAGIC), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vrec_sw_kernel(const float *inp, float *out, int N) {
#if   LMUL_MODE == 8
    vrec_sw_m8(inp, out, N);
#elif LMUL_MODE == 4
    vrec_sw_m4(inp, out, N);
#elif LMUL_MODE == 2
    vrec_sw_m2(inp, out, N);
#elif LMUL_MODE == 1
    vrec_sw_m1(inp, out, N);
#else
#   error "LMUL_MODE must be 1, 2, 4 or 8"
#endif
}

static void check_result(const float *input, const float *x, const float *ref, int r) {
    for (int i = 0; i < r; i++) {
        float diff = fabsf(x[i] - ref[i]);
        printf("At index %d:\t, value %f\t expected %f\t real %f\t error %f\n",
               i, input[i], ref[i], x[i], diff);
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
        snrt_dma_start_1d(g_inp, data_positive, N * sizeof(float));
        snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();

    const int start = (int)((int64_t)cid       * N / (int64_t)cores);
    const int end   = (int)((int64_t)(cid + 1) * N / (int64_t)cores);
    const int len   = end - start;

    snrt_cluster_hw_barrier();

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0) vrec_sw_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[REC_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_result(g_inp, g_out, outRec, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
