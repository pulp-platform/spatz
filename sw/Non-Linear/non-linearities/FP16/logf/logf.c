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
 * 
 * --------------------------------------------------------------------------
 * 
 * RVV logf (SEW=16) — strip-mined, maskless, runtime-selectable LMUL (8/4/2).
 *
 * Domain reduction:
 *   x = m * 2^e, with m ∈ [1, 2).  ln(x) = ln(m) + e * ln(2)
 *
 * Approximation (degree-4 on t = m - 1):
 *   ln(m) ≈ t * (L1 + t*(L2 + t*(L3 + t*L4)))
 *
 * LMUL=8 variant uses only v0, v8, v16, v24 (no spills).
 * Two-core split: core 0 → [0, N/2), core 1 → [N/2, N)
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
#define LMUL_MODE 8     // set to 8/4/2 at compile time, e.g. -DLMUL_MODE=4
#endif

#define THRESHOLD 0.00010f

// Degree-4 Chebyshev-like fit for ln(1+t) on t∈[0,1)
#define L1   0.99999994f
#define L2  -0.49999905f
#define L3   0.33333197f
#define L4  -0.24999910f

// ln(2) split (higher precision accumulation with two FMAs)
#define LN2_HI  0.693145751953125f        // high part
#define LN2_LO  1.428606765330187e-06f    // low part so that LN2_HI + LN2_LO ≈ ln(2)

// L1 pointers shared in L1 (published by core 0)
static __fp16 *g_in  = NULL;
static __fp16 *g_out = NULL;

/* ========================= LMUL = 8 =========================
 * Reg plan (SEW=16, LMUL=8),  4 vector registers:
 *   v8  : x (__fp16)
 *   v0  : xi / scalar ones (int/float-as-bits)
 *   v16 : m -> t -> ln(m) (__fp16)
 *   v24 : e (int/float) or poly accumulator / temps
 */
static inline void vlogf_m8_strip(const __fp16* inp, __fp16* out, int N)
{
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma": "=r"(vl): "r"(remaining): "memory");

        // Load x → v8
        asm volatile("vle16.v v8, (%0)" :: "r"(pin) : "memory");

        // e = ((xi >> 23) - 127) as int in v24
        asm volatile("vmv.v.v   v24, v8");           // copy x bits
        asm volatile("vsrl.vi   v24, v24, 10");      // shift exponent to LSB (FP16: 10-bit mantissa)
        asm volatile("vmv.v.x   v16, %0" :: "r"(15));
        asm volatile("vsub.vv   v24, v24, v16");     // v24 = e (int)

        // m in [1,2): xi = (xi & 0x03FF) | 0x3C00  (FP16 format)
        asm volatile("vmv.v.x   v16, %0" :: "r"(0x03FFu));
        asm volatile("vand.vv   v8,  v8,  v16");     // clear sign+exp
        asm volatile("vmv.v.x   v16, %0" :: "r"(0x3C00u));
        asm volatile("vor.vv    v8,  v8,  v16");     // v8 = m bits

        // t = m - 1.0  → keep in v8
        asm volatile("vfmv.v.f  v0,  %0" :: "f"(1.0f));
        asm volatile("vfsub.vv  v8, v8, v0");        // v8 = t

        // ln(m) poly: ln(1+t) ≈ t * (L1 + t*(L2 + t*(L3 + t*L4)))
        asm volatile("vfmv.v.f  v0, %0" :: "f"(L4));   // p = L4
        asm volatile("vfmul.vv  v0, v0, v8");          // p = p*t
        asm volatile("vfadd.vf  v0, v0, %0" :: "f"(L3));
        asm volatile("vfmul.vv  v0, v0, v8");          // p = p*t
        asm volatile("vfadd.vf  v0, v0, %0" :: "f"(L2));
        asm volatile("vfmul.vv  v0, v0, v8");          // p = p*t
        asm volatile("vfadd.vf  v0, v0, %0" :: "f"(L1));
        asm volatile("vfmul.vv  v8, v8, v0");          // v8 = t * p = ln(m)

        // e (int) → __fp16         asm volatile("vfcvt.f.x.v v24, v24");          // v24 = (float)e

        // ln(x) = ln(m) + e*ln2, split for precision
        asm volatile("vfmacc.vf v8, %0, v24" :: "f"(LN2_HI));
        asm volatile("vfmacc.vf v8, %0, v24" :: "f"(LN2_LO));

        // Store result
        asm volatile("vse16.v v8, (%0)" :: "r"(pout) : "memory");

        pin       += vl;
        pout      += vl;
        remaining -= (int)vl;
    }
}

