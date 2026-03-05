/*  rsqrt_sw.c – Software inverse-sqrt via Quake magic + 2 NR iterations (BF16)
 *
 *  Algorithm:
 *    y = reinterpret_bf16( MAGIC - (reinterpret_int16(a) >> 1) )
 *    half = a * 0.5
 *    2x NR:  t = y*y;  t = half*t;  t = 1.5 - t;  y = y*t
 *
 *  MAGIC = 0x5F37
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

static const unsigned int QUAKE_MAGIC = 0x5F37u;
#define BF16_HALF     0x3F00u  /* 0.5 */
#define BF16_ONE_HALF 0x3FC0u  /* 1.5 */

/* ========================= LMUL = 8 ========================= */
static inline void vrsqrt_sw_m8(const uint16_t *inp, uint16_t *out, int N) {
    float h = bf16_scalar(BF16_HALF);
    float p = bf16_scalar(BF16_ONE_HALF);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v8,  (%[pin])           \n\t"
            "vsrl.vi    v16, v8,  1              \n\t"
            "vrsub.vx   v16, v16, %[M]          \n\t"
            "vfmul.vf   v24, v8,  %[h]          \n\t"
            "vfmul.vv   v8,  v16, v16            \n\t"
            "vfmul.vv   v8,  v24, v8             \n\t"
            "vfmv.v.f   v0,  %[p]               \n\t"
            "vfsub.vv   v8,  v0,  v8             \n\t"
            "vfmul.vv   v16, v16, v8             \n\t"
            : : [pin]"r"(inp),
                [M]"r"(QUAKE_MAGIC), [h]"f"(h), [p]"f"(p)
            : "memory"
        );
        asm volatile(
            "vle16.v    v8,  (%[pin])           \n\t"
            "vfmul.vf   v24, v8,  %[h]          \n\t"
            "vfmul.vv   v8,  v16, v16            \n\t"
            "vfmul.vv   v8,  v24, v8             \n\t"
            "vfmv.v.f   v0,  %[p]               \n\t"
            "vfsub.vv   v8,  v0,  v8             \n\t"
            "vfmul.vv   v16, v16, v8             \n\t"
            "vse16.v    v16, (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [h]"f"(h), [p]"f"(p)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vrsqrt_sw_m4(const uint16_t *inp, uint16_t *out, int N) {
    float h = bf16_scalar(BF16_HALF);
    float p = bf16_scalar(BF16_ONE_HALF);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vsrl.vi    v4,  v0,  1              \n\t"
            "vrsub.vx   v4,  v4,  %[M]          \n\t"
            "vfmul.vf   v12, v0,  %[h]          \n\t"
            "vfmul.vv   v8,  v4,  v4             \n\t"
            "vfmul.vv   v8,  v12, v8             \n\t"
            "vfmv.v.f   v16, %[p]               \n\t"
            "vfsub.vv   v8,  v16, v8             \n\t"
            "vfmul.vv   v4,  v4,  v8             \n\t"
            "vfmul.vv   v8,  v4,  v4             \n\t"
            "vfmul.vv   v8,  v12, v8             \n\t"
            "vfmv.v.f   v16, %[p]               \n\t"
            "vfsub.vv   v8,  v16, v8             \n\t"
            "vfmul.vv   v4,  v4,  v8             \n\t"
            "vse16.v    v4,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(QUAKE_MAGIC), [h]"f"(h), [p]"f"(p)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vrsqrt_sw_m2(const uint16_t *inp, uint16_t *out, int N) {
    float h = bf16_scalar(BF16_HALF);
    float p = bf16_scalar(BF16_ONE_HALF);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vsrl.vi    v2,  v0,  1              \n\t"
            "vrsub.vx   v2,  v2,  %[M]          \n\t"
            "vfmul.vf   v6,  v0,  %[h]          \n\t"
            "vfmul.vv   v4,  v2,  v2             \n\t"
            "vfmul.vv   v4,  v6,  v4             \n\t"
            "vfmv.v.f   v8,  %[p]               \n\t"
            "vfsub.vv   v4,  v8,  v4             \n\t"
            "vfmul.vv   v2,  v2,  v4             \n\t"
            "vfmul.vv   v4,  v2,  v2             \n\t"
            "vfmul.vv   v4,  v6,  v4             \n\t"
            "vfmv.v.f   v8,  %[p]               \n\t"
            "vfsub.vv   v4,  v8,  v4             \n\t"
            "vfmul.vv   v2,  v2,  v4             \n\t"
            "vse16.v    v2,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(QUAKE_MAGIC), [h]"f"(h), [p]"f"(p)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vrsqrt_sw_m1(const uint16_t *inp, uint16_t *out, int N) {
    float h = bf16_scalar(BF16_HALF);
    float p = bf16_scalar(BF16_ONE_HALF);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])           \n\t"
            "vsrl.vi    v1,  v0,  1              \n\t"
            "vrsub.vx   v1,  v1,  %[M]          \n\t"
            "vfmul.vf   v3,  v0,  %[h]          \n\t"
            "vfmul.vv   v2,  v1,  v1             \n\t"
            "vfmul.vv   v2,  v3,  v2             \n\t"
            "vfmv.v.f   v4,  %[p]               \n\t"
            "vfsub.vv   v2,  v4,  v2             \n\t"
            "vfmul.vv   v1,  v1,  v2             \n\t"
            "vfmul.vv   v2,  v1,  v1             \n\t"
            "vfmul.vv   v2,  v3,  v2             \n\t"
            "vfmv.v.f   v4,  %[p]               \n\t"
            "vfsub.vv   v2,  v4,  v2             \n\t"
            "vfmul.vv   v1,  v1,  v2             \n\t"
            "vse16.v    v1,  (%[po])             \n\t"
            : : [pin]"r"(inp), [po]"r"(out),
                [M]"r"(QUAKE_MAGIC), [h]"f"(h), [p]"f"(p)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vrsqrt_sw_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vrsqrt_sw_m8(inp, out, N);
#elif LMUL_MODE == 4
    vrsqrt_sw_m4(inp, out, N);
#elif LMUL_MODE == 2
    vrsqrt_sw_m2(inp, out, N);
#elif LMUL_MODE == 1
    vrsqrt_sw_m1(inp, out, N);
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
    asm volatile("csrwi 0x800, 3" ::: "memory");
    if (len > 0) vrsqrt_sw_kernel(g_inp + start, g_out + start, len);
    asm volatile("csrwi 0x800, 0" ::: "memory");
    snrt_cluster_hw_barrier();
    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[RSQRT_BF16 LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_bf16(g_out, outR, N, "RSQRT_BF16");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
