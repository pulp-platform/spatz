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
 * RVV expf (SEW=32) — strip-mined, maskless, runtime-selectable LMUL (8/4/2).
 *
 * Build-time params:
 *   -DLMUL_MODE=8|4|2     (default: 8)
 *   -DEXP_DEG=0|2|3|4     (0 = Schraudolph fast exp; 2/3/4 = Chebyshev + RNE reduction)
 *
 * Identities / approach:
 *   For EXP_DEG = 0 (Schraudolph):
 *     E(x) ≈ *(float*)&( (uint32_t)( B + C*x ) )   // fused here as vfmacc
 *     with C = 2^23 / ln(2) ≈ 12102203.0f,  B ≈ 1064866805.0f
 *
 *   For EXP_DEG = 2/3/4 (RNE reduction + Chebyshev in r):
 *     y = x*LOG2E ; k = nearint(y) (RNE); r = x - k*LN2
 *     exp(x) ≈ P(r) * 2^k
 *     Degree-2:  P(r) = A0 + r*(A1 + r*A2)                      (2 FMAs)
 *     Degree-3:  P(r) = A0 + r*(A1 + r*(A2 + r*A3))             (3 FMAs)
 *     Degree-4:  P(r) = A0 + r*(A1 + r*(A2 + r*(A3 + r*A4)))    (4 FMAs)
 *   The polynomial evaluation below is arranged to maximize vfmacc usage.
 */

#include <stdint.h>
#include <inttypes.h>
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

#ifndef EXP_DEG
#define EXP_DEG 0
#endif

#define THRESHOLD 0.00010f

/* Reduction constants */
#define LOG2E_F  1.4426950408889634f
#define LN2_F    0.6931471805599453f

/* Degree-0 Schraudolph constants */
#define SCH_C  12102203.0f     /* 2^23 / ln(2) */
#define SCH_B  1064866805.0f   /* bias near (127<<23), tuned */

/* ========================= LMUL = 8 =========================
 * v0  : x / r
 * v8  : staging (poly)
 * v16 : k (i32) → bits(2^k)
 * v24 : P / result / temps
 */
static inline void vexp_m8_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle32.v   v0, (%0)" :: "r"(pin) : "memory");

#if (EXP_DEG == 0)
        /* Schraudolph: v24 = B + C*x  (FMA), then reinterpret to float bits */
        asm volatile("vfmv.v.f        v24, %[B]" :: [B]"f"(SCH_B));
        asm volatile("vfmacc.vf       v24, %[C], v0" :: [C]"f"(SCH_C));
        asm volatile("vfcvt.xu.f.v v8, v24");
        asm volatile("vse32.v         v8, (%0)" :: "r"(pout) : "memory");

#else
        /* y = x*LOG2E; k=nearint(y); r = x - k*LN2 */
        asm volatile("vfmul.vf        v24, v0,  %[log2e]" :: [log2e]"f"(LOG2E_F));
        asm volatile("vfcvt.x.f.v     v16, v24");                                   /* k (i32, RNE) */
        asm volatile("vfcvt.f.x.v     v24, v16");                                   /* k (f32) */
        asm volatile("vfnmsac.vf      v0,  %[ln2], v24" :: [ln2]"f"(LN2_F));        /* r in v0 */

#  if   (EXP_DEG == 2)
        /* t = A1 + r*A2 ;   P = A0 + r*t  (2 FMAs) */
        asm volatile("vfmv.v.f        v8,  %[a1]" :: [a1]"f"(1.01508951f));        /* t ← A1 */
        asm volatile("vfmacc.vf       v8,  %[a2], v0" :: [a2]"f"(0.50502354f));    /* t += r*A2 */
        asm volatile("vfmv.v.f        v24, %[a0]" :: [a0]"f"(0.99992448f));        /* P ← A0 */
        asm volatile("vfmacc.vv       v24, v0,  v8");                               /* P += r*t */

