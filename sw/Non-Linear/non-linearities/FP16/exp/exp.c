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
 * RVV expf (SEW=16, FP16) — strip-mined, maskless, runtime-selectable LMUL (8/4/2).
 *
 * Build-time params:
 *   -DLMUL_MODE=8|4|2     (default: 8)
 *
 * Identities / approach:
 *   Schraudolph fast exp (EXP_DEG=0):
 *     E(x) ≈ *(half*)&( (uint16_t)( B + C*x ) )   // fused here as vfmacc
 *     with C = 2^10 / ln(2) ≈ 1448.15f (FP16 optimized),  B ≈ 15360.0f
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

#define THRESHOLD 0.00010f

/* Degree-0 Schraudolph constants (optimized for FP16) */
#define SCH_C_FP16  1477.32f     /* 2^10 / ln(2) for FP16 */
#define SCH_B_FP16  15360.0f     /* bias = 15<<10, tuned for FP16 */
// Minimum Max-relative error 
// #define SCH_C_FP16  1477.32f
// #define SCH_B_FP16  15315.5f   /* 15360 - 44.5 */

/* ========================= LMUL = 8 =========================
 * v0  : x
 * v8  : staging (poly)
 * v16 : result/temps
 */
static inline void vexp_m8_strip_fp16(const __fp16* inp, __fp16* out, int N) {
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle16.v   v0, (%0)" :: "r"(pin) : "memory");

        /* Schraudolph: v16 = B + C*x  (FMA), then reinterpret to float bits */
        asm volatile("vfmv.v.f        v16, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf       v16, %[C], v0" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v8, v16");
        asm volatile("vse16.v         v8, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ========================= LMUL = 4 =========================
 * v12 : x
 * v16 : P / temps
 * v8  : staging
 */
static inline void vexp_m4_strip_fp16(const __fp16* inp, __fp16* out, int N) {
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle16.v   v12, (%0)" :: "r"(pin) : "memory");

        asm volatile("vfmv.v.f        v16, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf       v16, %[C], v12" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v16, v16");
        asm volatile("vse16.v         v16, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ========================= LMUL = 2 =========================
 * v4  : x
 * v6  : P / temps
 * v8  : staging
 */
static inline void vexp_m2_strip_fp16(const __fp16* inp, __fp16* out, int N) {
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        asm volatile("vle16.v   v4, (%0)" :: "r"(pin) : "memory");

        asm volatile("vfmv.v.f        v6, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf       v6, %[C], v4" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v6, v6");
        asm volatile("vse16.v         v6, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* Fixed-shape "clean burst" exp kernel for FP16 using vfexpf.v:
 * - Assumes VLEN = 512 bits, e16,m1 => vl = 32 lanes
 * - Assumes N = 4096 (or generally N is an exact multiple of 4*vl)
 * - Uses custom vfexpf instruction for fast FP16 exponential
 */
static inline void vexp_optimized_fp16(const __fp16* inp, __fp16* out, int N) {
  const __fp16 *pin  = inp;
  __fp16       *pout = out;

  unsigned long vl;
  // e16,m1 with VLEN=512 => vl = 32
  asm volatile("vsetvli %0, zero, e16, m8, ta, ma" : "=r"(vl) :: "memory");

  // Process 4 vectors per iteration => 4*vl elements per iter
  const int iters = N / (int)(4ul * vl);

  for (int i = 0; i < iters; i++) {
    // --- STEP 1: LOAD PHASE (Fill VRF) ---
    asm volatile("vle16.v v0, (%0)" :: "r"(pin)            : "memory");
    asm volatile("vle16.v v8, (%0)" :: "r"(pin +      vl)  : "memory");
    asm volatile("vle16.v v16, (%0)" :: "r"(pin + 2ul*vl)   : "memory");
    asm volatile("vle16.v v24, (%0)" :: "r"(pin + 3ul*vl)   : "memory");

    asm volatile("" ::: "memory");

    // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
    // Use custom vfexpf.v instruction (FP16 hardware exponential)
    asm volatile("vfexpf.v v0, v0");
    asm volatile("vfexpf.v v8, v8");
    asm volatile("vfexpf.v v16, v16");
    asm volatile("vfexpf.v v24, v24");

    // --- STEP 3: STORE PHASE (Drain) ---
    asm volatile("vse16.v v0, (%0)" :: "r"(pout)           : "memory");
    asm volatile("vse16.v v8, (%0)" :: "r"(pout +      vl) : "memory");
    asm volatile("vse16.v v16, (%0)" :: "r"(pout + 2ul*vl)  : "memory");
    asm volatile("vse16.v v24, (%0)" :: "r"(pout + 3ul*vl)  : "memory");

    pin  += 4ul * vl;
    pout += 4ul * vl;
  }
}

/* LMUL=4 "clean burst" variant using vfexpf.v:
 * - Assumes VLEN = 512 bits, e16,m4 => vl = 16 lanes
 * - Uses group-aligned registers (base must be multiple of 4)
 * - 4 vector-groups per iteration => 4*vl elements per iter
 */
static inline void vexp_optimized_m4_fp16(const __fp16* inp, __fp16* out, int N) {
  const __fp16 *pin  = inp;
  __fp16       *pout = out;

  unsigned long vl;
  // Fixed VTYPE: e16, m4
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
    // outputs also group-aligned: 16,20,24,28
    asm volatile("vfexpf.v v16, v0");
    asm volatile("vfexpf.v v20, v4");
    asm volatile("vfexpf.v v24, v8");
    asm volatile("vfexpf.v v28, v12");

    // --- STEP 3: STORE PHASE ---
    asm volatile("vse16.v v16, (%0)" :: "r"(pout)            : "memory");
    asm volatile("vse16.v v20, (%0)" :: "r"(pout +      vl)  : "memory");
    asm volatile("vse16.v v24, (%0)" :: "r"(pout + 2ul*vl)   : "memory");
    asm volatile("vse16.v v28, (%0)" :: "r"(pout + 3ul*vl)   : "memory");

    pin  += 4ul * vl;
    pout += 4ul * vl;
  }
}

/* LMUL=2 "clean burst" variant using vfexpf.v:
 * - Assumes VLEN = 512 bits, e16,m2 => vl = 16 lanes
 * - Uses group-aligned registers (base must be even)
 * - 4 vector-groups per iteration => 4*vl elements per iter
 */
static inline void vexp_optimized_m2_fp16(const __fp16* inp, __fp16* out, int N) {
  const __fp16 *pin  = inp;
  __fp16       *pout = out;

  unsigned long vl;
  // Fixed VTYPE: e16, m2
  asm volatile("vsetvli %0, zero, e16, m2, ta, ma" : "=r"(vl) :: "memory");

  // 4 vector-groups per iteration => 4*vl elements/iter
  const int iters = N / (int)(4ul * vl);

  for (int i = 0; i < iters; i++) {
    // --- STEP 1: LOAD PHASE ---
    // group-aligned bases for m2: 0,2,4,6,8,10,12,14,...
    asm volatile("vle16.v v0,  (%0)" :: "r"(pin)            : "memory");
    asm volatile("vle16.v v2,  (%0)" :: "r"(pin +      vl)  : "memory");
    asm volatile("vle16.v v4,  (%0)" :: "r"(pin + 2ul*vl)   : "memory");
    asm volatile("vle16.v v6,  (%0)" :: "r"(pin + 3ul*vl)   : "memory");

    asm volatile("" ::: "memory");

    // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
    asm volatile("vfexpf.v v8,  v0");
    asm volatile("vfexpf.v v10, v2");
    asm volatile("vfexpf.v v12, v4");
    asm volatile("vfexpf.v v14, v6");

    // --- STEP 3: STORE PHASE ---
    asm volatile("vse16.v v8,  (%0)" :: "r"(pout)            : "memory");
    asm volatile("vse16.v v10, (%0)" :: "r"(pout +      vl)  : "memory");
    asm volatile("vse16.v v12, (%0)" :: "r"(pout + 2ul*vl)   : "memory");
    asm volatile("vse16.v v14, (%0)" :: "r"(pout + 3ul*vl)   : "memory");

    pin  += 4ul * vl;
    pout += 4ul * vl;
  }
}

/* ------------------- Kernel selector ------------------- */
static inline void vexp_strip_fp16(const __fp16* inp, __fp16* out, int N) {
#if   (LMUL_MODE == 8)
    vexp_m8_strip_fp16(inp, out, N);
#elif (LMUL_MODE == 4)
    vexp_m4_strip_fp16(inp, out, N);
#elif (LMUL_MODE == 2)
    vexp_m2_strip_fp16(inp, out, N);
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

    static __fp16 *g_in  = NULL;
    static __fp16 *g_out = NULL;

    if (cid == 0) {
        g_in  = (__fp16*)snrt_l1alloc(N * sizeof(__fp16));
        g_out = (__fp16*)snrt_l1alloc(N * sizeof(__fp16));
        if (!g_in || !g_out) { printf("alloc failed\n"); return 1; }

        /* Load input from DRAM once, publish to other core */
        snrt_dma_start_1d(g_in, data1_dram, N * sizeof(__fp16));
        snrt_dma_wait_all();
        memset(g_out, 0, N * sizeof(__fp16));
    }
    snrt_cluster_hw_barrier();  /* publish g_in/g_out */

    /* Two-core split */
    const int mid   = N >> 1;
    const int start = (cid == 0) ? 0   : mid;
    const int count = (cid == 0) ? mid : (N - mid);

    if (count > 0) {
        start_kernel();
        unsigned t0 = benchmark_get_cycle();

        vexp_m8_strip_fp16(g_in + start, g_out + start, count);

        unsigned cycles = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[exp_fp16 LMUL=%d] core %u cycles: %u\n",
               LMUL_MODE, cid, cycles);
    }

    snrt_cluster_hw_barrier();

    /* (Optional) golden check */
    if (cid == 0) {
       printf("CHECK RESULTS (exp_fp16)\n");
       check_result_fp16(g_in, g_out, (__fp16*)outE, N);
    }

    snrt_cluster_hw_barrier();
    return 0;
}