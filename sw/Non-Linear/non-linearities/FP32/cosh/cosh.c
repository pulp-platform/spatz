/*
 * Copyright (C) 2021 ETH Zurich and University of Bologna
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
 * RVV coshf (SEW=32) — strip-mined, maskless, runtime-selectable LMUL (8/4/2).
 *
 * Build-time params:
 *   -DLMUL_MODE=8|4|2         (default: 8)
 *   -DCOSH_DEG=0|2|3|4        (0 = Schraudolph fast exp; 2/3/4 = Cheby Horner)
 *
 * Identities:
 *   cosh(x) = 0.5 * (exp(x) + exp(-x))
 *
 * COSH_DEG=0 (Schraudolph fast exp):
 *   E(±x) ≈ *(float*)&( (uint32_t) ( (±x)*C + B ) )
 *   C = 2^23 / ln(2) ≈ 12102203.0f,  B ≈ 1064866805.0f (tuned bias)
 *
 * COSH_DEG=2/3/4 (Chebyshev on r ∈ [-ln2/2, +ln2/2], RNE k):
 *   y = x*LOG2E;  k = nearint(y);  r = x - k*LN2
 *   exp(x)   ≈ P_plus(r)  * 2^k
 *   exp(-x)  ≈ P_minus(r) * 2^-k
 *   cosh(x)  = 0.5*(exp(x)+exp(-x))
 */

#include <stdint.h>
#include <math.h>
#include <snrt.h>
#include <inttypes.h>
#include "printf.h"
#include <spatz_cluster_peripheral.h>
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"
#include <stdio.h>
#include <string.h>

#ifndef LMUL_MODE
#define LMUL_MODE 2
#endif

#ifndef COSH_DEG
#define COSH_DEG 0
#endif

#define THRESHOLD 0.00010f

/* Reduction constants */
#define LOG2E  1.4426950408889634f
#define LN2    0.6931471805599453f

/* ---- Degree 0: Schraudolph constants (exp) ---- */
#if (COSH_DEG == 0)
#define SCH_C  12102203.0f     /* 2^23 / ln(2) */
#define SCH_B  1064866805.0f   /* bias (near 127<<23), tuned */
#endif

/* ========================= LMUL = 8 =========================
 * m8 plan:
 *   v24 : x / r / E / Pplus / final (most payload)
 *   v0  : y / rO / 2^{-k} bits (int-as-fp)
 *   v8  : r^2 or k (float view)
 *   v16 : k (i32) then 2^k bits (int-as-fp)
 */
static inline void vcosh_m8_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle32.v   v24, (%0)" :: "r"(pin) : "memory"); /* x -> v24 */

#if (COSH_DEG == 0)
        /* --- Schraudolph: cosh ≈ 0.5*(E(+x)+E(-x)) --- */
        asm volatile("vfmul.vf  v16, v24, %[c]"    :: [c]   "f"(SCH_C));    /* x*C */
        asm volatile("vfadd.vf  v24, v16, %[b]"    :: [b]   "f"(SCH_B));    /* +B + x*c  */
        asm volatile("vfrsub.vf  v16, v16, %[b]"    :: [b]   "f"(SCH_B));    /* +B - x*c   */
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");                          /* bits E(+x) */
        asm volatile("vfcvt.rtz.xu.f.v v16, v16");                          /* bits E(-x) */
        asm volatile("vfadd.vv  v24, v24, v16");
        asm volatile("vfmul.vf  v24, v24, %[half]" :: [half]"f"(0.5f));
#else
        /* --- Chebyshev reduction: r and k --- */
        asm volatile("vfmul.vf        v0,  v24, %[kLOG2E]" :: [kLOG2E]"f"(LOG2E)); /* y */
        asm volatile("vfcvt.x.f.v     v16, v0");                                   /* k (i32, RNE) */
        asm volatile("vfcvt.f.x.v     v8,  v16");                                  /* k (f32) */
        asm volatile("vfnmsac.vf      v24, %[kLN2], v8" :: [kLN2]"f"(LN2));        /* r */

        /* r2 = r*r -> v8 */
        asm volatile("vfmul.vv        v8,  v24, v24");