#  elif (EXP_DEG == 3)
        /* t = A2 + r*A3 ; u = A1 + r*t ; P = A0 + r*u  (3 FMAs total) */
        asm volatile("vfmv.v.f        v8,  %[a2]" :: [a2]"f"(0.50502354f));        /* t ← A2 */
        asm volatile("vfmacc.vf       v8,  %[a3], v0" :: [a3]"f"(0.16792160f));    /* t += r*A3 */
        asm volatile("vfmv.v.f        v24, %[a1]" :: [a1]"f"(0.99996230f));        /* u ← A1 */
        asm volatile("vfmacc.vv       v24, v0,  v8");                               /* u += r*t */
        asm volatile("vfmv.v.f        v8,  %[a0]" :: [a0]"f"(0.99992450f));        /* P ← A0 */
        asm volatile("vfmacc.vv       v8,  v0,  v24");                              /* P += r*u */
        asm volatile("vmv.v.v         v24, v8");                                    /* P -> v24 */

#  elif (EXP_DEG == 4)
        /* P(r) = A0 + r*(A1 + r*(A2 + r*(A3 + r*A4))) */
        asm volatile("vfmv.v.f  v8,  %[a3]" :: [a3]"f"(0.16792161f));          // s ← A3
        asm volatile("vfmacc.vf v8,  %[a4], v0" :: [a4]"f"(0.04191753f));      // s += r*A4

        asm volatile("vfmv.v.f  v24, %[a2]" :: [a2]"f"(0.49998870f));          // t ← A2
        asm volatile("vfmacc.vv v24, v0,  v8");                                // t += r*s   (r first!)

        asm volatile("vfmv.v.f  v8,  %[a1]" :: [a1]"f"(0.99996230f));          // u ← A1
        asm volatile("vfmacc.vv v8,  v0,  v24");                               // u += r*t   (r first!)

        asm volatile("vfmv.v.f  v24, %[a0]" :: [a0]"f"(1.00000000f));          // P ← A0
        asm volatile("vfmacc.vv v24, v0,  v8");         

#  endif

        /* y = P * 2^k via exponent injection */
        asm volatile("vadd.vx         v16, v16, %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v16, v16, 23");
        asm volatile("vfmul.vv        v0,  v24, v16");                               /* result in v0 */
        asm volatile("vse32.v         v0, (%0)" :: "r"(pout) : "memory");
#endif

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ========================= LMUL = 4 =========================
 * v12 : x / r
 * v24 : P / temps
 * v16 : k (i32) → bits(2^k)
 * v8  : staging
 */
static inline void vexp_m4_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle32.v   v12, (%0)" :: "r"(pin) : "memory");

#if (EXP_DEG == 0)
        asm volatile("vfmv.v.f        v24, %[B]" :: [B]"f"(SCH_B));
        asm volatile("vfmacc.vf       v24, %[C], v12" :: [C]"f"(SCH_C));
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");
        asm volatile("vse32.v         v24, (%0)" :: "r"(pout) : "memory");

#else
        asm volatile("vfmul.vf        v24, v12, %[log2e]" :: [log2e]"f"(LOG2E_F));
        asm volatile("vfcvt.x.f.v     v16, v24");                                   /* k */
        asm volatile("vfcvt.f.x.v     v24, v16");                                   /* kf */
        asm volatile("vfnmsac.vf      v12, %[ln2], v24" :: [ln2]"f"(LN2_F));        /* r */

#  if   (EXP_DEG == 2)
        asm volatile("vfmv.v.f        v8,  %[a1]" :: [a1]"f"(1.01508951f));
        asm volatile("vfmacc.vf       v8,  %[a2], v12" :: [a2]"f"(0.50502354f));
        asm volatile("vfmv.v.f        v24, %[a0]" :: [a0]"f"(0.99992448f));
        asm volatile("vfmacc.vv       v24, v12, v8");                                /* P in v24 */

#  elif (EXP_DEG == 3)
        asm volatile("vfmv.v.f        v8,  %[a2]" :: [a2]"f"(0.50502354f));         /* t */
        asm volatile("vfmacc.vf       v8,  %[a3], v12" :: [a3]"f"(0.16792160f));    /* t += r*A3 */
        asm volatile("vfmv.v.f        v24, %[a1]" :: [a1]"f"(0.99996230f));         /* u */
        asm volatile("vfmacc.vv       v24, v12, v8");                                /* u += r*t */
        asm volatile("vfmv.v.f        v8,  %[a0]" :: [a0]"f"(0.99992450f));         /* P */
        asm volatile("vfmacc.vv       v8,  v12, v24");                               /* P += r*u */
        asm volatile("vmv.v.v         v24, v8");