static inline void vlogf_4_Degree(const __fp16* inp, __fp16* out, int N)
{
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    // 1. Pre-load integer constants into standard registers
    // These will be passed as inputs "r" to the asm block
    unsigned int bias_int = 15;
    unsigned int mask_mant = 0x03FF;
    unsigned int one_bits  = 0x3C00;

    // 2. Pre-load __fp16 constants into fp registers
    // These will be passed as inputs "f"
    // (Variables L1..L4, LN2_HI, LN2_LO are used directly)

    while (remaining > 0) {
        size_t vl;

        // Implementation Note:
        // v8  (m8): holds 'x', then mantissa 'm', then 't', then final result
        // v24 (m8): holds exponent 'e' (integer then float)
        // v0  (m8): accumulator for polynomial 'p'
        // This leaves v16 (m8) completely free or available for other optimizations.

        asm volatile(
            // Set vector length to max supported by m8 (LMUL=8)
            "vsetvli %0, %1, e16, m8, ta, ma \n\t"

            // ---------------------------------------------------------
            // 1. Load Input: x
            // ---------------------------------------------------------
            "vle16.v v8, (%2) \n\t"

            // ---------------------------------------------------------
            // 2. Extract Exponent: e = ((x >> 10) - 15)
            // ---------------------------------------------------------
            "vmv.v.v  v24, v8 \n\t"           // Copy x to v24
            "vsrl.vi  v24, v24, 10 \n\t"      // Shift exponent to LSB
            "vsub.vx  v24, v24, %3 \n\t"      // Subtract bias (127). Uses scalar input!
            "vfcvt.f.x.v v24, v24 \n\t"       // Convert exponent to __fp16 
            // ---------------------------------------------------------
            // 3. Extract Mantissa: m = (x & 0x03FF) | 0x3C00
            // ---------------------------------------------------------
            "vand.vx  v8, v8, %4 \n\t"        // Mask off exponent/sign. Uses scalar input!
            "vor.vx   v8, v8, %5 \n\t"        // Force exponent to 1.0. Uses scalar input!
            
            // t = m - 1.0
            "vfsub.vf v8, v8, %6 \n\t"        // v8 = t. Uses scalar 1.0f!

            // ---------------------------------------------------------
            // 4. Polynomial Approximation: ln(1+t) using Horner's Method
            // Poly: t * (L1 + t * (L2 + t * (L3 + t * L4)))
            // ---------------------------------------------------------
            
            // Init accumulator p = L4
            "vfmv.v.f v0, %10 \n\t"           
            
            // p = p * t + L3
            "vfmul.vv v0, v0, v8 \n\t"
            "vfadd.vf v0, v0, %9 \n\t"

            // p = p * t + L2
            "vfmul.vv v0, v0, v8 \n\t"
            "vfadd.vf v0, v0, %8 \n\t"

            // p = p * t + L1
            "vfmul.vv v0, v0, v8 \n\t"
            "vfadd.vf v0, v0, %7 \n\t"

            // Final: ln_m = p * t
            "vfmul.vv v8, v0, v8 \n\t" 

            // ---------------------------------------------------------
            // 5. Combine: result = ln_m + e * ln2
            // ---------------------------------------------------------
            // We use fused multiply-accumulate (vfmacc) for speed
            // v8 = v8 + (e * LN2_HI)
            "vfmacc.vf v8, %11, v24 \n\t"
            // v8 = v8 + (e * LN2_LO)
            "vfmacc.vf v8, %12, v24 \n\t"

            // ---------------------------------------------------------
            // 6. Store Result
            // ---------------------------------------------------------
            "vse16.v v8, (%13) \n\t"

            : "=&r"(vl)                                     // %0: Output VL
            : "r"(remaining),                               // %1: Input N
              "r"(pin),                                     // %2: Input pointer
              "r"(bias_int),                                // %3: 15 (FP16 bias)
              "r"(mask_mant),                               // %4: 0x03FF (FP16 mantissa)
              "r"(one_bits),                                // %5: 0x3C00 (FP16 1.0)
              "f"(1.0f),                                    // %6: 1.0f
              "f"(L1), "f"(L2), "f"(L3), "f"(L4),           // %7-%10: Coeffs
              "f"(LN2_HI), "f"(LN2_LO),                     // %11-%12: LN2
              "r"(pout)                                     // %13: Output pointer
            : "v0", "v8", "v16", "v24", "memory"            // Clobbers (all vector groups)
        );

        pin       += vl;
        pout      += vl;
        remaining -= vl;
    }
}


