/*
 * Copyright (C) 2021 ETH Zurich
 * and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * -----------------------------------------------------------------------
 *
 * RVV tanhf (SEW=32) — strip-mined, maskless, runtime-sized VL.
 *
 * Build-time params:
 *   -DLMUL_MODE=8|4|2        (default: 4)
 *   -DTANH_DEG=0|2|3|4       (0 = Schraudolph fast exp; 2/3/4 = Cheby Horner)
 *
 * Identity:
 *   tanh(x) = sign(x) * (1 - 2/(exp(2|x|) + 1))
 *
 * TANH_DEG=0 (Schraudolph fast exp):
 *   E ≈ *(float*)&( (uint32_t) (z*C + B) ),  where z = 2|x|
 *   C = 2^23 / ln(2) ≈ 12102203.0f,  B ≈ 1064866805.0f (tuned bias)
 *
 * TANH_DEG=2/3/4 (Chebyshev on r ∈ [-ln2, ln2]):
 *   z = 2|x|;  y=z*LOG2E;  k=int(y);  r = z - k*LN2
 *   exp(z) ≈ P(r) * 2^k,  P via Horner with FP32-friendly coeffs.
 */

#include <stdint.h>
#include <math.h>
#include <snrt.h>
#include "printf.h"
#include <spatz_cluster_peripheral.h>
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"
#include <stdio.h>
#include <string.h>

#ifndef LMUL_MODE
#define LMUL_MODE 8
#endif

#ifndef TANH_DEG
#define TANH_DEG 0
#endif

#define THRESHOLD 0.00010f

/* Reduction constants */
#define LOG2E  1.4426950408889634f
#define LN2    0.6931471805599453f

/* ---- Degree 0: Schraudolph constants (exp) ---- */
#if (TANH_DEG == 0)
#define SCH_C  12102203.0f     /* 2^23 / ln(2) */
#define SCH_B  1064866805.0f   /* bias (near 127<<23), tuned */
#endif

/* ---- Chebyshev coefficients for exp(r) on r ~ [-ln2, ln2] ---- */
#if   (TANH_DEG == 2)
/* P2(r) = A0 + r*(A1 + r*A2) */
#define A0  0.99877346f
#define A1  1.06127100f
#define A2  0.52032185f
#elif (TANH_DEG == 3)
/* P3(r) = A0 + r*(A1 + r*(A2 + r*A3)) */
#define A0  0.99916065f
#define A1  0.99953520f
#define A2  0.51739150f
#define A3  0.17116465f
#elif (TANH_DEG == 4)
/* P4(r) = A0 + r*(A1 + r*(A2 + r*(A3 + r*A4))) */
#define A0  1.00000000f
#define A1  0.99996230f
#define A2  0.49998870f
#define A3  0.16792161f
#define A4  0.04191753f
#endif

/* ========================= LMUL = 8 =========================
 * v0  : x / |x| / r / P / final tanh
 * v8  : sign(x) in {+1,-1}
 * v16 : k / 2^k bits  (Cheby)  |  den (Schraudolph)
 * v24 : temp          (Cheby)  |  E-bit pattern (Schraudolph)
 */
static inline void vtanh_m8_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        /* Load and get sign/abs */
        asm volatile("vle32.v   v0, (%0)" :: "r"(pin) : "memory");
        asm volatile("vfmv.v.f  v8,  %0" :: "f"(1.0f));
        asm volatile("vfsgnj.vv v8,  v8,  v0");   /* sign(x) -> {+1,-1} */
        asm volatile("vfsgnjx.vv v0,  v0,  v0");  /* |x| */

        /* z = 2*|x| */
        asm volatile("vfmul.vf  v0,  v0,  %[two]" :: [two]"f"(2.0f));

#if (TANH_DEG == 0)
        /* ---- Schraudolph fast exp ---- */
        asm volatile("vfmul.vf  v24, v0,  %[C]" :: [C]"f"(SCH_C));      /* z*C */
        asm volatile("vfadd.vf  v24, v24, %[B]" :: [B]"f"(SCH_B));      /* z*C + B */
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");                      /* (uint) */
        /* Reinterpret as float: use v24 directly in FP ops */
        asm volatile("vfadd.vf  v16, v24, %[one]" :: [one]"f"(1.0f));   /* den = E + 1 */
        asm volatile("vfsub.vf  v24, v24, %[one]" :: [one]"f"(1.0f));   /* num = E - 1 */
        asm volatile("vfdiv.vv  v24, v24, v16");                        /* core = (E-1)/(E+1) */
        asm volatile("vfmul.vv  v0,  v24, v8");                         /* apply sign */
