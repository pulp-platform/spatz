/*  exp_sw.c – Schraudolph deg-0 fast exp(x) for BF16 power evaluation
 *
 *  Algorithm:  exp(x) ≈ reinterpret_bf16( uint16( B + C·x ) )
 *    where C = 2^7 / ln2 ≈ 185.0 (BF16: 0x4339)
 *          B ≈ 16256.0            (BF16: 0x467E)
 *
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

#define BF16_SCH_B 0x467Eu   /* 16256.0 */
#define BF16_SCH_C 0x4339u   /*   185.0 */

/* ========================= LMUL = 8 ========================= */
static inline void vexp_m8(const uint16_t *inp, uint16_t *out, int N) {
    float B = bf16_scalar(BF16_SCH_B);
    float C = bf16_scalar(BF16_SCH_C);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v           v0,  (%[p])       \n\t"
            "vfmv.v.f          v16, %[B]         \n\t"
            "vfmacc.vf         v16, %[C], v0     \n\t"
            "vfcvt.rtz.xu.f.v  v8,  v16          \n\t"
            "vse16.v           v8,  (%[o])       \n\t"
            :: [p]"r"(inp), [o]"r"(out), [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vexp_m4(const uint16_t *inp, uint16_t *out, int N) {
    float B = bf16_scalar(BF16_SCH_B);
    float C = bf16_scalar(BF16_SCH_C);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v           v0,  (%[p])       \n\t"
            "vfmv.v.f          v8,  %[B]         \n\t"
            "vfmacc.vf         v8,  %[C], v0     \n\t"
            "vfcvt.rtz.xu.f.v  v4,  v8           \n\t"
            "vse16.v           v4,  (%[o])       \n\t"
            :: [p]"r"(inp), [o]"r"(out), [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vexp_m2(const uint16_t *inp, uint16_t *out, int N) {
    float B = bf16_scalar(BF16_SCH_B);
    float C = bf16_scalar(BF16_SCH_C);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v           v0,  (%[p])       \n\t"
            "vfmv.v.f          v4,  %[B]         \n\t"
            "vfmacc.vf         v4,  %[C], v0     \n\t"
            "vfcvt.rtz.xu.f.v  v2,  v4           \n\t"
            "vse16.v           v2,  (%[o])       \n\t"
            :: [p]"r"(inp), [o]"r"(out), [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vexp_m1(const uint16_t *inp, uint16_t *out, int N) {
    float B = bf16_scalar(BF16_SCH_B);
    float C = bf16_scalar(BF16_SCH_C);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v           v0,  (%[p])       \n\t"
            "vfmv.v.f          v2,  %[B]         \n\t"
            "vfmacc.vf         v2,  %[C], v0     \n\t"
            "vfcvt.rtz.xu.f.v  v1,  v2           \n\t"
            "vse16.v           v1,  (%[o])       \n\t"
            :: [p]"r"(inp), [o]"r"(out), [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vexp_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vexp_m8(inp, out, N);
#elif LMUL_MODE == 4
    vexp_m4(inp, out, N);
#elif LMUL_MODE == 2
    vexp_m2(inp, out, N);
#elif LMUL_MODE == 1
    vexp_m1(inp, out, N);
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

    asm volatile("csrwi 0x800, 3" ::: "memory");

    unsigned t0 = 0;
    if (cid == 0) { start_kernel(); t0 = benchmark_get_cycle(); }

    if (len > 0) vexp_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[EXP_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_bf16(g_out, outE, N, "EXP_SW");
    }

    asm volatile("csrwi 0x800, 0" ::: "memory");
    snrt_cluster_hw_barrier();
    return 0;
}