#  elif (EXP_DEG == 4)
        asm volatile("vfmv.v.f        v8,  %[a3]" :: [a3]"f"(0.16792161f));         /* s */
        asm volatile("vfmacc.vf       v8,  %[a4], v12" :: [a4]"f"(0.04191753f));    /* s += r*A4 */
        asm volatile("vfmv.v.f        v24, %[a2]" :: [a2]"f"(0.49998870f));         /* t */
        asm volatile("vfmacc.vv       v24, v12, v8");                                /* t += r*s */
        asm volatile("vfmv.v.f        v8,  %[a1]" :: [a1]"f"(0.99996230f));         /* u */
        asm volatile("vfmacc.vv       v8,  v12, v24");                               /* u += r*t */
        asm volatile("vfmv.v.f        v24, %[a0]" :: [a0]"f"(1.00000000f));         /* P */
        asm volatile("vfmacc.vv       v24, v12, v8");                                /* P += r*u */

        
#  endif

        asm volatile("vadd.vx         v16, v16, %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v16, v16, 23");
        asm volatile("vfmul.vv        v12, v24, v16");
        asm volatile("vse32.v         v12, (%0)" :: "r"(pout) : "memory");
#endif

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ========================= LMUL = 2 =========================
 * v4  : x / r
 * v10 : P / temps
 * v8  : k (i32) → bits(2^k)
 * v6  : staging
 */
static inline void vexp_m2_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle32.v   v4, (%0)" :: "r"(pin) : "memory");

#if (EXP_DEG == 0)
        asm volatile("vfmv.v.f        v10, %[B]" :: [B]"f"(SCH_B));
        asm volatile("vfmacc.vf       v10, %[C], v4" :: [C]"f"(SCH_C));
        asm volatile("vfcvt.rtz.xu.f.v v10, v10");
        asm volatile("vse32.v         v10, (%0)" :: "r"(pout) : "memory");

#else
        asm volatile("vfmul.vf        v10, v4,  %[log2e]" :: [log2e]"f"(LOG2E_F));
        asm volatile("vfcvt.x.f.v     v8,  v10");                                   /* k */
        asm volatile("vfcvt.f.x.v     v10, v8");
        asm volatile("vfnmsac.vf      v4,  %[ln2], v10" :: [ln2]"f"(LN2_F));        /* r */

#  if   (EXP_DEG == 2)
        asm volatile("vfmv.v.f        v6,  %[a1]" :: [a1]"f"(1.01508951f));
        asm volatile("vfmacc.vf       v6,  %[a2], v4" :: [a2]"f"(0.50502354f));
        asm volatile("vfmv.v.f        v10, %[a0]" :: [a0]"f"(0.99992448f));
        asm volatile("vfmacc.vv       v10, v4,  v6");                                /* P in v10 */

#  elif (EXP_DEG == 3)
        asm volatile("vfmv.v.f        v6,  %[a2]" :: [a2]"f"(0.50502354f));         /* t */
        asm volatile("vfmacc.vf       v6,  %[a3], v4" :: [a3]"f"(0.16792160f));     /* t += r*A3 */
        asm volatile("vfmv.v.f        v10, %[a1]" :: [a1]"f"(0.99996230f));         /* u */
        asm volatile("vfmacc.vv       v10, v4,  v6");                                /* u += r*t */
        asm volatile("vfmv.v.f        v6,  %[a0]" :: [a0]"f"(0.99992450f));         /* P */
        asm volatile("vfmacc.vv       v6,  v4,  v10");                               /* P += r*u */
        asm volatile("vmv.v.v         v10, v6");

