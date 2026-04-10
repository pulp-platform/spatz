/*  sin_sw.c – Software sine via Cody-Waite range reduction + cheap polynomial (FP16)
 *
 *  Algorithm:
 *    r = x - round(x * 2/pi) * pi/2      (single-constant Cody-Waite)
 *    z = r*r
 *    cos_poly = 1 + C2*z,    sin_poly = r*(1 + S3*z)
 *    Maskless quadrant blend:  sin = sign_s * (s0 + b0*(c0 - s0))
 *    where sign_s = 1 - 2*b1,  b0 = k&1,  b1 = (k>>1)&1
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

#define FP16_C2       0xB7F0u   /* -0.49609375  */
#define FP16_S3       0xB150u   /* -0.16601562  */
#define FP16_INVPIO2  0x3918u   /*  0.63671875  */
#define FP16_PIO2     0x3E48u   /*  1.5703125   */
#define FP16_ONE      0x3C00u   /*  1.0         */

/* ========================= LMUL = 8 ========================= */
static inline void vsin_m8(const uint16_t *inp, uint16_t *out, int N) {
    float INV_val  = bf16_scalar(FP16_INVPIO2);
    float PIO2_val = bf16_scalar(FP16_PIO2);
    float C2_val   = bf16_scalar(FP16_C2);
    float S3_val   = bf16_scalar(FP16_S3);
    float ONE_val  = bf16_scalar(FP16_ONE);
    uint16_t k_buf[256] __attribute__((aligned(64)));
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v8,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v16, v8                \n\t"
            "vse16.v    v16, (%[kbuf])          \n\t"
            "vfcvt.f.x.v v8,  v16               \n\t"
            "vmv.v.v    v24, v0                 \n\t"
            "vfnmsac.vf v24, %[PIO2], v8        \n\t"
            "vfmul.vv   v0,  v24, v24           \n\t"
            "vfmv.v.f   v8,  %[ONE]             \n\t"
            "vfmacc.vf  v8,  %[C2], v0          \n\t"
            "vfmv.v.f   v16, %[ONE]             \n\t"
            "vfmacc.vf  v16, %[S3], v0          \n\t"
            "vfmul.vv   v16, v16, v24           \n\t"
            :
            : [pin]"r"(inp), [kbuf]"r"(k_buf),
              [INV]"f"(INV_val), [PIO2]"f"(PIO2_val),
              [S3]"f"(S3_val), [C2]"f"(C2_val), [ONE]"f"(ONE_val)
            : "memory"
        );
        asm volatile(
            "vle16.v    v0,  (%[kbuf])          \n\t"
            "vand.vi    v24, v0,  1             \n\t"
            "vsrl.vi    v0,  v0,  1             \n\t"
            "vand.vi    v0,  v0,  1             \n\t"
            "vadd.vv    v0,  v0,  v0            \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"
            "vfcvt.f.x.v v0,  v0                \n\t"
            "vfcvt.f.x.v v24, v24               \n\t"
            "vfsub.vv   v8,  v8,  v16           \n\t"
            "vfmacc.vv  v16, v8,  v24           \n\t"
            "vfmul.vv   v16, v16, v0            \n\t"
            "vse16.v    v16, (%[po])            \n\t"
            :
            : [kbuf]"r"(k_buf), [po]"r"(out)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 ========================= */
static inline void vsin_m4(const uint16_t *inp, uint16_t *out, int N) {
    float INV_val  = bf16_scalar(FP16_INVPIO2);
    float PIO2_val = bf16_scalar(FP16_PIO2);
    float C2_val   = bf16_scalar(FP16_C2);
    float S3_val   = bf16_scalar(FP16_S3);
    float ONE_val  = bf16_scalar(FP16_ONE);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v8,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v16, v8                \n\t"
            "vfcvt.f.x.v v8,  v16               \n\t"
            "vmv.v.v    v24, v0                 \n\t"
            "vfnmsac.vf v24, %[PIO2], v8        \n\t"
            "vfmul.vv   v0,  v24, v24           \n\t"
            "vfmv.v.f   v4,  %[ONE]             \n\t"
            "vfmacc.vf  v4,  %[C2], v0          \n\t"
            "vfmv.v.f   v12, %[ONE]             \n\t"
            "vfmacc.vf  v12, %[S3], v0          \n\t"
            "vfmul.vv   v12, v12, v24           \n\t"
            "vand.vi    v8,  v16, 3             \n\t"
            "vand.vi    v20, v8,  1             \n\t"
            "vsrl.vi    v28, v8,  1             \n\t"
            "vand.vi    v28, v28, 1             \n\t"
            "vadd.vv    v0,  v28, v28           \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"
            "vfcvt.f.x.v v0,  v0                \n\t"
            "vfcvt.f.x.v v20, v20               \n\t"
            "vfsub.vv   v8,  v4,  v12           \n\t"
            "vfmacc.vv  v12, v8,  v20           \n\t"
            "vfmul.vv   v12, v12, v0            \n\t"
            "vse16.v    v12, (%[po])            \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [INV]"f"(INV_val), [PIO2]"f"(PIO2_val),
              [S3]"f"(S3_val), [C2]"f"(C2_val), [ONE]"f"(ONE_val)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vsin_m2(const uint16_t *inp, uint16_t *out, int N) {
    float INV_val  = bf16_scalar(FP16_INVPIO2);
    float PIO2_val = bf16_scalar(FP16_PIO2);
    float C2_val   = bf16_scalar(FP16_C2);
    float S3_val   = bf16_scalar(FP16_S3);
    float ONE_val  = bf16_scalar(FP16_ONE);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v4,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v8,  v4                \n\t"
            "vfcvt.f.x.v v4,  v8                \n\t"
            "vmv.v.v    v12, v0                 \n\t"
            "vfnmsac.vf v12, %[PIO2], v4        \n\t"
            "vfmul.vv   v0,  v12, v12           \n\t"
            "vfmv.v.f   v2,  %[ONE]             \n\t"
            "vfmacc.vf  v2,  %[C2], v0          \n\t"
            "vfmv.v.f   v6,  %[ONE]             \n\t"
            "vfmacc.vf  v6,  %[S3], v0          \n\t"
            "vfmul.vv   v6,  v6,  v12           \n\t"
            "vand.vi    v4,  v8,  3             \n\t"
            "vand.vi    v10, v4,  1             \n\t"
            "vsrl.vi    v14, v4,  1             \n\t"
            "vand.vi    v14, v14, 1             \n\t"
            "vadd.vv    v0,  v14, v14           \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"
            "vfcvt.f.x.v v0,  v0                \n\t"
            "vfcvt.f.x.v v10, v10               \n\t"
            "vfsub.vv   v4,  v2,  v6            \n\t"
            "vfmacc.vv  v6,  v4,  v10           \n\t"
            "vfmul.vv   v6,  v6,  v0            \n\t"
            "vse16.v    v6,  (%[po])            \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [INV]"f"(INV_val), [PIO2]"f"(PIO2_val),
              [S3]"f"(S3_val), [C2]"f"(C2_val), [ONE]"f"(ONE_val)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 ========================= */
static inline void vsin_m1(const uint16_t *inp, uint16_t *out, int N) {
    float INV_val  = bf16_scalar(FP16_INVPIO2);
    float PIO2_val = bf16_scalar(FP16_PIO2);
    float C2_val   = bf16_scalar(FP16_C2);
    float S3_val   = bf16_scalar(FP16_S3);
    float ONE_val  = bf16_scalar(FP16_ONE);
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle16.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v2,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v4,  v2                \n\t"
            "vfcvt.f.x.v v2,  v4                \n\t"
            "vmv.v.v    v6,  v0                 \n\t"
            "vfnmsac.vf v6,  %[PIO2], v2        \n\t"
            "vfmul.vv   v0,  v6,  v6            \n\t"
            "vfmv.v.f   v1,  %[ONE]             \n\t"
            "vfmacc.vf  v1,  %[C2], v0          \n\t"
            "vfmv.v.f   v3,  %[ONE]             \n\t"
            "vfmacc.vf  v3,  %[S3], v0          \n\t"
            "vfmul.vv   v3,  v3,  v6            \n\t"
            "vand.vi    v2,  v4,  3             \n\t"
            "vand.vi    v5,  v2,  1             \n\t"
            "vsrl.vi    v7,  v2,  1             \n\t"
            "vand.vi    v7,  v7,  1             \n\t"
            "vadd.vv    v0,  v7,  v7            \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"
            "vfcvt.f.x.v v0,  v0                \n\t"
            "vfcvt.f.x.v v5,  v5                \n\t"
            "vfsub.vv   v2,  v1,  v3            \n\t"
            "vfmacc.vv  v3,  v2,  v5            \n\t"
            "vfmul.vv   v3,  v3,  v0            \n\t"
            "vse16.v    v3,  (%[po])            \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [INV]"f"(INV_val), [PIO2]"f"(PIO2_val),
              [S3]"f"(S3_val), [C2]"f"(C2_val), [ONE]"f"(ONE_val)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vsin_kernel(const uint16_t *inp, uint16_t *out, int N) {
#if   LMUL_MODE == 8
    vsin_m8(inp, out, N);
#elif LMUL_MODE == 4
    vsin_m4(inp, out, N);
#elif LMUL_MODE == 2
    vsin_m2(inp, out, N);
#elif LMUL_MODE == 1
    vsin_m1(inp, out, N);
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
    if (len > 0) vsin_kernel(g_inp + start, g_out + start, len);
    snrt_cluster_hw_barrier();
    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[SIN_FP16 LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_fp16(g_out, outS, N, "SIN_FP16");
    }
    snrt_cluster_hw_barrier();
    return 0;
}
