/*  sanity_test.c — Quick smoke test for ALL fp32 kernels (SW + HW)
 *
 *  Tests 8 functions × 2 variants = 16 kernels.
 *  Uses LMUL=8, N=16 elements only.
 *  Reports per-kernel PASS/FAIL and a final summary line.
 *
 *  Checks:
 *    1. Output not all-zero  (kernel didn't execute / illegal instruction)
 *    2. No NaN values        (corrupted computation)
 *    3. Max absolute error vs golden reference
 *    4. First 2 outputs for visual spot-check
 */

#include "snrt.h"
#include "printf.h"
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"
#include <math.h>
#include <stdint.h>

#define TEST_N 16

/* ================================================================
 *  HW kernels — simple loop: load → custom instruction → store
 * ================================================================ */
#define DEF_HW_F32(name, instr)                                              \
static inline void name(const float *inp, float *out, int N) {               \
    int rem = N;                                                             \
    while (rem > 0) {                                                        \
        size_t vl;                                                           \
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"                       \
                     : "=r"(vl) : "r"(rem));                                 \
        asm volatile("vle32.v v0, (%0)" :: "r"(inp) : "memory");             \
        asm volatile(instr " v0, v0");                                       \
        asm volatile("vse32.v v0, (%0)" :: "r"(out) : "memory");             \
        inp += vl; out += vl; rem -= vl;                                     \
    }                                                                        \
}

DEF_HW_F32(kern_exp_hw,   "vfexpf.v")
DEF_HW_F32(kern_cos_hw,   "vfcos.v")
DEF_HW_F32(kern_sin_hw,   "vfsin.v")
DEF_HW_F32(kern_cosh_hw,  "vfcoshf.v")
DEF_HW_F32(kern_tanh_hw,  "vftanhf.v")
DEF_HW_F32(kern_log_hw,   "vflog.v")
DEF_HW_F32(kern_rec_hw,   "vfrec7.v")
DEF_HW_F32(kern_rsqrt_hw, "vfrsqrt7.v")

/* ================================================================
 *  SW kernels — m8 only, from individual source files
 * ================================================================ */

/* exp_sw: Schraudolph */
static inline void kern_exp_sw(const float *inp, float *out, int N) {
    const float C = 12102203.0f, B = 1064866805.0f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[p])             \n\t"
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v16, v8           \n\t"
            "vse32.v   v16, (%[o])              \n\t"
            :: [p]"r"(inp), [o]"r"(out), [B]"f"(B), [C]"f"(C)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* cos_sw: Cody-Waite range reduction + polynomial */
static inline void kern_cos_sw(const float *inp, float *out, int N) {
    const float C2 = -0.49670f, S3 = -0.16605f;
    const float INVPIO2 = 0.63661977f, PIO2 = 1.57079632679489661923f;
    unsigned int k_buf[128] __attribute__((aligned(64)));
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v8,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v16, v8                \n\t"
            "vse32.v    v16, (%[kbuf])          \n\t"
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
              [INV]"f"(INVPIO2), [PIO2]"f"(PIO2),
              [S3]"f"(S3), [C2]"f"(C2), [ONE]"f"(1.0f)
            : "memory"
        );
        asm volatile(
            "vle32.v    v0,  (%[kbuf])          \n\t"
            "vand.vi    v24, v0,  1             \n\t"
            "vsrl.vi    v0,  v0,  1             \n\t"
            "vand.vi    v0,  v0,  1             \n\t"
            "vxor.vv    v0,  v0,  v24           \n\t"
            "vadd.vv    v0,  v0,  v0            \n\t"
            "vrsub.vi   v0,  v0,  1             \n\t"
            "vfcvt.f.x.v v0,  v0                \n\t"
            "vfcvt.f.x.v v24, v24               \n\t"
            "vfsub.vv   v16, v16, v8            \n\t"
            "vfmacc.vv  v8,  v16, v24           \n\t"
            "vfmul.vv   v8,  v8,  v0            \n\t"
            "vse32.v    v8,  (%[po])            \n\t"
            :
            : [kbuf]"r"(k_buf), [po]"r"(out)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* sin_sw: Cody-Waite + SIN quadrant blend */
static inline void kern_sin_sw(const float *inp, float *out, int N) {
    const float C2 = -0.49670f, S3 = -0.16605f;
    const float INVPIO2 = 0.63661977f, PIO2 = 1.57079632679489661923f;
    unsigned int k_buf[128] __attribute__((aligned(64)));
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[pin])          \n\t"
            "vfmul.vf   v8,  v0,  %[INV]       \n\t"
            "vfcvt.x.f.v v16, v8                \n\t"
            "vse32.v    v16, (%[kbuf])          \n\t"
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
              [INV]"f"(INVPIO2), [PIO2]"f"(PIO2),
              [S3]"f"(S3), [C2]"f"(C2), [ONE]"f"(1.0f)
            : "memory"
        );
        asm volatile(
            "vle32.v    v0,  (%[kbuf])          \n\t"
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
            "vse32.v    v16, (%[po])            \n\t"
            :
            : [kbuf]"r"(k_buf), [po]"r"(out)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* cosh_sw: Schraudolph exp(x) + exp(-x) / 2 */
static inline void kern_cosh_sw(const float *inp, float *out, int N) {
    const float C = 12102203.0f, B = 1064866805.0f, half = 0.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[p])             \n\t"
            "vfmv.v.f  v8,  %[B]               \n\t"
            "vfmacc.vf v8,  %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v8,  v8           \n\t"
            "vfsgnjn.vv v0, v0, v0              \n\t"
            "vfmv.v.f  v16, %[B]               \n\t"
            "vfmacc.vf v16, %[C], v0            \n\t"
            "vfcvt.rtz.xu.f.v v16, v16          \n\t"
            "vfadd.vv  v24, v8, v16             \n\t"
            "vfmul.vf  v24, v24, %[H]           \n\t"
            "vse32.v   v24, (%[o])              \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [B]"f"(B), [C]"f"(C), [H]"f"(half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* tanh_sw: polynomial approximation */
static inline void kern_tanh_sw(const float *inp, float *out, int N) {
    const float A = 0.02655122f, B = -0.22830456f, C = 0.97858769f;
    const float P1 = 1.0f, N1 = -1.0f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v   v0,  (%[p])             \n\t"
            "vfmul.vv  v8,  v0,  v0            \n\t"
            "vfmv.v.f  v16, %[B]               \n\t"
            "vfmacc.vf v16, %[A], v8            \n\t"
            "vfmv.v.f  v24, %[C]               \n\t"
            "vfmacc.vv v24, v8,  v16            \n\t"
            "vfmul.vv  v24, v0,  v24            \n\t"
            "vfmax.vf  v24, v24, %[N1]          \n\t"
            "vfmin.vf  v24, v24, %[P1]          \n\t"
            "vse32.v   v24, (%[o])              \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [A]"f"(A), [B]"f"(B), [C]"f"(C),
               [P1]"f"(P1), [N1]"f"(N1)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* log_sw: bit-manipulation trick */
static inline void kern_log_sw(const float *inp, float *out, int N) {
    const unsigned int BIAS = 0x3F800000u;
    const float SC = 8.26295829e-8f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v         v0,  (%[p])       \n\t"
            "vsub.vx         v8,  v0,  %[bias] \n\t"
            "vfcvt.f.x.v     v16, v8            \n\t"
            "vfmul.vf        v24, v16, %[sc]    \n\t"
            "vse32.v         v24, (%[o])        \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [bias]"r"(BIAS), [sc]"f"(SC)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* rec_sw: Newton-Raphson reciprocal */
static inline void kern_rec_sw(const float *inp, float *out, int N) {
    const unsigned int M = 0x7EFFFFFFu;
    const float two = 2.0f;
    const uint32_t two_bits = 0x40000000u;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v8,  (%[p])            \n\t"
            "vrsub.vx   v16, v8,  %[M]         \n\t"
            "vmv.v.x    v24, %[tb]             \n\t"
            "vfnmsac.vv v24, v8,  v16           \n\t"
            "vfmul.vv   v16, v16, v24           \n\t"
            "vfmv.v.f   v24, %[two]            \n\t"
            "vfnmsac.vv v24, v8,  v16           \n\t"
            "vfmul.vv   v16, v16, v24           \n\t"
            "vse32.v    v16, (%[o])             \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [M]"r"(M), [tb]"r"(two_bits), [two]"f"(two)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* rsqrt_sw: Quake fast inverse sqrt + 2 NR steps */
static inline void kern_rsqrt_sw(const float *inp, float *out, int N) {
    const unsigned int M = 0x5F3759DFu;
    const float half = 0.5f, three_half = 1.5f;
    int rem = N;
    while (rem > 0) {
        size_t vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(rem));
        asm volatile(
            "vle32.v    v0,  (%[p])            \n\t"
            "vsrl.vi    v8,  v0,  1            \n\t"
            "vrsub.vx   v8,  v8,  %[M]         \n\t"
            "vfmul.vf   v24, v0,  %[H]         \n\t"
            "vfmul.vv   v16, v8,  v8            \n\t"
            "vfmul.vv   v16, v24, v16           \n\t"
            "vfrsub.vf  v16, v16, %[T]          \n\t"
            "vfmul.vv   v8,  v8,  v16           \n\t"
            "vfmul.vv   v16, v8,  v8            \n\t"
            "vfmul.vv   v16, v24, v16           \n\t"
            "vfrsub.vf  v16, v16, %[T]          \n\t"
            "vfmul.vv   v8,  v8,  v16           \n\t"
            "vse32.v    v8,  (%[o])             \n\t"
            :: [p]"r"(inp), [o]"r"(out),
               [M]"r"(M), [H]"f"(half), [T]"f"(three_half)
            : "memory"
        );
        inp += vl; out += vl; rem -= vl;
    }
}

/* ================================================================
 *  Test harness
 * ================================================================ */

typedef void (*kern_f32_t)(const float *, float *, int);

static int sanity(const char *label, kern_f32_t fn,
                  const float *inp, float *out, const float *ref, int N)
{
    /* zero output */
    for (int i = 0; i < N; i++) out[i] = 0.0f;

    /* run kernel */
    fn(inp, out, N);

    /* check */
    int all_zero = 1, nan_cnt = 0;
    float max_err = 0.0f;
    for (int i = 0; i < N; i++) {
        if (out[i] != 0.0f) all_zero = 0;
        if (out[i] != out[i]) nan_cnt++;
        float d = fabsf(out[i] - ref[i]);
        if (d > max_err) max_err = d;
    }
    int pass = !all_zero && !nan_cnt;
    printf("[FP32 %s] %s  max_err=%.4e  out[0]=%.6f exp=%.6f  out[1]=%.6f exp=%.6f\n",
           label, pass ? "PASS" : "FAIL", max_err,
           out[0], ref[0], out[1], ref[1]);
    if (all_zero) printf("[FP32 %s]   -> output entirely zero\n", label);
    if (nan_cnt)  printf("[FP32 %s]   -> %d NaN values\n", label, nan_cnt);
    return pass;
}

static float *g_inp_s, *g_inp_p, *g_out;

int main(void) {
    const unsigned cid = snrt_cluster_core_idx();
    snrt_cluster_hw_barrier();

    if (cid == 0) {
        g_inp_s = (float *)snrt_l1alloc(TEST_N * sizeof(float));
        g_inp_p = (float *)snrt_l1alloc(TEST_N * sizeof(float));
        g_out   = (float *)snrt_l1alloc(TEST_N * sizeof(float));

        snrt_dma_start_1d(g_inp_s, data_signed,  TEST_N * sizeof(float));
        snrt_dma_wait_all();
        snrt_dma_start_1d(g_inp_p, data_positive, TEST_N * sizeof(float));
        snrt_dma_wait_all();

        int total = 0, passed = 0;

        printf("\n========== FP32 SANITY TEST ==========\n");

        /* --- HW kernels --- */
        total++; passed += sanity("EXP_HW",   kern_exp_hw,   g_inp_s, g_out, outE,   TEST_N);
        total++; passed += sanity("COS_HW",   kern_cos_hw,   g_inp_s, g_out, outCos, TEST_N);
        total++; passed += sanity("SIN_HW",   kern_sin_hw,   g_inp_s, g_out, outS,   TEST_N);
        total++; passed += sanity("COSH_HW",  kern_cosh_hw,  g_inp_s, g_out, outC,   TEST_N);
        total++; passed += sanity("TANH_HW",  kern_tanh_hw,  g_inp_s, g_out, outT,   TEST_N);
        total++; passed += sanity("LOG_HW",   kern_log_hw,   g_inp_p, g_out, outL,   TEST_N);
        total++; passed += sanity("REC_HW",   kern_rec_hw,   g_inp_p, g_out, outRec, TEST_N);
        total++; passed += sanity("RSQRT_HW", kern_rsqrt_hw, g_inp_p, g_out, outR,   TEST_N);

        /* --- SW kernels --- */
        total++; passed += sanity("EXP_SW",   kern_exp_sw,   g_inp_s, g_out, outE,   TEST_N);
        total++; passed += sanity("COS_SW",   kern_cos_sw,   g_inp_s, g_out, outCos, TEST_N);
        total++; passed += sanity("SIN_SW",   kern_sin_sw,   g_inp_s, g_out, outS,   TEST_N);
        total++; passed += sanity("COSH_SW",  kern_cosh_sw,  g_inp_s, g_out, outC,   TEST_N);
        total++; passed += sanity("TANH_SW",  kern_tanh_sw,  g_inp_s, g_out, outT,   TEST_N);
        total++; passed += sanity("LOG_SW",   kern_log_sw,   g_inp_p, g_out, outL,   TEST_N);
        total++; passed += sanity("REC_SW",   kern_rec_sw,   g_inp_p, g_out, outRec, TEST_N);
        total++; passed += sanity("RSQRT_SW", kern_rsqrt_sw, g_inp_p, g_out, outR,   TEST_N);

        printf("======= FP32 SUMMARY: %d/%d PASSED =======\n\n", passed, total);
    }

    snrt_cluster_hw_barrier();
    return 0;
}
