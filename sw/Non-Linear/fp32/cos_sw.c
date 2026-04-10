/*  cos_sw.c – Software cosine via Cody-Waite range reduction + cheap polynomial
 *
 *  Algorithm:
 *    r = x - round(x * 2/pi) * pi/2      (single-constant Cody-Waite)
 *    z = r*r
 *    cos_poly = 1 + C2*z,    sin_poly = r*(1 + S3*z)     (2-term cheap)
 *    Maskless quadrant blend:  cos = sign_c * (c0 + b0*(s0 - c0))
 *    where sign_c = 1 - 2*(b0 ^ b1),  b0 = k&1,  b1 = (k>>1)&1
 *
 *  Compile with -DLMUL_MODE={1,2,4,8}
 */

#include "snrt.h"
#include "printf.h"
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"

#include <math.h>

#ifndef LMUL_MODE
#define LMUL_MODE 4
#endif

static const float C2      = -0.49670f;
static const float S3      = -0.16605f;
static const float INVPIO2 = 0.63661977f;
static const float PIO2    = 1.57079632679489661923f;

/* ========================= LMUL = 8 =========================
 * 4 register groups (v0, v8, v16, v24).
 * Memory spill for integer k.
 *
 * Phase 1: range reduce, compute c0(v8) and s0(v16).
 * Phase 2: reload k, quadrant decode, output cos.
 */