// -----------------------------------------------------------------------------
// Constants for "Cheap" Log Approximation (Mitchell + Quadratic)
// -----------------------------------------------------------------------------
static const __fp16 LN2   = 0.69314718f;
static const __fp16 HALF  = 0.5f;
static const __fp16 ONE   = 1.0f;

// -----------------------------------------------------------------------------
// Function: vlogf_approx_a2
// Algorithm: ln(x) = k*ln2 + (t - 0.5*t^2)
//            where x = 2^k * (1+t), t in [0, 1)
// -----------------------------------------------------------------------------
static inline void vlogf_approx(const __fp16* inp, __fp16* out, int N)
{
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    
    unsigned int bias_int  = 15;       // FP16 exponent bias
    unsigned int mask_mant = 0x03FF;   // FP16 mantissa mask
    unsigned int one_bits  = 0x3C00;   // FP16 1.0 bit pattern

    while (remaining > 0) {
        size_t vl;

        // Strip-mining: Set Vector Length for 16-bit half, Grouping=8
        asm volatile(
            "vsetvli %0, %1, e16, m8, ta, ma"
            : "=r"(vl) : "r"(remaining)
        );

        // Inline Assembly Block implementing your specific algorithm
        asm volatile(
            // -----------------------------------------------------------------
            // 1. Load Data
            // -----------------------------------------------------------------
            "vle16.v v8, (%2) \n\t"          // v8 = x

            // -----------------------------------------------------------------
            // 2. Extract Exponent (k) -> Mapped to v24
            // -----------------------------------------------------------------
            "vmv.v.v  v24, v8 \n\t"          // Copy x to v24
            "vsrl.vi  v24, v24, 10 \n\t"     // Shift right 10 (FP16 mantissa width)
            "vsub.vx  v24, v24, %3 \n\t"     // k_int = exp - 15
            "vfcvt.f.x.v v24, v24 \n\t"      // v24 = k___fp16 

            // -----------------------------------------------------------------
            // 3. Extract Mantissa (z) -> Mapped to v8 (reuse input reg)
            // -----------------------------------------------------------------
            "vand.vx  v8, v8, %4 \n\t"       // Mask mantissa 
            "vor.vx   v8, v8, %5 \n\t"       // Force exponent 1.0 (z)
            
            // t = z - 1.0 -> Mapped to v16
            "vfsub.vf v16, v8, %6 \n\t"      // v16 = t 

            // -----------------------------------------------------------------
            // 4. Quadratic Correction: term2 = t - 0.5 * t^2
            // -----------------------------------------------------------------
            // v0 = t^2 (User's v18)
            "vfmul.vv v0, v16, v16 \n\t"     

            // Calculation: result = t - (0.5 * t^2)
            // We use vfnmsac (Negative Multiply-Subtract-Accumulate)
            // Dest = Dest - (Src1 * Src2)
            
            "vmv.v.v  v8, v16 \n\t"          // Move 't' to result register v8
            "vfnmsac.vf v8, %7, v0 \n\t"     // v8 = v8 - (0.5 * v0) 
                                             // v8 is now the "correction" term

            // -----------------------------------------------------------------
            // 5. Final Combine: out = correction + k * ln2
            // -----------------------------------------------------------------
            // vfmacc: Dest = Dest + (Src1 * Src2)
            "vfmacc.vf v8, %8, v24 \n\t"     // v8 += LN2 * k_float

            // -----------------------------------------------------------------
            // 6. Store Result
            // -----------------------------------------------------------------
            "vse16.v v8, (%9) \n\t"

            // Constraints
            : "=&r"(vl)                      // %0
            : "r"(remaining),                // %1
              "r"(pin),                      // %2
              "r"(bias_int),                 // %3: 15 (FP16 bias)
              "r"(mask_mant),                // %4: 0x03FF (FP16 mantissa)
              "r"(one_bits),                 // %5: 0x3C00 (FP16 1.0)
              "f"(ONE),                      // %6: 1.0
              "f"(HALF),                     // %7: 0.5
              "f"(LN2),                      // %8: 0.693...
              "r"(pout)                      // %9
            : "memory", "v0", "v8", "v16", "v24" // Clobbers
        );

        pin       += vl;
        pout      += vl;
        remaining -= vl;
    }
}


