// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

/*
 * RVV cosh (SEW=16) — FP16 Schraudolph + hardware vfcosh.v
 * Strip-mined and optimized variants with dual LMUL support (8/4/2).
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


// FP16 Schraudolph constants (adjusted for 16-bit precision)
// cosh(x) = 0.5 * (exp(x) + exp(-x))
// Using separate Schraudolph for pos and neg
#define SCH_C_FP16 1477.32f        // 2^10 / ln(2) — correct FP16 Schraudolph constant  
#define SCH_B_FP16 15360.0f        // Bias for Schraudolph approximation (15 << 10) 14291.8

// Strip-mined Schraudolph-based cosh kernels (m8, m4, m2)
static inline void vcosh_m8_strip_fp16(const __fp16* inp, __fp16* out, int N) {
    int remaining = N;
    while (remaining > 0) {
        int vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle16.v v0, (%0)" :: "r"(inp) : "memory");
        
        // Compute exp(x) using Schraudolph
        asm volatile("vfmv.v.f v16, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf v16, %[C], v0" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v16, v16");  // Integer bits ARE exp(x) as FP16
        
        // Compute exp(-x) using Schraudolph
        asm volatile("vfneg.v v0, v0");
        asm volatile("vfmv.v.f v24, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf v24, %[C], v0" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");  // Integer bits ARE exp(-x) as FP16
        
        // cosh(x) = 0.5 * (exp(x) + exp(-x))
        asm volatile("vfadd.vv v16, v16, v24");
        asm volatile("vfmul.vf v16, v16, %[half]" :: [half]"f"(0.5f));
        
        asm volatile("vse16.v v16, (%0)" :: "r"(out) : "memory");
        
        int adv = vl;
        inp += adv;
        out += adv;
        remaining -= adv;
    }
}

static inline void vcosh_m4_strip_fp16(const __fp16* inp, __fp16* out, int N) {
    int remaining = N;
    while (remaining > 0) {
        int vl;
        asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle16.v v12, (%0)" :: "r"(inp) : "memory");
        
        asm volatile("vfmv.v.f v16, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf v16, %[C], v12" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v16, v16");  // exp(x) bits
        
        asm volatile("vfneg.v v12, v12");
        asm volatile("vfmv.v.f v20, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf v20, %[C], v12" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v20, v20");  // exp(-x) bits
        
        asm volatile("vfadd.vv v16, v16, v20");
        asm volatile("vfmul.vf v16, v16, %[half]" :: [half]"f"(0.5f));
        
        asm volatile("vse16.v v16, (%0)" :: "r"(out) : "memory");
        
        int adv = vl;
        inp += adv;
        out += adv;
        remaining -= adv;
    }
}

static inline void vcosh_m2_strip_fp16(const __fp16* inp, __fp16* out, int N) {
    int remaining = N;
    while (remaining > 0) {
        int vl;
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(remaining));
        asm volatile("vle16.v v4, (%0)" :: "r"(inp) : "memory");
        
        asm volatile("vfmv.v.f v6, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf v6, %[C], v4" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v6, v6");  // exp(x) bits
        
        asm volatile("vfneg.v v4, v4");
        asm volatile("vfmv.v.f v8, %[B]" :: [B]"f"(SCH_B_FP16));
        asm volatile("vfmacc.vf v8, %[C], v4" :: [C]"f"(SCH_C_FP16));
        asm volatile("vfcvt.rtz.xu.f.v v8, v8");  // exp(-x) bits
        
        asm volatile("vfadd.vv v6, v6, v8");
        asm volatile("vfmul.vf v6, v6, %[half]" :: [half]"f"(0.5f));
        
        asm volatile("vse16.v v6, (%0)" :: "r"(out) : "memory");
        
        int adv = vl;
        inp += adv;
        out += adv;
        remaining -= adv;
    }
}

static inline void vcosh_strip_fp16(const __fp16* inp, __fp16* out, int N) {
#if LMUL_MODE == 8
    vcosh_m8_strip_fp16(inp, out, N);
#elif LMUL_MODE == 4
    vcosh_m4_strip_fp16(inp, out, N);
#elif LMUL_MODE == 2
    vcosh_m2_strip_fp16(inp, out, N);
#else
#error "LMUL_MODE must be 2, 4, or 8"
#endif
}

// Optimized hardware-accelerated cosh kernels using vfcosh.v
static inline void vcosh_optimized_fp16(const __fp16* inp, __fp16* out, int N) {
    int iters = N / (4 * 32);  // 4 vectors × vl=32 per iteration
    for (int i = 0; i < iters; i++) {
        asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"(32));
        
        // Load 4 vector groups
        asm volatile("vle16.v v0, (%0)" :: "r"(inp) : "memory");
        inp += 32;
        asm volatile("vle16.v v8, (%0)" :: "r"(inp) : "memory");
        inp += 32;
        asm volatile("vle16.v v16, (%0)" :: "r"(inp) : "memory");
        inp += 32;
        asm volatile("vle16.v v24, (%0)" :: "r"(inp) : "memory");
        inp += 32;
        
        // Execute hardware cosh on each group
        asm volatile("vfcoshf.v v4, v0");
        asm volatile("vfcoshf.v v5, v8");
        asm volatile("vfcoshf.v v6, v16");
        asm volatile("vfcoshf.v v7, v24");
        
        // Store results
        asm volatile("vse16.v v4, (%0)" :: "r"(out) : "memory");
        out += 32;
        asm volatile("vse16.v v5, (%0)" :: "r"(out) : "memory");
        out += 32;
        asm volatile("vse16.v v6, (%0)" :: "r"(out) : "memory");
        out += 32;
        asm volatile("vse16.v v7, (%0)" :: "r"(out) : "memory");
        out += 32;
    }
}

static inline void vcosh_optimized_m8_fp16(const __fp16* inp, __fp16* out, int N) {
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
        // Use custom vfcoshf.v instruction (FP16 hardware exponential)
        asm volatile("vfcoshf.v v0, v0");
        asm volatile("vfcoshf.v v8, v8");
        asm volatile("vfcoshf.v v16, v16");
        asm volatile("vfcoshf.v v24, v24");

        // --- STEP 3: STORE PHASE (Drain) ---
        asm volatile("vse16.v v0, (%0)" :: "r"(pout)           : "memory");
        asm volatile("vse16.v v8, (%0)" :: "r"(pout +      vl) : "memory");
        asm volatile("vse16.v v16, (%0)" :: "r"(pout + 2ul*vl)  : "memory");
        asm volatile("vse16.v v24, (%0)" :: "r"(pout + 3ul*vl)  : "memory");

        pin  += 4ul * vl;
        pout += 4ul * vl;
  }
}

static inline void vcosh_optimized_m2_fp16(const __fp16* inp, __fp16* out, int N) {
    int iters = N / (4 * 16);  // 4 groups × vl=16
    for (int i = 0; i < iters; i++) {
        asm volatile("vsetvli zero, %0, e16, m2, ta, ma" :: "r"(16));
        
        asm volatile("vle16.v v0, (%0)" :: "r"(inp) : "memory");
        inp += 16;
        asm volatile("vle16.v v2, (%0)" :: "r"(inp) : "memory");
        inp += 16;
        asm volatile("vle16.v v4, (%0)" :: "r"(inp) : "memory");
        inp += 16;
        asm volatile("vle16.v v6, (%0)" :: "r"(inp) : "memory");
        inp += 16;
        
        asm volatile("vfcoshf.v v8, v0");
        asm volatile("vfcoshf.v v10, v2");
        asm volatile("vfcoshf.v v12, v4");
        asm volatile("vfcoshf.v v14, v6");
        
        asm volatile("vse16.v v8, (%0)" :: "r"(out) : "memory");
        out += 16;
        asm volatile("vse16.v v10, (%0)" :: "r"(out) : "memory");
        out += 16;
        asm volatile("vse16.v v12, (%0)" :: "r"(out) : "memory");
        out += 16;
        asm volatile("vse16.v v14, (%0)" :: "r"(out) : "memory");
        out += 16;
    }
}

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

int main() {
    unsigned int cid = snrt_cluster_core_idx();
    unsigned int cnum = snrt_cluster_core_num();
    const int B = B_Size, C = C_Size, T = T_Size;
    const int N = B * T * C;
    __fp16 *g_in, *g_out;
    
    if (cid == 0) {
        // L1 buffer allocation
        g_in = snrt_l1alloc(N * sizeof(__fp16));
        g_out = snrt_l1alloc(N * sizeof(__fp16));
        
        // Load from DRAM
        snrt_dma_start_1d(g_in, (void*)data1_dram, N * sizeof(__fp16));
        snrt_dma_wait_all();
        memset(g_out, 0, N * sizeof(__fp16));
    }
    snrt_cluster_hw_barrier();
    
    // Two-core split
    int start = (cid * N) / cnum;
    int end = ((cid + 1) * N) / cnum;
    int count = end - start;
    
    unsigned int t0 = benchmark_get_cycle();
    start_kernel();
    vcosh_optimized_m8_fp16(g_in + start, g_out + start, count);
    unsigned int cycles = benchmark_get_cycle() - t0;
    stop_kernel();
    
    snrt_cluster_hw_barrier();
    
    if (cid == 0) {
        printf("cosh cycles: %u\n", cycles);
        check_result_fp16(g_in, g_out, outE, N);
    }
    
    return 0;
}
