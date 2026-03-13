/*  log_sw.c – Bit-trick fast log for BF16
 *
 *  Algorithm: log(x) ≈ (as_int(x) - BIAS) * SC
 *  BF16 BIAS = 0x3F80 (127 << 7), SC = 0.0054016113
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

/* BF16 constants for log bit-trick */
#define BF16_BIAS  0x3F80u  /* 16256 as int (127<<7) */
#define BF16_SC    0x3BB1u  /* 0.0054016113 in bf16  */

/* ========================= LMUL = 8 ========================= */
static inline void vlog_m8(const uint16_t *inp, uint16_t *out, int N) {
    float SC_val   = bf16_scalar(BF16_SC);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            /* interpret bits as signed integer, subtract BIAS */
            "li         t0,  0x3F80            \n\t"
            "vsub.vx    v8,  v0,  t0           \n\t"
            /* convert integer to float */
            "vfcvt.f.x.v v16, v8               \n\t"
            /* multiply by SC */
            "vfmul.vf   v24, v16, %[SC]        \n\t"
            "vse16.v    v24, (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [SC]"f"(SC_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vlog_m4(const uint16_t *inp, uint16_t *out, int N) {
    float SC_val   = bf16_scalar(BF16_SC);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "li         t0,  0x3F80            \n\t"
            "vsub.vx    v4,  v0,  t0           \n\t"
            "vfcvt.f.x.v v8,  v4               \n\t"
            "vfmul.vf   v12, v8,  %[SC]        \n\t"
            "vse16.v    v12, (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [SC]"f"(SC_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vlog_m2(const uint16_t *inp, uint16_t *out, int N) {
    float SC_val   = bf16_scalar(BF16_SC);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "li         t0,  0x3F80            \n\t"
            "vsub.vx    v2,  v0,  t0           \n\t"
            "vfcvt.f.x.v v4,  v2               \n\t"
            "vfmul.vf   v6,  v4,  %[SC]        \n\t"
            "vse16.v    v6,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [SC]"f"(SC_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vlog_m1(const uint16_t *inp, uint16_t *out, int N) {
    float SC_val   = bf16_scalar(BF16_SC);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "li         t0,  0x3F80            \n\t"
            "vsub.vx    v1,  v0,  t0           \n\t"
            "vfcvt.f.x.v v2,  v1               \n\t"
            "vfmul.vf   v3,  v2,  %[SC]        \n\t"
            "vse16.v    v3,  (%[po])           \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [SC]"f"(SC_val)
            : "memory", "t0"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vlog_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vlog_m8(inp, out, N);
#elif LMUL_MODE == 4
    vlog_m4(inp, out, N);
#elif LMUL_MODE == 2
    vlog_m2(inp, out, N);
#elif LMUL_MODE == 1
    vlog_m1(inp, out, N);
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

    if (len > 0) vlog_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[LOG_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_bf16(g_out, outL, N, "LOG_SW");
    }

    asm volatile("csrwi 0x800, 0" ::: "memory");
    snrt_cluster_hw_barrier();
    return 0;
}