# if (COSH_DEG == 2)
        /* P_plus = (A0 + A2*r2) + r*A1 ;  P_minus = (A0 + A2*r2) - r*A1 */
        const float A0 = 0.99992448f, A1 = 1.01508951f, A2 = 0.50502354f;
        /* rO = r*A1 -> v0 */
        asm volatile("vmv.v.v         v0,  v24");
        asm volatile("vfmul.vf        v0,  v0,  %[cA1]" :: [cA1]"f"(A1));
        /* E = A0 + A2*r2 -> v24 */
        asm volatile("vfmv.v.f        v24, %[cA0]" :: [cA0]"f"(A0));
        asm volatile("vfmacc.vf       v24, %[cA2], v8" :: [cA2]"f"(A2));
# elif (COSH_DEG == 3)
        /* rO = r * (A1 + r2*A3) ; E = A0 + r2*A2 */
        const float A0 = 0.9999245f, A1 = 0.9999623f, A2 = 0.50502354f, A3 = 0.1679216f;
        /* rO in v0 */
        asm volatile("vfmv.v.f        v0,  %[cA1]" :: [cA1]"f"(A1));
        asm volatile("vfmacc.vf       v0,  %[cA3], v8" :: [cA3]"f"(A3));
        asm volatile("vfmul.vv        v0,  v0,  v24");
        /* E in v24 */
        asm volatile("vfmv.v.f        v24, %[cA0]" :: [cA0]"f"(A0));
        asm volatile("vfmacc.vf       v24, %[cA2], v8" :: [cA2]"f"(A2));
# elif (COSH_DEG == 4)
        /* rO = r*(A1 + r2*A3) ; E = A0 + r2*(A2 + r2*A4) */
        const float A0 = 1.0f, A1 = 0.9999623f, A2 = 0.4999887f, A3 = 0.16792161f, A4 = 0.04191753f;
        /* rO in v0 */
        asm volatile("vfmv.v.f        v0,  %[cA1]" :: [cA1]"f"(A1));
        asm volatile("vfmacc.vf       v0,  %[cA3], v8" :: [cA3]"f"(A3));
        asm volatile("vfmul.vv        v0,  v0,  v24");
        /* E in v24 */
        asm volatile("vfmv.v.f        v24, %[cA2]" :: [cA2]"f"(A2));
        asm volatile("vfmacc.vf       v24, %[cA4], v8" :: [cA4]"f"(A4));
        asm volatile("vfmul.vv        v24, v24, v8");
        asm volatile("vfadd.vf        v24, v24, %[cA0]" :: [cA0]"f"(A0));
# endif

        /* Pplus = E + rO  ; Pminus = E - rO */
        asm volatile("vmv.v.v         v8,  v24");                 /* E copy */
        asm volatile("vfadd.vv        v24, v24, v0");             /* Pplus */
        asm volatile("vfsub.vv        v8,  v8,  v0");             /* Pminus */

        /* 2^k (v16) and 2^{-k} (v0) bit injection */
        asm volatile("vmv.v.v         v0,  v16");                 /* v0 = k (i32) */
        asm volatile("vrsub.vi        v0,  v0,  0");              /* v0 = -k */
        asm volatile("vadd.vx         v16, v16, %[bias]" :: [bias]"r"(126));
        asm volatile("vsll.vi         v16, v16, 23");             /* bits(2^k) */
        asm volatile("vadd.vx         v0,  v0,  %[bias]" :: [bias]"r"(126));
        asm volatile("vsll.vi         v0,  v0,  23");             /* bits(2^-k) */

        /* cosh = 0.5*(2^k*Pplus + 2^-k*Pminus)  -> v24 */
        asm volatile("vfmul.vv        v24, v24, v16");
        asm volatile("vfmul.vv        v8,  v8,  v0");
        asm volatile("vfadd.vv        v24, v24, v8");
        // asm volatile("vfmul.vf        v24, v24, %[half]" :: [half]"f"(0.5f));