static inline void vcos_m8(const float *inp, float *out, int N) {
    unsigned int k_buf[128] __attribute__((aligned(64)));
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(rem));
        /* Phase 1: range reduction + polynomials */
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v8,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v16, v8                \n\t"
            "vse32.v    v16, (%[kbuf])          \n\t"
            "vfcvt.f.x.v v8,  v16               \n\t"
            "vmv.v.v    v24, v0                 \n\t"
            "vfnmsac.vf v24, %[PIO2], v8        \n\t"
            /* z = r*r -> v0 */
            "vfmul.vv   v0,  v24, v24           \n\t"
            /* c0 = 1 + C2*z -> v8 */
            "vfmv.v.f   v8,  %[ONE]             \n\t"
            "vfmacc.vf  v8,  %[C2], v0          \n\t"
            /* s0 = r*(1 + S3*z) -> v16 */
            "vfmv.v.f   v16, %[ONE]             \n\t"
            "vfmacc.vf  v16, %[S3], v0          \n\t"
            "vfmul.vv   v16, v16, v24           \n\t"
            :
            : [pin]"r"(inp), [kbuf]"r"(k_buf),
              [INV]"f"(INVPIO2), [PIO2]"f"(PIO2),
              [S3]"f"(S3), [C2]"f"(C2), [ONE]"f"(1.0f)
            : "memory"
        );
        /* Phase 2: quadrant decode for COS
         * v8=c0, v16=s0.  sign_c = 1-2*(b0^b1),  cos = c0 + b0*(s0-c0) */
        asm volatile(
            "vle32.v    v0,  (%[kbuf])          \n\t"
            "vand.vi    v24, v0,  1             \n\t"   /* b0 */
            "vsrl.vi    v0,  v0,  1             \n\t"
            "vand.vi    v0,  v0,  1             \n\t"   /* b1 */
            "vxor.vv    v0,  v0,  v24           \n\t"   /* b0^b1 */
            "vadd.vv    v0,  v0,  v0            \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"   /* sign_c (int) */
            "vfcvt.f.x.v v0,  v0                \n\t"   /* sign_c (float) */
            "vfcvt.f.x.v v24, v24               \n\t"   /* b0 (float) */
            "vfsub.vv   v16, v16, v8            \n\t"   /* s0 - c0 */
            "vfmacc.vv  v8,  v16, v24           \n\t"   /* c0 + b0*(s0-c0) */
            "vfmul.vv   v8,  v8,  v0            \n\t"   /* apply sign */
            "vse32.v    v8,  (%[po])            \n\t"
            :
            : [kbuf]"r"(k_buf), [po]"r"(out)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 4 =========================
 * 8 register groups: v0,v4,v8,v12,v16,v20,v24,v28
 * v0=x/z/sign, v4=c0, v8=kf/q/diff, v12=s0, v16=k, v20=b0, v24=r, v28=b1
 */
static inline void vcos_m4(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            /* load x -> v0 */
            "vle32.v    v0,  (%[pin])          \n\t"
            /* k = round(x * INVPIO2) */
            "vfmul.vf   v8,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v16, v8                \n\t"
            "vfcvt.f.x.v v8,  v16               \n\t"
            /* r = x - kf*PIO2 */
            "vmv.v.v    v24, v0                 \n\t"
            "vfnmsac.vf v24, %[PIO2], v8        \n\t"
            /* z = r*r */
            "vfmul.vv   v0,  v24, v24           \n\t"
            /* c0 = 1 + C2*z */
            "vfmv.v.f   v4,  %[ONE]             \n\t"
            "vfmacc.vf  v4,  %[C2], v0          \n\t"
            /* s0 = r * (1 + S3*z) */
            "vfmv.v.f   v12, %[ONE]             \n\t"
            "vfmacc.vf  v12, %[S3], v0          \n\t"
            "vfmul.vv   v12, v12, v24           \n\t"
            /* quadrant selection */
            "vand.vi    v8,  v16, 3             \n\t"   /* q */
            "vand.vi    v20, v8,  1             \n\t"   /* b0 */
            "vsrl.vi    v28, v8,  1             \n\t"
            "vand.vi    v28, v28, 1             \n\t"   /* b1 */
            /* sign_c = 1 - 2*(b0^b1) */
            "vxor.vv    v0,  v20, v28           \n\t"
            "vadd.vv    v0,  v0,  v0            \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"
            "vfcvt.f.x.v v0,  v0                \n\t"
            /* convert b0 to float */
            "vfcvt.f.x.v v20, v20               \n\t"
            /* cos = c0 + b0*(s0 - c0) */
            "vfsub.vv   v8,  v12, v4            \n\t"
            "vfmacc.vv  v4,  v8,  v20           \n\t"
            /* apply sign */
            "vfmul.vv   v4,  v4,  v0            \n\t"
            "vse32.v    v4,  (%[po])            \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [INV]"f"(INVPIO2), [PIO2]"f"(PIO2),
              [S3]"f"(S3), [C2]"f"(C2), [ONE]"f"(1.0f)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 2 =========================
 * v0=x/z/sign, v2=c0, v4=kf/q/diff, v6=s0, v8=k, v10=b0, v12=r, v14=b1
 */
static inline void vcos_m2(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v4,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v8,  v4                \n\t"
            "vfcvt.f.x.v v4,  v8                \n\t"
            "vmv.v.v    v12, v0                 \n\t"
            "vfnmsac.vf v12, %[PIO2], v4        \n\t"
            "vfmul.vv   v0,  v12, v12           \n\t"
            /* c0 */
            "vfmv.v.f   v2,  %[ONE]             \n\t"
            "vfmacc.vf  v2,  %[C2], v0          \n\t"
            /* s0 */
            "vfmv.v.f   v6,  %[ONE]             \n\t"
            "vfmacc.vf  v6,  %[S3], v0          \n\t"
            "vfmul.vv   v6,  v6,  v12           \n\t"
            /* quadrant */
            "vand.vi    v4,  v8,  3             \n\t"
            "vand.vi    v10, v4,  1             \n\t"
            "vsrl.vi    v14, v4,  1             \n\t"
            "vand.vi    v14, v14, 1             \n\t"
            /* sign_c = 1 - 2*(b0^b1) */
            "vxor.vv    v0,  v10, v14           \n\t"
            "vadd.vv    v0,  v0,  v0            \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"
            "vfcvt.f.x.v v0,  v0                \n\t"
            "vfcvt.f.x.v v10, v10               \n\t"
            /* cos = c0 + b0*(s0 - c0) */
            "vfsub.vv   v4,  v6,  v2            \n\t"
            "vfmacc.vv  v2,  v4,  v10           \n\t"
            "vfmul.vv   v2,  v2,  v0            \n\t"
            "vse32.v    v2,  (%[po])            \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [INV]"f"(INVPIO2), [PIO2]"f"(PIO2),
              [S3]"f"(S3), [C2]"f"(C2), [ONE]"f"(1.0f)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ========================= LMUL = 1 =========================
 * v0=x/z/sign, v1=c0, v2=kf/q/diff, v3=s0, v4=k, v5=b0, v6=r, v7=b1
 */
static inline void vcos_m1(const float *inp, float *out, int N) {
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v2,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v4,  v2                \n\t"
            "vfcvt.f.x.v v2,  v4                \n\t"
            "vmv.v.v    v6,  v0                 \n\t"
            "vfnmsac.vf v6,  %[PIO2], v2        \n\t"
            "vfmul.vv   v0,  v6,  v6            \n\t"
            /* c0 */
            "vfmv.v.f   v1,  %[ONE]             \n\t"
            "vfmacc.vf  v1,  %[C2], v0          \n\t"
            /* s0 */
            "vfmv.v.f   v3,  %[ONE]             \n\t"
            "vfmacc.vf  v3,  %[S3], v0          \n\t"
            "vfmul.vv   v3,  v3,  v6            \n\t"
            /* quadrant */
            "vand.vi    v2,  v4,  3             \n\t"
            "vand.vi    v5,  v2,  1             \n\t"
            "vsrl.vi    v7,  v2,  1             \n\t"
            "vand.vi    v7,  v7,  1             \n\t"
            /* sign_c = 1 - 2*(b0^b1) */
            "vxor.vv    v0,  v5,  v7            \n\t"
            "vadd.vv    v0,  v0,  v0            \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"
            "vfcvt.f.x.v v0,  v0                \n\t"
            "vfcvt.f.x.v v5,  v5                \n\t"
            /* cos = c0 + b0*(s0 - c0) */
            "vfsub.vv   v2,  v3,  v1            \n\t"
            "vfmacc.vv  v1,  v2,  v5            \n\t"
            "vfmul.vv   v1,  v1,  v0            \n\t"
            "vse32.v    v1,  (%[po])            \n\t"
            :
            : [pin]"r"(inp), [po]"r"(out),
              [INV]"f"(INVPIO2), [PIO2]"f"(PIO2),
              [S3]"f"(S3), [C2]"f"(C2), [ONE]"f"(1.0f)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

static inline void vcos_kernel(const float *inp, float *out, int N) {
#if   LMUL_MODE == 8
    vcos_m8(inp, out, N);
#elif LMUL_MODE == 4
    vcos_m4(inp, out, N);
#elif LMUL_MODE == 2
    vcos_m2(inp, out, N);
#elif LMUL_MODE == 1
    vcos_m1(inp, out, N);
#else
#   error "LMUL_MODE must be 1, 2, 4 or 8"
#endif
}

static void check_result(const float *input, const float *x, const float *ref, int r) {
    int err = 0;
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

    if (len > 0) vcos_kernel(g_inp + start, g_out + start, len);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cyc = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[COS_SW LMUL=%d] cycles=%u  cores=%u  N=%d\n",
               LMUL_MODE, cyc, cores, N);
        check_result(g_inp, g_out, outCos, N);
    }
    snrt_cluster_hw_barrier();
    return 0;
}