// -----------------------------------------------------------------------------
// Constants for "Bit-Hack" Approximation
// -----------------------------------------------------------------------------
// Magic Bias: Represents __fp16(1.0) as integer (0x3C00).
// Subtracting this centers the result so log(1.0) = 0.
static const unsigned int MAGIC_BIAS = 0x3C00; 

// Scaling Factor: ln(2) / 2^10
// We divide by 2^10 to normalize the FP16 integer bits, then multiply by ln(2)
// to convert log2 to natural log.
// Value: 0.69314718 / 1024.0 = 6.7690e-4
static const __fp16 SCALE_FACTOR = 6.7690545e-4f;

static inline void vlogf_optimized_m4(const __fp16* inp, __fp16* out, int N) {
  const __fp16 *pin  = inp;
  __fp16       *pout = out;

  unsigned long vl;
  // Fixed VTYPE: e16, m4 — with VLEN=512: vl = 128
  asm volatile("vsetvli %0, zero, e16, m4, ta, ma" : "=r"(vl) :: "memory");

  // 4 vector-groups per iteration => 4*vl elements/iter
  const int iters = N / (int)(4ul * vl);

  for (int i = 0; i < iters; i++) {
    // --- STEP 1: LOAD PHASE ---
    // group-aligned bases for m4: 0,4,8,12
    asm volatile("vle16.v v0,  (%0)" :: "r"(pin)            : "memory");
    asm volatile("vle16.v v4,  (%0)" :: "r"(pin +      vl)  : "memory");
    asm volatile("vle16.v v8,  (%0)" :: "r"(pin + 2ul*vl)   : "memory");
    asm volatile("vle16.v v12, (%0)" :: "r"(pin + 3ul*vl)   : "memory");

    asm volatile("" ::: "memory");

    // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
    // outputs in separate group-aligned regs: 16,20,24,28
    asm volatile("vflog.v v16, v0");
    asm volatile("vflog.v v20, v4");
    asm volatile("vflog.v v24, v8");
    asm volatile("vflog.v v28, v12");

    // --- STEP 3: STORE PHASE ---
    asm volatile("vse16.v v16, (%0)" :: "r"(pout)            : "memory");
    asm volatile("vse16.v v20, (%0)" :: "r"(pout +      vl)  : "memory");
    asm volatile("vse16.v v24, (%0)" :: "r"(pout + 2ul*vl)   : "memory");
    asm volatile("vse16.v v28, (%0)" :: "r"(pout + 3ul*vl)   : "memory");

    pin  += 4ul * vl;
    pout += 4ul * vl;
  }
}