#endif

        asm volatile("vse32.v v24, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ========================= LMUL = 4 =========================
 * m4 plan:
 *   v12 : x / r / E / Pplus / final
 *   v24 : temp (rO) or bits(2^-k)
 *   v16 : k (i32) then bits(2^k)
 *   v8  : r^2 or Pminus
 */
static inline void vcosh_m4_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle32.v   v12, (%0)" :: "r"(pin) : "memory"); /* x -> v12 */

#if (COSH_DEG == 0)
        asm volatile("vfmul.vf  v24, v12, %[neg1]" :: [neg1]"f"(-1.0f));
        asm volatile("vfmul.vf  v12, v12, %[c]"    :: [c]   "f"(SCH_C));
        asm volatile("vfadd.vf  v12, v12, %[b]"    :: [b]   "f"(SCH_B));
        asm volatile("vfmul.vf  v24, v24, %[c]"    :: [c]   "f"(SCH_C));
        asm volatile("vfadd.vf  v24, v24, %[b]"    :: [b]   "f"(SCH_B));
        asm volatile("vfcvt.rtz.xu.f.v v12, v12");
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");
        asm volatile("vfadd.vv  v12, v12, v24");
        asm volatile("vfmul.vf  v12, v12, %[half]" :: [half]"f"(0.5f));
#else
        asm volatile("vfmul.vf        v24, v12, %[kLOG2E]" :: [kLOG2E]"f"(LOG2E));
        asm volatile("vfcvt.x.f.v     v16, v24");                      /* k (i32, RNE) */
        asm volatile("vfcvt.f.x.v     v8,  v16");                      /* k (f32) */
        asm volatile("vfnmsac.vf      v12, %[kLN2], v8" :: [kLN2]"f"(LN2)); /* r */

        asm volatile("vfmul.vv        v8,  v12, v12");                 /* r^2 */

# if (COSH_DEG == 2)
        const float A0 = 0.99992448f, A1 = 1.01508951f, A2 = 0.50502354f;
        asm volatile("vmv.v.v         v24, v12");
        asm volatile("vfmul.vf        v24, v24, %[cA1]" :: [cA1]"f"(A1)); /* rO */
        asm volatile("vfmv.v.f        v12, %[cA0]" :: [cA0]"f"(A0));      /* E */
        asm volatile("vfmacc.vf       v12, %[cA2], v8" :: [cA2]"f"(A2));
# elif (COSH_DEG == 3)
        const float A0 = 0.9999245f, A1 = 0.9999623f, A2 = 0.50502354f, A3 = 0.1679216f;
        asm volatile("vfmv.v.f        v24, %[cA1]" :: [cA1]"f"(A1));
        asm volatile("vfmacc.vf       v24, %[cA3], v8" :: [cA3]"f"(A3));
        asm volatile("vfmul.vv        v24, v24, v12");                         /* rO */
        asm volatile("vfmv.v.f        v12, %[cA0]" :: [cA0]"f"(A0));           /* E */
        asm volatile("vfmacc.vf       v12, %[cA2], v8" :: [cA2]"f"(A2));
# elif (COSH_DEG == 4)
        const float A0 = 1.0f, A1 = 0.9999623f, A2 = 0.4999887f, A3 = 0.16792161f, A4 = 0.04191753f;
        asm volatile("vfmv.v.f        v24, %[cA1]" :: [cA1]"f"(A1));
        asm volatile("vfmacc.vf       v24, %[cA3], v8" :: [cA3]"f"(A3));
        asm volatile("vfmul.vv        v24, v24, v12");                         /* rO */
        asm volatile("vfmv.v.f        v12, %[cA2]" :: [cA2]"f"(A2));           /* E: A2 + A4*r2 */
        asm volatile("vfmacc.vf       v12, %[cA4], v8" :: [cA4]"f"(A4));
        asm volatile("vfmul.vv        v12, v8,  v12");                          /* r2*t */
        asm volatile("vfadd.vf        v12, v12, %[cA0]" :: [cA0]"f"(A0));       /* E */
# endif

        /* Pplus / Pminus */
        asm volatile("vmv.v.v         v8,  v12");                 /* E copy */
        asm volatile("vfadd.vv        v12, v12, v24");            /* Pplus */
        asm volatile("vfsub.vv        v8,  v8,  v24");            /* Pminus */

        /* 2^k and 2^{-k} */
        asm volatile("vmv.v.v         v24, v16");                 /* k */
        asm volatile("vrsub.vi        v24, v24, 0");              /* -k */
        asm volatile("vadd.vx         v16, v16, %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v16, v16, 23");
        asm volatile("vadd.vx         v24, v24, %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v24, v24, 23");

        asm volatile("vfmul.vv        v12, v12, v16");            /* Pplus *= 2^k */
        asm volatile("vfmul.vv        v8,  v8,  v24");            /* Pminus*= 2^-k */
        asm volatile("vfadd.vv        v12, v12, v8");
        asm volatile("vfmul.vf        v12, v12, %[half]" :: [half]"f"(0.5f));
#endif

        asm volatile("vse32.v v12, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}
static inline void vcosh_m1_strip_burst_4(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;

        asm volatile("vsetvli %0, zero, e32, m1, ta, ma" : "=r"(vl) :: "memory");


        /* load 4 vectors */
        asm volatile("vle32.v   v0, (%0)" :: "r"(pin)           : "memory");
        asm volatile("vle32.v   v1, (%0)" :: "r"(pin + vl)      : "memory");
        asm volatile("vle32.v   v2, (%0)" :: "r"(pin + 2*vl)    : "memory");
        asm volatile("vle32.v   v3, (%0)" :: "r"(pin + 3*vl)    : "memory");

        /* --- group 0: v0 -> v0 --- */
        asm volatile("vfmul.vf           v4, v0, %[c]"     :: [c]"f"(SCH_C));
        asm volatile("vfadd.vf           v0, v4, %[b]"     :: [b]"f"(SCH_B));   /* B + xC */
        asm volatile("vfrsub.vf          v4, v4, %[b]"     :: [b]"f"(SCH_B));   /* B - xC */
        asm volatile("vfcvt.rtz.xu.f.v   v0, v0");
        asm volatile("vfcvt.rtz.xu.f.v   v4, v4");
        asm volatile("vfadd.vv           v0, v0, v4");
        asm volatile("vfmul.vf           v0, v0, %[half]"  :: [half]"f"(0.5f));

        /* --- group 1: v1 -> v1 --- */
        asm volatile("vfmul.vf           v5, v1, %[c]"     :: [c]"f"(SCH_C));
        asm volatile("vfadd.vf           v1, v5, %[b]"     :: [b]"f"(SCH_B));
        asm volatile("vfrsub.vf          v5, v5, %[b]"     :: [b]"f"(SCH_B));
        asm volatile("vfcvt.rtz.xu.f.v   v1, v1");
        asm volatile("vfcvt.rtz.xu.f.v   v5, v5");
        asm volatile("vfadd.vv           v1, v1, v5");
        asm volatile("vfmul.vf           v1, v1, %[half]"  :: [half]"f"(0.5f));

        /* --- group 2: v2 -> v2 --- */
        asm volatile("vfmul.vf           v6, v2, %[c]"     :: [c]"f"(SCH_C));
        asm volatile("vfadd.vf           v2, v6, %[b]"     :: [b]"f"(SCH_B));
        asm volatile("vfrsub.vf          v6, v6, %[b]"     :: [b]"f"(SCH_B));
        asm volatile("vfcvt.rtz.xu.f.v   v2, v2");
        asm volatile("vfcvt.rtz.xu.f.v   v6, v6");
        asm volatile("vfadd.vv           v2, v2, v6");
        asm volatile("vfmul.vf           v2, v2, %[half]"  :: [half]"f"(0.5f));

        /* --- group 3: v3 -> v3 --- */
        asm volatile("vfmul.vf           v7, v3, %[c]"     :: [c]"f"(SCH_C));
        asm volatile("vfadd.vf           v3, v7, %[b]"     :: [b]"f"(SCH_B));
        asm volatile("vfrsub.vf          v7, v7, %[b]"     :: [b]"f"(SCH_B));
        asm volatile("vfcvt.rtz.xu.f.v   v3, v3");
        asm volatile("vfcvt.rtz.xu.f.v   v7, v7");
        asm volatile("vfadd.vv           v3, v3, v7");
        asm volatile("vfmul.vf           v3, v3, %[half]"  :: [half]"f"(0.5f));

        /* store 4 vectors */
        asm volatile("vse32.v v0, (%0)" :: "r"(pout)        : "memory");
        asm volatile("vse32.v v1, (%0)" :: "r"(pout + vl)   : "memory");
        asm volatile("vse32.v v2, (%0)" :: "r"(pout + 2*vl) : "memory");
        asm volatile("vse32.v v3, (%0)" :: "r"(pout + 3*vl) : "memory");

        pin       += 4 * (int)vl;
        pout      += 4 * (int)vl;
        remaining -= (int)(4 * vl);
    }
}

/* ========================= LMUL = 2 =========================
 * m2 plan:
 *   v4  : x / r / E / Pplus / final
 *   v10 : temp (rO) or bits(2^-k)
 *   v8  : k (i32) then bits(2^k)
 *   v6  : r^2 or Pminus
 */
static inline void vcosh_m2_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle32.v   v4, (%0)" :: "r"(pin) : "memory"); /* x -> v4 */

#if (COSH_DEG == 0)
        asm volatile("vfmul.vf  v10, v4, %[neg1]" :: [neg1]"f"(-1.0f));
        asm volatile("vfmul.vf  v4,  v4, %[c]"    :: [c]   "f"(SCH_C));
        asm volatile("vfadd.vf  v4,  v4, %[b]"    :: [b]   "f"(SCH_B));
        asm volatile("vfmul.vf  v10, v10, %[c]"   :: [c]   "f"(SCH_C));
        asm volatile("vfadd.vf  v10, v10, %[b]"   :: [b]   "f"(SCH_B));
        asm volatile("vfcvt.rtz.xu.f.v v4,  v4");
        asm volatile("vfcvt.rtz.xu.f.v v10, v10");
        asm volatile("vfadd.vv  v4,  v4,  v10");
        asm volatile("vfmul.vf  v4,  v4,  %[half]" :: [half]"f"(0.5f));
#else
        asm volatile("vfmul.vf        v10, v4,  %[kLOG2E]" :: [kLOG2E]"f"(LOG2E));
        asm volatile("vfcvt.x.f.v     v8,  v10");                     /* k (i32, RNE) */
        asm volatile("vfcvt.f.x.v     v6,  v8");                      /* k (f32) */
        asm volatile("vfnmsac.vf      v4,  %[kLN2], v6" :: [kLN2]"f"(LN2)); /* r */

        asm volatile("vfmul.vv        v6,  v4,  v4");                 /* r^2 */

# if (COSH_DEG == 2)
        const float A0 = 0.99992448f, A1 = 1.01508951f, A2 = 0.50502354f;
        asm volatile("vmv.v.v         v10, v4");
        asm volatile("vfmul.vf        v10, v10, %[cA1]" :: [cA1]"f"(A1)); /* rO */
        asm volatile("vfmv.v.f        v4,  %[cA0]" :: [cA0]"f"(A0));      /* E */
        asm volatile("vfmacc.vf       v4,  %[cA2], v6" :: [cA2]"f"(A2));
# elif (COSH_DEG == 3)
        const float A0 = 0.9999245f, A1 = 0.9999623f, A2 = 0.50502354f, A3 = 0.1679216f;
        asm volatile("vfmv.v.f        v10, %[cA1]" :: [cA1]"f"(A1));
        asm volatile("vfmacc.vf       v10, %[cA3], v6" :: [cA3]"f"(A3));
        asm volatile("vfmul.vv        v10, v10, v4");                          /* rO */
        asm volatile("vfmv.v.f        v4,  %[cA0]" :: [cA0]"f"(A0));           /* E */
        asm volatile("vfmacc.vf       v4,  %[cA2], v6" :: [cA2]"f"(A2));
# elif (COSH_DEG == 4)
        const float A0 = 1.0f, A1 = 0.9999623f, A2 = 0.4999887f, A3 = 0.16792161f, A4 = 0.04191753f;
        asm volatile("vfmv.v.f        v10, %[cA1]" :: [cA1]"f"(A1));
        asm volatile("vfmacc.vf       v10, %[cA3], v6" :: [cA3]"f"(A3));
        asm volatile("vfmul.vv        v10, v10, v4");                          /* rO */
        asm volatile("vfmv.v.f        v4,  %[cA2]" :: [cA2]"f"(A2));           /* E: A2 + A4*r2 */
        asm volatile("vfmacc.vf       v4,  %[cA4], v6" :: [cA4]"f"(A4));
        asm volatile("vfmul.vv        v4,  v6,  v4");                           /* r2*t */
        asm volatile("vfadd.vf        v4,  v4,  %[cA0]" :: [cA0]"f"(A0));       /* E */
# endif

        /* Pplus / Pminus */
        asm volatile("vmv.v.v         v6,  v4");                    /* E copy */
        asm volatile("vfadd.vv        v4,  v4,  v10");              /* Pplus */
        asm volatile("vfsub.vv        v6,  v6,  v10");              /* Pminus */

        /* 2^k and 2^{-k} */
        asm volatile("vmv.v.v         v10, v8");                    /* k */
        asm volatile("vrsub.vi        v10, v10, 0");                /* -k */
        asm volatile("vadd.vx         v8,  v8,  %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v8,  v8,  23");               /* bits(2^k) */
        asm volatile("vadd.vx         v10, v10, %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v10, v10, 23");               /* bits(2^-k) */

        asm volatile("vfmul.vv        v4,  v4,  v8");
        asm volatile("vfmul.vv        v6,  v6,  v10");
        asm volatile("vfadd.vv        v4,  v4,  v6");
        asm volatile("vfmul.vf        v4,  v4,  %[half]" :: [half]"f"(0.5f));
#endif

        asm volatile("vse32.v v4, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}
void vcosh_optimized_v8(const float* inp, float* out,  int N) {
    const float *pin = inp;
    float *pout = out;
    
    // N = 2048
    
    unsigned long vl;
    // Configure VTYPE for max length (128 elements per group)
    asm volatile("vsetvli %0, zero, e32, m1, ta, ma" : "=r"(vl)); 

    // Loop 4 times to process 2048 elements
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
        asm volatile("vfcoshf.v v0,  v0");
        asm volatile("vfcoshf.v v8,  v8");
        asm volatile("vfcoshf.v v16,  v16");
        asm volatile("vfcoshf.v v24,  v24");

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
void vcosh_optimized(const float* inp, float* out,  int N) {
    const float *pin = inp;
    float *pout = out;
    
    // N = 2048
    
    unsigned long vl;
    // Configure VTYPE for max length (128 elements per group)
    asm volatile("vsetvli %0, zero, e32, m1, ta, ma" : "=r"(vl)); 

    // Loop 4 times to process 2048 elements
    for (int i = 0; i < 32; i++) {
        
        // --- STEP 1: LOAD PHASE (Fill the Register File) ---
        asm volatile("vle32.v v0,  (%0)" :: "r"(pin)         : "memory");
        asm volatile("vle32.v v1,  (%0)" :: "r"(pin + vl)    : "memory");
        asm volatile("vle32.v v2, (%0)" :: "r"(pin + vl*2)  : "memory");
        asm volatile("vle32.v v3, (%0)" :: "r"(pin + vl*3)  : "memory");

        // Memory Barrier (
        asm volatile("" ::: "memory"); 

        // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
        // Operands in v0-v31 are fully resident in VRF.
        // There are NO dependencies between these instructions.
        // In the waveform, you will see these issue back-to-back.
        asm volatile("vfcoshf.v v4,  v0");
        asm volatile("vfcoshf.v v5,  v1");
        asm volatile("vfcoshf.v v6,  v2");
        asm volatile("vfcoshf.v v7,  v3");

        // --- STEP 3: STORE PHASE (Drain) ---
        asm volatile("vse32.v v4,  (%0)" :: "r"(pout)        : "memory");
        asm volatile("vse32.v v5,  (%0)" :: "r"(pout + vl)   : "memory");
        asm volatile("vse32.v v6, (%0)" :: "r"(pout + vl*2) : "memory");
        asm volatile("vse32.v v7, (%0)" :: "r"(pout + vl*3) : "memory");

        // Move pointers for the next batch of 512
        pin  += (vl * 4);
        pout += (vl * 4);
    }
}

// LMUL=4 version of your vfcoshf “clean burst” microbenchmark.
// Key LMUL rule: with m4, each vector operand is a 4-register group, so base regs
// must be multiples of 4. Use v0,v4,v8,v12 for inputs and v16,v20,v24,v28 for outputs.

static inline void vcosh_optimized_m4(const float* inp, float* out, int N) {
  const float *pin  = inp;
  float       *pout = out;

  unsigned long vl;
  // Configure VTYPE for max length with LMUL=4
  asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(vl) :: "memory");

  // 4 vector-groups per iter => 4*vl elements per iter
  const int iters = N / (int)(4ul * vl);

  for (int i = 0; i < iters; i++) {
    // --- STEP 1: LOAD PHASE ---
    asm volatile("vle32.v v0,  (%0)" :: "r"(pin)            : "memory");
    asm volatile("vle32.v v8,  (%0)" :: "r"(pin +      vl)  : "memory");
    asm volatile("vle32.v v16,  (%0)" :: "r"(pin + 2ul*vl)   : "memory");
    asm volatile("vle32.v v24, (%0)" :: "r"(pin + 3ul*vl)   : "memory");

    asm volatile("" ::: "memory");

    // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
    asm volatile("vfcoshf.v v0, v0");
    asm volatile("vfcoshf.v v8, v8");
    asm volatile("vfcoshf.v v16, v16");
    asm volatile("vfcoshf.v v24, v24");

    // --- STEP 3: STORE PHASE ---
    asm volatile("vse32.v v0, (%0)" :: "r"(pout)            : "memory");
    asm volatile("vse32.v v8, (%0)" :: "r"(pout +      vl)  : "memory");
    asm volatile("vse32.v v16, (%0)" :: "r"(pout + 2ul*vl)   : "memory");
    asm volatile("vse32.v v24, (%0)" :: "r"(pout + 3ul*vl)   : "memory");

    pin  += 4ul * vl;
    pout += 4ul * vl;
  }
}

/* ------------------- Kernel selector ------------------- */
static inline void vcosh_strip(const float* inp, float* out, int N) {
#if   (LMUL_MODE == 8)
    vcosh_m8_strip(inp, out, N);
#elif (LMUL_MODE == 4)
    vcosh_m4_strip(inp, out, N);
#elif (LMUL_MODE == 2)
    vcosh_m2_strip(inp, out, N);
#else
#   error "LMUL_MODE must be 8, 4, or 2"
#endif
}

static inline uint32_t f32_bits(float v) {
        uint32_t u;
        memcpy(&u, &v, sizeof u);
        return u;
}

static void check_result(const float *input, const float *x, const float *ref, int r) {
    int err = 0;
    for (int i = 0; i < r; i++) {
        float diff = fabsf(x[i] - ref[i]);
        printf("At index %d:\t, value %f\t expected %f\t real %f\t error %f\n",
                i, input[i], ref[i], x[i], diff);
        
    }
}
/* ----------------------------- Main ----------------------------- */
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

    /* Two-core split */
    const int mid   = N >> 1;
    const int start = (cid == 0) ? 0   : mid;
    const int count = (cid == 0) ? mid : (N - mid);

    if (count > 0) {
        start_kernel();
        unsigned t0 = benchmark_get_cycle();

        vcosh_optimized_m4(g_in + start, g_out + start, count);

        unsigned cycles = benchmark_get_cycle() - t0;
        stop_kernel();

        printf("[cosh LMUL=%d DEG=%d] core %u cycles: %u\n",
               LMUL_MODE, COSH_DEG, cid, cycles);
    }

    snrt_cluster_hw_barrier();

    /* Validate on core 0 (expects golden `outH` in golden/gold.h) */
    if (cid == 0) {
        printf("CHECK RESULTS (cosh)\n");
        check_result(g_in,g_out, outC, N);
    }

    snrt_cluster_hw_barrier();
    return 0;
}