#  elif (EXP_DEG == 4)
        asm volatile("vfmv.v.f        v6,  %[a3]" :: [a3]"f"(0.16792161f));         /* s */
        asm volatile("vfmacc.vf       v6,  %[a4], v4" :: [a4]"f"(0.04191753f));     /* s += r*A4 */
        asm volatile("vfmv.v.f        v10, %[a2]" :: [a2]"f"(0.49998870f));         /* t */
        asm volatile("vfmacc.vv       v10, v4,  v6");                                /* t += r*s */
        asm volatile("vfmv.v.f        v6,  %[a1]" :: [a1]"f"(0.99996230f));         /* u */
        asm volatile("vfmacc.vv       v6,  v4,  v10");                               /* u += r*t */
        asm volatile("vfmv.v.f        v10, %[a0]" :: [a0]"f"(1.00000000f));         /* P */
        asm volatile("vfmacc.vv       v10, v4,  v6");                                /* P += r*u */
#  endif

        asm volatile("vadd.vx         v8,  v8,  %[bias]" :: [bias]"r"(127));
        asm volatile("vsll.vi         v8,  v8,  23");
        asm volatile("vfmul.vv        v4,  v10, v8");
        asm volatile("vse32.v         v4,  (%0)" :: "r"(pout) : "memory");
#endif

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

static inline void vexp_custom(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;
        while (remaining > 0) {
                unsigned long vl;
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                         : "=r"(vl) : "r"(remaining) : "memory");
        
                asm volatile("vle32.v   v0, (%0)" :: "r"(pin) : "memory");


                asm volatile("vfexpf.v      v8, v0");
                asm volatile("vse32.v         v8, (%0)" :: "r"(pout) : "memory");
        
                pin  += vl;
                pout += vl;
                remaining -= (int)vl;
        }
}



// Fixed-shape “clean burst” exp kernel:
// - Assumes VLEN = 512 bits, e32,m1 => vl = 16 lanes
// - Assumes N = 4096 (or generally N is an exact multiple of 4*vl)
// - Uses contiguous regs v0..v3 (inputs) and v4..v7 (outputs) to avoid WAW on inputs
// - No remaining/tail logic (by request)

static inline void vexp_optimized(const float* inp, float* out, int N) {
  const float *pin  = inp;
  float       *pout = out;

  unsigned long vl;
  // e32,m1 with VLEN=512 => vl = 16
  asm volatile("vsetvli %0, zero, e32, m2, ta, ma" : "=r"(vl) :: "memory");

  // Process 4 vectors per iteration => 4*vl elements per iter
  // If N=4096 and vl=16 => iters = 4096/(64)=64
  const int iters = N / (int)(4ul * vl);

  for (int i = 0; i < iters; i++) {
    // --- STEP 1: LOAD PHASE (Fill VRF) ---
    asm volatile("vle32.v v0, (%0)" :: "r"(pin)            : "memory");
    asm volatile("vle32.v v2, (%0)" :: "r"(pin +      vl)  : "memory");
    asm volatile("vle32.v v4, (%0)" :: "r"(pin + 2ul*vl)   : "memory");
    asm volatile("vle32.v v6, (%0)" :: "r"(pin + 3ul*vl)   : "memory");

    asm volatile("" ::: "memory");

    // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
    // Use distinct destination regs to keep inputs intact and avoid WAW hazards.
    asm volatile("vfexpf.v v8, v0");
    asm volatile("vfexpf.v v10, v2");
    asm volatile("vfexpf.v v12, v4");
    asm volatile("vfexpf.v v14, v6");

    // --- STEP 3: STORE PHASE (Drain) ---
    asm volatile("vse32.v v8, (%0)" :: "r"(pout)           : "memory");
    asm volatile("vse32.v v10, (%0)" :: "r"(pout +      vl) : "memory");
    asm volatile("vse32.v v12, (%0)" :: "r"(pout + 2ul*vl)  : "memory");
    asm volatile("vse32.v v14, (%0)" :: "r"(pout + 3ul*vl)  : "memory");

    pin  += 4ul * vl;
    pout += 4ul * vl;
  }
}

