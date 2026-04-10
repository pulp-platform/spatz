/*  cosh_sw.c – Schraudolph-based fast cosh(x) for BF16 power evaluation
 *
 *  Algorithm:  cosh(x) = 0.5 * (exp(x) + exp(-x))
 *    where exp(x) ≈ reinterpret_bf16( uint16( B + C·x ) )
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

static inline float bf16_scalar(uint32_t bf16_hex) {
    float f;
    uint32_t nanboxed = 0xFFFF0000u | bf16_hex;
    asm volatile("fmv.w.x %0, %1" : "=f"(f) : "r"(nanboxed));
    return f;
}

#define BF16_SCH_B 0x467Eu   /* 16256.0 */
#define BF16_SCH_C 0x4339u   /*   185.0 */
#define BF16_HALF  0x3F00u   /*     0.5 */

/* ========================= LMUL = 8 ========================= */
static inline void vcosh_m8(const uint16_t *inp, uint16_t *out, int N) {
    float B = bf16_scalar(BF16_SCH_B);
    float C = bf16_scalar(BF16_SCH_C);
    float H = bf16_scalar(BF16_HALF);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v8,  v8           \n\t"
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v16, %[B]               \n\t"
            "vfmacc.vf v16, %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v16, v16          \n\t"
            "vfadd.vv  v24, v8, v16             \n\t"
            "vfmul.vf  v24, v24, %[H]           \n\t"
            "vse16.v   v24, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(B), [C]"f"(C), [H]"f"(H)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vcosh_m4(const uint16_t *inp, uint16_t *out, int N) {
    float B = bf16_scalar(BF16_SCH_B);
    float C = bf16_scalar(BF16_SCH_C);
    float H = bf16_scalar(BF16_HALF);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v4,  %[B]               \n\t"
            "vfmacc.vf v4,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v4, v4            \n\t"
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v8, v8            \n\t"
            "vfadd.vv  v12, v4, v8              \n\t"
            "vfmul.vf  v12, v12, %[H]           \n\t"
            "vse16.v   v12, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(B), [C]"f"(C), [H]"f"(H)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vcosh_m2(const uint16_t *inp, uint16_t *out, int N) {
    float B = bf16_scalar(BF16_SCH_B);
    float C = bf16_scalar(BF16_SCH_C);
    float H = bf16_scalar(BF16_HALF);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v2,  %[B]               \n\t"
            "vfmacc.vf v2,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v2, v2            \n\t"
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v4,  %[B]               \n\t"
            "vfmacc.vf v4,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v4, v4            \n\t"
            "vfadd.vv  v6,  v2, v4              \n\t"
            "vfmul.vf  v6,  v6,  %[H]           \n\t"
            "vse16.v   v6,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(B), [C]"f"(C), [H]"f"(H)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vcosh_m1(const uint16_t *inp, uint16_t *out, int N) {
    float B = bf16_scalar(BF16_SCH_B);
    float C = bf16_scalar(BF16_SCH_C);
    float H = bf16_scalar(BF16_HALF);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v   v0,  (%[pin])           \n\t"
            "vfmv.v.f  v1,  %[B]               \n\t"
            "vfmacc.vf v1,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v1, v1            \n\t"
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v2,  %[B]               \n\t"
            "vfmacc.vf v2,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v2, v2            \n\t"
            "vfadd.vv  v3,  v1, v2              \n\t"
            "vfmul.vf  v3,  v3,  %[H]           \n\t"
            "vse16.v   v3,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [B]"f"(B), [C]"f"(C), [H]"f"(H)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vcosh_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vcosh_m8(inp, out, N);
#elif LMUL_MODE == 4
    vcosh_m4(inp, out, N);
#elif LMUL_MODE == 2
    vcosh_m2(inp, out, N);
#elif LMUL_MODE == 1
    vcosh_m1(inp, out, N);
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
    asm volatile("csrwi 0x800, 3" ::: "memory");
    if (len > 0) vcosh_kernel(g_inp + start, g_out + start, len);
    asm volatile("csrwi 0x800, 0" ::: "memory");
    snrt_cluster_hw_barrier();
    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[COSH_BF16 LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_bf16(g_out, outC, N, "COSH_BF16");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