static inline void vlogf_m8_cheapest(const __fp16* inp, __fp16* out, int N)
{
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    // Pre-load scalar constants
    unsigned int bias = MAGIC_BIAS;
    __fp16 scale = SCALE_FACTOR;

    while (remaining > 0) {
        size_t vl;
        asm volatile(
            "vsetvli %0, %1, e16, m8, ta, ma" 
            : "=r"(vl) : "r"(remaining)
        );

        asm volatile(
            // -----------------------------------------------------------------
            // 1. Load Data (Interpret bits as Integer)
            // -----------------------------------------------------------------
            "vle16.v v0, (%2) \n\t"        // v8 = I_x (Raw Bits)

            // -----------------------------------------------------------------
            // 2. Integer Subtract (The "Log" step)
            // -----------------------------------------------------------------
            // Result = I_x - Bias
            "vsub.vx v8, v0, %3 \n\t"      // v8 is now a signed integer

            // -----------------------------------------------------------------
            // 3. Convert to Float
            // -----------------------------------------------------------------
            // Cast the integer result directly to __fp16             "vfcvt.f.x.v v16, v8 \n\t"      // v8 is now float(I_x - Bias)

            // -----------------------------------------------------------------
            // 4. Scale to Natural Log
            // -----------------------------------------------------------------
            // Final = float_val * (ln2 / 2^10)
            "vfmul.vf v24, v16, %4 \n\t"     // v16 = ln(x)

            // -----------------------------------------------------------------
            // 5. Store
            // -----------------------------------------------------------------
            "vse16.v v24, (%5) \n\t"

            : "=&r"(vl)                    // %0
            : "r"(remaining),              // %1
              "r"(pin),                    // %2
              "r"(bias),                   // %3: 0x3C00 (FP16 1.0)
              "f"(scale),                  // %4: 6.77e-4 (ln2/2^10)
              "r"(pout)                    // %5
            : "memory", "v8"               // Only ONE register group used!
        );

        pin       += vl;
        pout      += vl;
        remaining -= vl;
    }
}
/* ========================= LMUL = 4 =========================
 * Wider register set; conventional staging.
 */