// LMUL=4 “clean burst” with 4 register-groups per iteration
// - Fixed VTYPE: e32,m4
// - Uses group-aligned regs for m4: bases 0,4,8,12
// - Schraudolph EXP_DEG==0: t = B + C*x ; y = vfcvt.xu.f.v(t)
// - Stores 32-bit integer lanes via vse32
// - No remaining/tail logic: assumes N is a multiple of 4*vl
// Same “clean burst” structure, but with LMUL=4.
// Key rule: with LMUL=4, each vector group consumes 4 registers, so the group base
// must be a multiple of 4. Hence we use v0,v4,v8,v12 for inputs and v16,v20,v24,v28 for outputs.

static inline void vexp_optimized_m4(const float* inp, float* out, int N) {
  const float *pin  = inp;
  float       *pout = out;

  unsigned long vl;
  // Fixed VTYPE: e32, m4
  asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(vl) :: "memory");

  // 4 vector-groups per iteration => 4*vl elements/iter
  const int iters = N / (int)(4ul * vl);

  for (int i = 0; i < iters; i++) {
    // --- STEP 1: LOAD PHASE ---
    // group-aligned bases for m4: 0,4,8,12
    asm volatile("vle32.v v0,  (%0)" :: "r"(pin)            : "memory");
    asm volatile("vle32.v v8,  (%0)" :: "r"(pin +      vl)  : "memory");
    asm volatile("vle32.v v16,  (%0)" :: "r"(pin + 2ul*vl)   : "memory");
    asm volatile("vle32.v v24, (%0)" :: "r"(pin + 3ul*vl)   : "memory");

    asm volatile("" ::: "memory");

    // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
    // outputs also group-aligned: 16,20,24,28
    asm volatile("vfexpf.v v0, v0");
    asm volatile("vfexpf.v v8, v8");
    asm volatile("vfexpf.v v16, v16");
    asm volatile("vfexpf.v v24, v24");

    // --- STEP 3: STORE PHASE ---
    asm volatile("vse32.v v0, (%0)" :: "r"(pout)            : "memory");
    asm volatile("vse32.v v8, (%0)" :: "r"(pout +      vl)  : "memory");
    asm volatile("vse32.v v16, (%0)" :: "r"(pout + 2ul*vl)   : "memory");
    asm volatile("vse32.v v24, (%0)" :: "r"(pout + 3ul*vl)   : "memory");

    pin  += 4ul * vl;
    pout += 4ul * vl;
  }
}


static inline void vexp_m4_burst4(const float* inp, float* out, int N) {
  const float *pin  = inp;
  float       *pout = out;

#if (EXP_DEG != 0)
# error "Only EXP_DEG==0 path implemented here."
#endif

  unsigned long vl;
  // Fixed VTYPE for clean waveforms
  asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(vl) :: "memory");

  // 4 vector-groups per iter
  const int iters = N / (int)(2ul * vl);

  for (int i = 0; i < iters; i++) {
    // --- STEP 1: LOAD PHASE ---
    // m4 groups occupy 4 regs each -> use 0,4,8,12
    asm volatile("vle32.v v0,  (%0)" :: "r"(pin)            : "memory");
    asm volatile("vle32.v v8,  (%0)" :: "r"(pin +      vl)  : "memory");
    

    asm volatile("" ::: "memory");

    // --- STEP 2: COMPUTE BURST ---
    // Temps:   v16, v20, v24, v28  (group-aligned for m4)
    // Outputs: v2,  v6,  v10, v14  (also group-aligned: 2,6,10,14 are NOT aligned!)
    //
    // IMPORTANT: for m4, base reg must be a multiple of 4.
    // So choose outputs at 0/4/8/12/16/20/24/28 only.
    // We'll store directly from the temp regs to keep everything aligned.

    asm volatile("vfmv.v.f     v16, %[B]" :: [B]"f"(SCH_B));
    asm volatile("vfmacc.vf    v16, %[C], v0"  :: [C]"f"(SCH_C));
    asm volatile("vfcvt.xu.f.v v16, v16");

    asm volatile("vfmv.v.f     v24, %[B]" :: [B]"f"(SCH_B));
    asm volatile("vfmacc.vf    v24, %[C], v8"  :: [C]"f"(SCH_C));
    asm volatile("vfcvt.xu.f.v v24, v24");


    // --- STEP 3: STORE PHASE ---
    asm volatile("vse32.v v16, (%0)" :: "r"(pout)            : "memory");
    asm volatile("vse32.v v24, (%0)" :: "r"(pout +      vl)  : "memory");
   

    pin  += 2ul * vl;
    pout += 2ul * vl;
  }
}