#else
        /* ---- Chebyshev range reduction for exp ---- */
        asm volatile("vfmul.vf        v24, v0,  %[log2e]" :: [log2e]"f"(LOG2E)); /* y */
        asm volatile("vfcvt.x.f.v     v16, v24");                                 /* k (i32) */
        asm volatile("vfcvt.f.x.v     v24, v16");                                 /* kf */
        asm volatile("vfnmsac.vf      v0,  %[ln2], v24" :: [ln2]"f"(LN2));        /* r */

        /* P(r) via Horner */
#  if   (TANH_DEG == 2)
        asm volatile("vfmv.v.f        v24, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmacc.vf       v24, %[kA2], v0" :: [kA2]"f"(A2));
        asm volatile("vfmul.vv        v0,  v0,  v24");
        asm volatile("vfadd.vf        v0,  v0,  %[kA0]" :: [kA0]"f"(A0));
#  elif (TANH_DEG == 3)
        asm volatile("vfmv.v.f        v24, %[kA2]" :: [kA2]"f"(A2));
        asm volatile("vfmacc.vf       v24, %[kA3], v0" :: [kA3]"f"(A3));
        asm volatile("vfmul.vv        v24, v24, v0");
        asm volatile("vfadd.vf        v24, v24, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmul.vv        v0,  v0,  v24");
        asm volatile("vfadd.vf        v0,  v0,  %[kA0]" :: [kA0]"f"(A0));
#  elif (TANH_DEG == 4)
        asm volatile("vfmv.v.f        v24, %[kA3]" :: [kA3]"f"(A3));
        asm volatile("vfmacc.vf       v24, %[kA4], v0" :: [kA4]"f"(A4));
        asm volatile("vfmul.vv        v24, v24, v0");
        asm volatile("vfadd.vf        v24, v24, %[kA2]" :: [kA2]"f"(A2));
        asm volatile("vfmul.vv        v24, v24, v0");
        asm volatile("vfadd.vf        v24, v24, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmul.vv        v0,  v0,  v24");
        asm volatile("vfadd.vf        v0,  v0,  %[kA0]" :: [kA0]"f"(A0));
#  endif

        /* E = P * 2^k (exponent injection) */
        asm volatile("vadd.vx         v16, v16, %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v16, v16, 23");
        asm volatile("vfmul.vv        v0,  v0,  v16");

        /* tanh core */
        asm volatile("vfadd.vf        v24, v0,  %[one]" :: [one]"f"(1.0f));
        asm volatile("vfmv.v.f        v16, %[two]" :: [two]"f"(2.0f));
        asm volatile("vfdiv.vv        v24, v16, v24");
        asm volatile("vfmv.v.f        v16, %[one]" :: [one]"f"(1.0f));
        asm volatile("vfsub.vv        v24, v16, v24");
        asm volatile("vfmul.vv        v0,  v24, v8");