static inline void vlogf_m4_strip(const __fp16* inp, __fp16* out, int N) {
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        // x → v12; xi → v8
        asm volatile("vle16.v v12, (%0)" :: "r"(pin) : "memory");
        asm volatile("vmv.v.v  v8,  v12");

        // e = ((xi>>10)-15) (int in v28) — FP16 format
        asm volatile("vmv.v.v   v28, v8");
        asm volatile("vsrl.vi   v28, v28, 10");
        asm volatile("vmv.v.x   v4, %0" :: "r"(15));
        asm volatile("vsub.vv   v28, v28, v4");

        // m = (xi & 0x03FF) | 0x3C00 — FP16 mantissa extraction
        asm volatile("vmv.v.x   v4, %0" :: "r"(0x03FFu));
        asm volatile("vand.vv   v8, v8, v4");
        asm volatile("vmv.v.x   v4, %0" :: "r"(0x3C00u));
        asm volatile("vor.vv    v8, v8, v4");

        // t = m - 1
        asm volatile("vmv.v.v   v16, v8");
        asm volatile("vfmv.v.f  v4,  %0" :: "f"(1.0f));
        asm volatile("vfsub.vv  v16, v16, v4");

        // ln(m) poly → v16
        asm volatile("vfmv.v.f  v24, %0" :: "f"(L4));
        asm volatile("vfmul.vv  v24, v24, v16");
        asm volatile("vfadd.vf  v24, v24, %0" :: "f"(L3));
        asm volatile("vfmul.vv  v24, v24, v16");
        asm volatile("vfadd.vf  v24, v24, %0" :: "f"(L2));
        asm volatile("vfmul.vv  v24, v24, v16");
        asm volatile("vfadd.vf  v24, v24, %0" :: "f"(L1));
        asm volatile("vfmul.vv  v16, v16, v24");

        // ln(x) = ln(m) + e*ln2  (split for precision)
        asm volatile("vfcvt.f.x.v v24, v28");
        asm volatile("vfmacc.vf   v16, %0, v24" :: "f"(LN2_HI));
        asm volatile("vfmacc.vf   v16, %0, v24" :: "f"(LN2_LO));

        asm volatile("vse16.v v16, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ========================= LMUL = 2 ========================= */
static inline void vlogf_m2_strip(const __fp16* inp, __fp16* out, int N) {
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        // x → v4; xi → v22
        asm volatile("vle16.v v4, (%0)" :: "r"(pin) : "memory");
        asm volatile("vmv.v.v  v22, v4");

        // e = ((xi>>10)-15) (int in v24) — FP16 format
        asm volatile("vmv.v.v   v24, v22");
        asm volatile("vsrl.vi   v24, v24, 10");
        asm volatile("vmv.v.x   v2, %0" :: "r"(15));
        asm volatile("vsub.vv   v24, v24, v2");

        // m = (xi & 0x03FF) | 0x3C00 — FP16 mantissa extraction
        asm volatile("vmv.v.x   v2, %0" :: "r"(0x03FFu));
        asm volatile("vand.vv   v22, v22, v2");
        asm volatile("vmv.v.x   v2, %0" :: "r"(0x3C00u));
        asm volatile("vor.vv    v22, v22, v2");

        // t = m - 1
        asm volatile("vmv.v.v   v6, v22");
        asm volatile("vfmv.v.f  v2,  %0" :: "f"(1.0f));
        asm volatile("vfsub.vv  v6, v6, v2");

        // ln(m) poly → v6
        asm volatile("vfmv.v.f  v8, %0" :: "f"(L4));
        asm volatile("vfmul.vv  v8, v8, v6");
        asm volatile("vfadd.vf  v8, v8, %0" :: "f"(L3));
        asm volatile("vfmul.vv  v8, v8, v6");
        asm volatile("vfadd.vf  v8, v8, %0" :: "f"(L2));
        asm volatile("vfmul.vv  v8, v8, v6");
        asm volatile("vfadd.vf  v8, v8, %0" :: "f"(L1));
        asm volatile("vfmul.vv  v6, v6, v8");

        // ln(x) = ln(m) + e*ln2  (split for precision)
        asm volatile("vfcvt.f.x.v v8, v24");
        asm volatile("vfmacc.vf   v6, %0, v8" :: "f"(LN2_HI));
        asm volatile("vfmacc.vf   v6, %0, v8" :: "f"(LN2_LO));

        asm volatile("vse16.v v6, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ------------------- Kernel selector ------------------- */
static inline void vlogf_strip(const __fp16* inp, __fp16* out, int N) {
#if   (LMUL_MODE == 8)
    vlogf_m8_strip(inp, out, N);
#elif (LMUL_MODE == 4)
    vlogf_m4_strip(inp, out, N);
#elif (LMUL_MODE == 2)
    vlogf_m2_strip(inp, out, N);
#else
#   error "LMUL_MODE must be 8, 4, or 2"
#endif
}

/* ------------------ Simple golden checker ------------------ */
static void check_result_fp16(const __fp16 *input, const __fp16 *x, const __fp16 *ref, int r) {
    int err = 0;
    for (int i = 0; i < r; i++) {
        float x_f = (float)x[i];
        float ref_f = (float)ref[i];
        float input_f = (float)input[i];
        float diff = fabsf(x_f - ref_f);
        printf("At index %d:\t value %f\t expected %f\t result %f\t error %f\n",
                i, input_f, ref_f, x_f, diff);
    }
}

/* ----------------------------- Main ----------------------------- */
int main(void) {
    const unsigned int cid = snrt_cluster_core_idx();
    snrt_cluster_hw_barrier();

    const int B = B_Size, C = C_Size, T = T_Size;
    const int N = B * T * C;

    if (cid == 0) {
        g_in  = (__fp16*)snrt_l1alloc(N * sizeof(__fp16));
        g_out = (__fp16*)snrt_l1alloc(N * sizeof(__fp16));
        if (!g_in || !g_out) { printf("alloc failed\n"); return 1; }

        // Load input from DRAM once, publish to other core
        snrt_dma_start_1d(g_in, data1_dram, N * sizeof(__fp16));
        snrt_dma_wait_all();
        memset(g_out, 0, N * sizeof(__fp16));
    }
    snrt_cluster_hw_barrier();  // publish g_in/g_out

    // Two-core split: core 0 does first half, core 1 does second half
    const int mid   = N >> 1;
    const int start = (cid == 0) ? 0   : mid;
    const int count = (cid == 0) ? mid : (N - mid);

    // Per-core compute
    if (count > 0) {

        start_kernel();
        unsigned t0 = benchmark_get_cycle();

        vlogf_optimized_m4(g_in + start, g_out + start, count);

        unsigned cycles = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[logf LMUL=%d] core %u cycles: %u\n", LMUL_MODE, cid, cycles);
    }

    snrt_cluster_hw_barrier();

    // Validate on core 0
    if (cid == 0) {
        printf("CHECK RESULTS (log)\n");
        check_result_fp16(g_in, g_out, outL, N);
    }

    snrt_cluster_hw_barrier();
    return 0;
}