// Fixed-shape microbenchmark kernel:

static inline void vexp_m1_vlen512_N4096(const float* inp, float* out) {
  const float *pin  = inp;
  float       *pout = out;

  unsigned long vl;
  // With VLEN=512 and e32,m1: vl = VLEN/32 = 16 lanes
  asm volatile("vsetvli %0, zero, e32, m1, ta, ma" : "=r"(vl) :: "memory");

  // N = 4096, each iter handles 4*vl elements => 4096 / (4*16) = 64 iters
  for (int i = 0; i < 64; i++) {
    // --- STEP 1: LOAD PHASE ---
    asm volatile("vle32.v v0, (%0)" :: "r"(pin)            : "memory");
    asm volatile("vle32.v v1, (%0)" :: "r"(pin +     vl)   : "memory");
    asm volatile("vle32.v v2, (%0)" :: "r"(pin + 2ul*vl)   : "memory");
    asm volatile("vle32.v v3, (%0)" :: "r"(pin + 3ul*vl)   : "memory");

    asm volatile("" ::: "memory");

    // --- STEP 2: COMPUTE BURST ---
    // Mirrors your EXP_DEG==0 draft: t = B + C*x ; y = trunc_to_uint(t)
    // (stored as 32-bit lanes into out)
    asm volatile("vfmv.v.f         v4, %[B]" :: [B]"f"(SCH_B));
    asm volatile("vfmacc.vf        v4, %[C], v0" :: [C]"f"(SCH_C));
    asm volatile("vfcvt.rtz.xu.f.v v4, v4");

    asm volatile("vfmv.v.f         v5, %[B]" :: [B]"f"(SCH_B));
    asm volatile("vfmacc.vf        v5, %[C], v1" :: [C]"f"(SCH_C));
    asm volatile("vfcvt.rtz.xu.f.v v5, v5");

    asm volatile("vfmv.v.f         v6, %[B]" :: [B]"f"(SCH_B));
    asm volatile("vfmacc.vf        v6, %[C], v2" :: [C]"f"(SCH_C));
    asm volatile("vfcvt.rtz.xu.f.v v6, v6");

    asm volatile("vfmv.v.f         v7, %[B]" :: [B]"f"(SCH_B));
    asm volatile("vfmacc.vf        v7, %[C], v3" :: [C]"f"(SCH_C));
    asm volatile("vfcvt.rtz.xu.f.v v7, v7");

    // --- STEP 3: STORE PHASE ---
    asm volatile("vse32.v v4, (%0)" :: "r"(pout)           : "memory");
    asm volatile("vse32.v v5, (%0)" :: "r"(pout +      vl) : "memory");
    asm volatile("vse32.v v6, (%0)" :: "r"(pout + 2ul*vl)  : "memory");
    asm volatile("vse32.v v7, (%0)" :: "r"(pout + 3ul*vl)  : "memory");

    pin  += 4ul * vl;
    pout += 4ul * vl;
  }
}


/* ------------------- Kernel selector ------------------- */
static inline void vexp_strip(const float* inp, float* out, int N) {
#if   (LMUL_MODE == 8)
    vexp_m8_strip(inp, out, N);
#elif (LMUL_MODE == 4)
    vexp_m4_strip(inp, out, N);
#elif (LMUL_MODE == 2)
    vexp_m2_strip(inp, out, N);
#else
#   error "LMUL_MODE must be 8, 4, or 2"
#endif
}

/* ------------------ Simple golden checker ------------------ */
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

        vexp_optimized_m4(g_in + start, g_out + start, count);

        unsigned cycles = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[exp LMUL=%d DEG=%d] core %u cycles: %u\n",
               LMUL_MODE, EXP_DEG, cid, cycles);
    }

    snrt_cluster_hw_barrier();

     /*(Optional) golden check */
    if (cid == 0) {
       printf("CHECK RESULTS (exp)\n");
       check_result(g_in,g_out, outE, N);
    }
    

    snrt_cluster_hw_barrier();
    return 0;
}