#endif

        /* Store */
        asm volatile("vse32.v v0, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}


// Coefficients for y = x * (C + x^2 * (B + x^2 * A))
#define COEFF_A  0.02655122f
#define COEFF_B -0.22830456f
#define COEFF_C  0.97858769f
#define LIMIT    2.0f

#include <stddef.h>


#include <stddef.h>

// Assuming defines or passed in constants
// #define COEFF_A ...
// #define COEFF_B ...
// #define COEFF_C ...

static inline void vtanh_approx_poly_clamped(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;
    size_t vl;

    const float a = COEFF_A;
    const float b = COEFF_B;
    const float c = COEFF_C;
    const float upper_bound = 1.0f;
    const float lower_bound = -1.0f;

    // Use a single loop with one asm block for safety and efficiency
    for (; remaining > 0; remaining -= (int)vl, pin += vl, pout += vl) {

        // 1. Calculate Vector Length
        asm volatile("vsetvli %0, %1, e32, m2, ta, mu"
                     : "=r"(vl) 
                     : "r"(remaining));


        asm volatile(
            "vle32.v v0, (%[pin])          \n\t" // Load X
            
            // Polynomial: x * (c + x^2 * (b + a * x^2))
            "vfmul.vv v8, v0, v0           \n\t" // v8 = x^2
            
            "vfmv.v.f v16, %[b]            \n\t" // v16 = b
            "vfmacc.vf v16, %[a], v8       \n\t" // v16 = b + (a * x^2)
            
            "vfmv.v.f v24, %[c]            \n\t" // v24 = c
            "vfmacc.vv v24, v16, v8        \n\t" // v24 = c + x^2(b + ax^2)
            "vfmul.vv v24, v24, v0         \n\t" // v24 = x(...) -> Result

            // Clamping 
            "vfmin.vf v24, v24, %[max_val] \n\t" // Clamp upper > 1.0
            "vfmax.vf v24, v24, %[min_val] \n\t" // Clamp lower < -1.0

            "vse32.v v24, (%[pout])        \n\t" // Store result
            
            : // No outputs (memory is updated via pointers)
            : [pin] "r"(pin), 
              [pout] "r"(pout), 
              [a] "f"(a), 
              [b] "f"(b), 
              [c] "f"(c),
              [max_val] "f"(upper_bound),
              [min_val] "f"(lower_bound)
            : "v0", "v8", "v16", "v24", "memory" // Clobber list
        );
    }
}


// Assumes COEFF_A, COEFF_B, COEFF_C are defined
// Assumes N = 2048 (Fixed)


/* ========================= LMUL = 4 ========================= */
static inline void vtanh_m4_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle32.v   v12, (%0)" :: "r"(pin) : "memory");
        asm volatile("vfmv.v.f  v8,  %[one]" :: [one]"f"(1.0f));
        asm volatile("vfsgnj.vv v8,  v8,  v12");
        asm volatile("vfsgnjx.vv v12, v12, v12");
        asm volatile("vfmul.vf  v12, v12, %[two]" :: [two]"f"(2.0f));

#if (TANH_DEG == 0)
        asm volatile("vfmul.vf  v24, v12, %[C]" :: [C]"f"(SCH_C));
        asm volatile("vfadd.vf  v24, v24, %[B]" :: [B]"f"(SCH_B));
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");
        asm volatile("vfadd.vf  v16, v24, %[one]" :: [one]"f"(1.0f));
        asm volatile("vfsub.vf  v24, v24, %[one]" :: [one]"f"(1.0f));
        asm volatile("vfdiv.vv  v24, v24, v16");
        asm volatile("vfmul.vv  v12, v24, v8");
#else
        asm volatile("vfmul.vf        v24, v12, %[log2e]" :: [log2e]"f"(LOG2E));
        asm volatile("vfcvt.x.f.v     v16, v24");
        asm volatile("vfcvt.f.x.v     v24, v16");
        asm volatile("vfnmsac.vf      v12, %[ln2], v24" :: [ln2]"f"(LN2));

#  if   (TANH_DEG == 2)
        asm volatile("vfmv.v.f        v24, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmacc.vf       v24, %[kA2], v12" :: [kA2]"f"(A2));
        asm volatile("vfmul.vv        v12, v12, v24");
        asm volatile("vfadd.vf        v12, v12, %[kA0]" :: [kA0]"f"(A0));
#  elif (TANH_DEG == 3)
        asm volatile("vfmv.v.f        v24, %[kA2]" :: [kA2]"f"(A2));
        asm volatile("vfmacc.vf       v24, %[kA3], v12" :: [kA3]"f"(A3));
        asm volatile("vfmul.vv        v24, v24, v12");
        asm volatile("vfadd.vf        v24, v24, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmul.vv        v12, v12, v24");
        asm volatile("vfadd.vf        v12, v12, %[kA0]" :: [kA0]"f"(A0));
#  elif (TANH_DEG == 4)
        asm volatile("vfmv.v.f        v24, %[kA3]" :: [kA3]"f"(A3));
        asm volatile("vfmacc.vf       v24, %[kA4], v12" :: [kA4]"f"(A4));
        asm volatile("vfmul.vv        v24, v24, v12");
        asm volatile("vfadd.vf        v24, v24, %[kA2]" :: [kA2]"f"(A2));
        asm volatile("vfmul.vv        v24, v24, v12");
        asm volatile("vfadd.vf        v24, v24, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmul.vv        v12, v12, v24");
        asm volatile("vfadd.vf        v12, v12, %[kA0]" :: [kA0]"f"(A0));
#  endif

        asm volatile("vadd.vx         v16, v16, %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v16, v16, 23");
        asm volatile("vfmul.vv        v12, v12, v16");

        asm volatile("vfadd.vf        v24, v12, %[one]" :: [one]"f"(1.0f));
        asm volatile("vfmv.v.f        v16, %[two]" :: [two]"f"(2.0f));
        asm volatile("vfdiv.vv        v24, v16, v24");
        asm volatile("vfmv.v.f        v16, %[one]" :: [one]"f"(1.0f));
        asm volatile("vfsub.vv        v24, v16, v24");
        asm volatile("vfmul.vv        v12, v24, v8");
#endif

        asm volatile("vse32.v v12, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vtanh_m2_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle32.v   v4, (%0)" :: "r"(pin) : "memory");
        asm volatile("vfmv.v.f  v6,  %[one]" :: [one]"f"(1.0f));
        asm volatile("vfsgnj.vv v6,  v6,  v4");
        asm volatile("vfsgnjx.vv v4,  v4,  v4");
        asm volatile("vfmul.vf  v4,  v4,  %[two]" :: [two]"f"(2.0f));

#if (TANH_DEG == 0)
        asm volatile("vfmul.vf  v10, v4,  %[C]" :: [C]"f"(SCH_C));
        asm volatile("vfadd.vf  v10, v10, %[B]" :: [B]"f"(SCH_B));
        asm volatile("vfcvt.rtz.xu.f.v v10, v10");
        asm volatile("vfadd.vf  v8,  v10, %[one]" :: [one]"f"(1.0f));
        asm volatile("vfsub.vf  v10, v10, %[one]" :: [one]"f"(1.0f));
        asm volatile("vfdiv.vv  v10, v10, v8");
        asm volatile("vfmul.vv  v4,  v10, v6");
#else
        asm volatile("vfmul.vf        v10, v4,  %[log2e]" :: [log2e]"f"(LOG2E));
        asm volatile("vfcvt.x.f.v     v8,  v10");
        asm volatile("vfcvt.f.x.v     v10, v8");
        asm volatile("vfnmsac.vf      v4,  %[ln2], v10" :: [ln2]"f"(LN2));

#  if   (TANH_DEG == 2)
        asm volatile("vfmv.v.f        v10, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmacc.vf       v10, %[kA2], v4" :: [kA2]"f"(A2));
        asm volatile("vfmul.vv        v4,  v4,  v10");
        asm volatile("vfadd.vf        v4,  v4,  %[kA0]" :: [kA0]"f"(A0));
#  elif (TANH_DEG == 3)
        asm volatile("vfmv.v.f        v10, %[kA2]" :: [kA2]"f"(A2));
        asm volatile("vfmacc.vf       v10, %[kA3], v4" :: [kA3]"f"(A3));
        asm volatile("vfmul.vv        v10, v10, v4");
        asm volatile("vfadd.vf        v10, v10, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmul.vv        v4,  v4,  v10");
        asm volatile("vfadd.vf        v4,  v4,  %[kA0]" :: [kA0]"f"(A0));
#  elif (TANH_DEG == 4)
        asm volatile("vfmv.v.f        v10, %[kA3]" :: [kA3]"f"(A3));
        asm volatile("vfmacc.vf       v10, %[kA4], v4" :: [kA4]"f"(A4));
        asm volatile("vfmul.vv        v10, v10, v4");
        asm volatile("vfadd.vf        v10, v10, %[kA2]" :: [kA2]"f"(A2));
        asm volatile("vfmul.vv        v10, v10, v4");
        asm volatile("vfadd.vf        v10, v10, %[kA1]" :: [kA1]"f"(A1));
        asm volatile("vfmul.vv        v4,  v4,  v10");
        asm volatile("vfadd.vf        v4,  v4,  %[kA0]" :: [kA0]"f"(A0));
#  endif

        asm volatile("vadd.vx         v8,  v8,  %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v8,  v8,  23");
        asm volatile("vfmul.vv        v4,  v4,  v8");

        asm volatile("vfadd.vf        v10, v4,  %[one]" :: [one]"f"(1.0f));
        asm volatile("vfmv.v.f        v8,  %[two]" :: [two]"f"(2.0f));
        asm volatile("vfdiv.vv        v10, v8,  v10");
        asm volatile("vfmv.v.f        v8,  %[one]" :: [one]"f"(1.0f));
        asm volatile("vfsub.vv        v10, v8,  v10");
        asm volatile("vfmul.vv        v4,  v10, v6");
#endif

        asm volatile("vse32.v v4, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

void vtanh_optimized(const float* inp, float* out,  int N) {
    const float *pin = inp;
    float *pout = out;
    
    // N = 2048
    
    unsigned long vl;
    // Configure VTYPE for max length (16 elements per group)
    asm volatile("vsetvli %0, zero, e32, m1, ta, ma" : "=r"(vl)); 

    // Loop 32 times to process 2048 elements
    for (int i = 0; i < 4; i++) {
        
        // --- STEP 1: LOAD PHASE (Fill the Register File) ---
        asm volatile("vle32.v v0,  (%0)" :: "r"(pin)         : "memory");
        asm volatile("vle32.v v8,  (%0)" :: "r"(pin + vl)    : "memory");
        asm volatile("vle32.v v16, (%0)" :: "r"(pin + vl*2)  : "memory");
        asm volatile("vle32.v v24, (%0)" :: "r"(pin + vl*3)  : "memory");

        // Memory Barrier (
        asm volatile("" ::: "memory"); 

        // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
        // Operands in v0-v31 are fully resident in VRF.
        // There are NO dependencies between these instructions.
        // In the waveform, you will see these issue back-to-back.
        asm volatile("vftanhf.v v0,  v0");
        asm volatile("vftanhf.v v8,  v8");
        asm volatile("vftanhf.v v16,  v16");
        asm volatile("vftanhf.v v24,  v24");

        // --- STEP 3: STORE PHASE (Drain) ---
        asm volatile("vse32.v v0,  (%0)" :: "r"(pout)        : "memory");
        asm volatile("vse32.v v8,  (%0)" :: "r"(pout + vl)   : "memory");
        asm volatile("vse32.v v16, (%0)" :: "r"(pout + vl*2) : "memory");
        asm volatile("vse32.v v24, (%0)" :: "r"(pout + vl*3) : "memory");

        // Move pointers for the next batch of 512
        pin  += (vl * 4);
        pout += (vl * 4);
    }
}
/* ------------------- Kernel selector ------------------- */
static inline void vtanh_strip(const float* inp, float* out, int N) {
#if   (LMUL_MODE == 8)
    vtanh_m8_strip(inp, out, N);
#elif (LMUL_MODE == 4)
    vtanh_m4_strip(inp, out, N);
#elif (LMUL_MODE == 2)
    vtanh_m2_strip(inp, out, N);
#else
#   error "LMUL_MODE must be 8, 4, or 2"
#endif
}

/* ------------------ Simple golden checker ------------------ */
static void check_result(const float *input, const float *x, const float *ref, int r) {
    int err = 0;
    for (int i = 0; i < r; i++) {
        float diff = fabsf(x[i] - ref[i]);
        printf("At index %d:\t, value %f\t expected %f\t real %f\t error %f\n",
                i, input[i], ref[i], x[i], diff);
        
    }
}

/* ----------------------------- Main ----------------------------- */
/* Two-core split with collision-free L1 pointers. */
int main(void) {
    const unsigned int cid = snrt_cluster_core_idx();
    snrt_cluster_hw_barrier();

    const int B = B_Size, C = C_Size, T = T_Size;
    const int N = B * T * C;

    static float *g_in  = NULL;
    static float *g_out = NULL;

    if (cid == 0) {
        g_in  = (float*)snrt_l1alloc(N * sizeof(float));
        g_out = (float*)snrt_l1alloc(N * sizeof(float));
        if (!g_in || !g_out) { printf("alloc failed\n"); return 1; }

        /* Load input from DRAM once, publish to other core */
        snrt_dma_start_1d(g_in, data1_dram, N * sizeof(float));
        snrt_dma_wait_all();
        memset(g_out, 0, N * sizeof(float));
    }
    snrt_cluster_hw_barrier();  /* publish g_in/g_out */

    /* Split work: core 0 → [0..N/2), core 1 → [N/2..N) */
    const int mid   = N >> 1;
    const int start = (cid == 0) ? 0   : mid;
    const int count = (cid == 0) ? mid : (N - mid);

    if (count > 0) {
        start_kernel();
        unsigned t0 = benchmark_get_cycle();

        vtanh_approx_poly_clamped(g_in + start, g_out + start, count);

        unsigned cycles = benchmark_get_cycle() - t0;
        stop_kernel();

        printf("[tanh LMUL=%d DEG=%d] core %u cycles: %u\n",
               LMUL_MODE, TANH_DEG, cid, cycles);
    }

    snrt_cluster_hw_barrier();

    /* Validate on core 0 (assumes golden `outT` in golden/gold.h) */
    if (cid == 0) {
        printf("CHECK RESULTS (tanh)\n");
        check_result(g_in, g_out, outT, N);
    }

    snrt_cluster_hw_barrier();
    return 0;
}
