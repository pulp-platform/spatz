/*
 * Copyright (C) 2021 ETH Zurich and University of Bologna
 * Apache-2.0 license
 * -----------------------------------------------------------------------
 *
 * RVV rsqrtf (SEW=32) — strip-mined, maskless, runtime-selectable LMUL (8/4/2).
 *
 * Algorithm
 *   1) Quake-style bit seed:
 *        i' = RSQRT_MAGIC - (as_int(x) >> 1)
 *        y0 = as_float(i')
 *   2) NR refinements for y ≈ 1/sqrt(x):
 *        y_{k+1} = y_k * (1.5 - 0.5 * x * y_k^2)
 *
 * Notes
 *   - Domain: x > 0 (no masking/branching for special cases).
 *   - NR_ITERS: default 3 (great accuracy). 2 is faster and still good.
 *   - Two-core split: core 0 → [0, N/2), core 1 → [N/2, N).
 *   - Select LMUL at compile time: -DLMUL_MODE=8|4|2
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
#define LMUL_MODE 8         /* 8, 4, or 2 */
#endif

#ifndef NR_ITERS
#define NR_ITERS 0         /* Newton refinements (2 or 3) */
#endif

/* Classic Quake magic; try 0x5F375A86u for a slightly different bias. */
#ifndef RSQRT_MAGIC
#define RSQRT_MAGIC 0x5F3759DFu
#define HALF        0.5f
#define ONE_POINT_FIVE 1.5f
#endif

#ifndef THRESHOLD
#define THRESHOLD 0.00010f
#endif

/* ========================= LMUL = 8 (3 regs used) =========================
 * Reg plan (SEW=32, LMUL=8):
 *   v8  : x (float)
 *   v16 : y (float)   [seed -> refined result]
 *   v24 : tmp (int/float)  [xi / y^2 / intermediates]
 *
 * NR step (no v0 usage, all scalars via constraints):
 *   t  = y*y           -> v24
 *   t  = t * x         -> v24
 *   t  = t * (-0.5f)   -> v24
 *   t  = t + 1.5f      -> v24
 *   y  = y * t         -> v16
 */
static inline void vrsqrt_m8_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        /* Load x -> v8 ; xi into v24 */
        asm volatile("vle32.v v8, (%0)" :: "r"(pin) : "memory");
        asm volatile("vmv.v.v  v24, v8");                             /* xi (as bits) */

        /* Seed: i' = MAGIC - (xi >> 1) -> y in v16 */
        asm volatile("vsrl.vi   v16, v24, 1");                        /* v16 = xi>>1 (int) */
        asm volatile("vmv.v.x   v24, %0" :: "r"(RSQRT_MAGIC));        /* v24 = MAGIC (int) */
        asm volatile("vsub.vv   v16, v24, v16");                      /* v16 = MAGIC - (xi>>1) */
        

        /* 2. THE NEWTON RAPHSON STEP */
        asm volatile("vfmul.vv  v24, v16, v16");                      /* v24 = y * y */
        asm volatile("vfmul.vv  v24, v24, v8");                       /* v24 = (y * y) * x */

        /* The Magic Instruction: vfnmsac (Fused Negative Multiply-Accumulate)
        Operation: vd = -(f[rs1] * vs2) + vd 
        We want:   v24 = -(0.5 * v24) + 1.5
        */
        asm volatile("vfmv.v.f  v20, %[c1]":: [c1]"f"(ONE_POINT_FIVE));                        /* Splat 1.5 into v20 */
        asm volatile("vfnmsac.vf v20, %[c05], v24" :: [c05]"f"(HALF));                  /* v20 = -(0.5 * v24) + 1.5 */
        asm volatile("vfmul.vv  v16, v16, v20");                      /* y = y * v20 */

        /* Newton refinements */
#if (NR_ITERS >= 1)
        asm volatile("vfmul.vv  v24, v16, v16");                      /* t = y*y */
        asm volatile("vfmul.vv  v24, v24, v8");                       /* t = t*x */
        asm volatile("vfmul.vf  v24, v24, %[MH]" :: [MH] "f"(-0.5f)); /* t *= -0.5 */
        asm volatile("vfadd.vf  v24, v24, %[A]"  :: [A]  "f"(1.5f));  /* t += 1.5 */
        asm volatile("vfmul.vv  v16, v16, v24");                      /* y *= t */
#endif
#if (NR_ITERS >= 2)
        asm volatile("vfmul.vv  v24, v16, v16");
        asm volatile("vfmul.vv  v24, v24, v8");
        asm volatile("vfmul.vf  v24, v24, %[MH]" :: [MH] "f"(-0.5f));
        asm volatile("vfadd.vf  v24, v24, %[A]"  :: [A]  "f"(1.5f));
        asm volatile("vfmul.vv  v16, v16, v24");
#endif
#if (NR_ITERS >= 3)
        asm volatile("vfmul.vv  v24, v16, v16");
        asm volatile("vfmul.vv  v24, v24, v8");
        asm volatile("vfmul.vf  v24, v24, %[MH]" :: [MH] "f"(-0.5f));
        asm volatile("vfadd.vf  v24, v24, %[A]"  :: [A]  "f"(1.5f));
        asm volatile("vfmul.vv  v16, v16, v24");
#endif

        /* Store */
        asm volatile("vse32.v v16, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}
#include <stdint.h>

#ifndef RSQRT_MAGIC
#define RSQRT_MAGIC 0x5f375a86u
#define HALF           0.5f
#define ONE_POINT_FIVE 1.5f
#endif

/**
 * RISC-V Vector Optimized Fast Inverse Square Root
 * Processes N elements using the Quake-style bit-hack + 1 Newton-Raphson iteration.
 */
void vrsqrt_sw(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        
        // 1. Set configuration: 32-bit elements, LMUL=8
        // This treats registers in groups of 8 (v8-v15, v16-v23, etc.)
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        // 2. Load input 'x' into v8 (effectively v8-v15)
        asm volatile("vle32.v v8, (%0)" :: "r"(pin) : "memory");

        // --- SEED PHASE ---
        // Treat bits as integers: y = MAGIC - (x >> 1)
        asm volatile("vsrl.vi   v16, v8, 1");          // v16 = x >> 1
        asm volatile("li        t0, %0" :: "i"(RSQRT_MAGIC));
        asm volatile("vrsub.vx  v16, v16, t0");        // v16 = MAGIC - (x >> 1)

        // --- NEWTON-RAPHSON PHASE ---
        // Formula: y = y * (1.5 - (0.5 * x * y * y))
        
        // v24 = x * y^2
        asm volatile("vfmul.vv   v24, v16, v16");      // v24 = y * y
        asm volatile("vfmul.vv   v24, v24, v8");       // v24 = (y * y) * x

        // v0 = 1.5 - (0.5 * v24)
        asm volatile("vfmv.v.f   v0, %[c1]" :: [c1]"f"(ONE_POINT_FIVE)); 
        // vfnmsac: vd = -(f[rs1] * vs2) + vd
        asm volatile("vfnmsac.vf v0, %[c05], v24" :: [c05]"f"(HALF));

        // Final y = y * v0
        asm volatile("vfmul.vv   v16, v16, v0");

        // 3. Store the result 'y'
        asm volatile("vse32.v v16, (%0)" :: "r"(pout) : "memory");

        // 4. Update pointers and loop counter
        pin       += vl;
        pout      += vl;
        remaining -= (int)vl;
    }
}
void vrsqrt_optimized(const float* inp, float* out,  int N) {
    const float *pin = inp;
    float *pout = out;
    int remaining = N;
    // N = 2048
    
    unsigned long vl;
    // Configure VTYPE for max length (16 elements per group)


    // Loop 32 times to process 2048 elements
   while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");
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
        asm volatile("vfrsqrt7.v v0,  v0");
        asm volatile("vfrsqrt7.v v8,  v8");
        asm volatile("vfrsqrt7.v v16,  v16");
        asm volatile("vfrsqrt7.v v24,  v24");

        // --- STEP 3: STORE PHASE (Drain) ---
        asm volatile("vse32.v v0,  (%0)" :: "r"(pout)        : "memory");
        asm volatile("vse32.v v8,  (%0)" :: "r"(pout + vl)   : "memory");
        asm volatile("vse32.v v16, (%0)" :: "r"(pout + vl*2) : "memory");
        asm volatile("vse32.v v24, (%0)" :: "r"(pout + vl*3) : "memory");

        // Move pointers for the next batch of 512
        pin  += (vl * 4);
        pout += (vl * 4);
        remaining -= (int)(vl * 4);
    }
}
/* ========================= LMUL = 4 (baseline) ========================= */
static inline void vrsqrt_m4_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        /* x -> v12, xi in v24 */
        asm volatile("vle32.v v12, (%0)" :: "r"(pin) : "memory");
        asm volatile("vmv.v.v  v24, v12");

        /* Seed: y in v16 */
        asm volatile("vmv.v.v   v28, v24");
        asm volatile("vsrl.vi   v28, v28, 1");                        /* v28 = xi>>1 */
        asm volatile("vmv.v.x   v24, %0" :: "r"(RSQRT_MAGIC));
        asm volatile("vsub.vv   v16, v24, v28");                      /* v16 = MAGIC - (xi>>1) */

        /* Newton refinements */
#if (NR_ITERS >= 1)
        asm volatile("vfmul.vv  v20, v16, v16");                      /* t = y*y */
        asm volatile("vfmul.vv  v20, v20, v12");                      /* t = t*x */
        asm volatile("vfmul.vf  v20, v20, %[MH]" :: [MH] "f"(-0.5f)); /* t *= -0.5 */
        asm volatile("vfadd.vf  v20, v20, %[A]"  :: [A]  "f"(1.5f));  /* t += 1.5 */
        asm volatile("vfmul.vv  v16, v16, v20");                      /* y *= t */
#endif
#if (NR_ITERS >= 2)
        asm volatile("vfmul.vv  v20, v16, v16");
        asm volatile("vfmul.vv  v20, v20, v12");
        asm volatile("vfmul.vf  v20, v20, %[MH]" :: [MH] "f"(-0.5f));
        asm volatile("vfadd.vf  v20, v20, %[A]"  :: [A]  "f"(1.5f));
        asm volatile("vfmul.vv  v16, v16, v20");
#endif
#if (NR_ITERS >= 3)
        asm volatile("vfmul.vv  v20, v16, v16");
        asm volatile("vfmul.vv  v20, v20, v12");
        asm volatile("vfmul.vf  v20, v20, %[MH]" :: [MH] "f"(-0.5f));
        asm volatile("vfadd.vf  v20, v20, %[A]"  :: [A]  "f"(1.5f));
        asm volatile("vfmul.vv  v16, v16, v20");
#endif

        asm volatile("vse32.v v16, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ========================= LMUL = 2 (more regs) ========================= */
static inline void vrsqrt_m2_strip(const float* inp, float* out, int N) {
    const float *pin  = inp;
    float       *pout = out;
    int remaining = N;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        /* x -> v4 ; xi -> v22 */
        asm volatile("vle32.v v4, (%0)" :: "r"(pin) : "memory");
        asm volatile("vmv.v.v  v22, v4");

        /* Seed -> v6 */
        asm volatile("vmv.v.v   v26, v22");
        asm volatile("vsrl.vi   v26, v26, 1");                        /* xi>>1 */
        asm volatile("vmv.v.x   v24, %0" :: "r"(RSQRT_MAGIC));
        asm volatile("vsub.vv   v6,  v24, v26");                      /* y0 */

        /* Newton refinements */
#if (NR_ITERS >= 1)
        asm volatile("vfmul.vv  v8,  v6,  v6");                       /* t = y*y */
        asm volatile("vfmul.vv  v8,  v8,  v4");                       /* t = t*x */
        asm volatile("vfmul.vf  v8,  v8,  %[MH]" :: [MH] "f"(-0.5f)); /* t *= -0.5 */
        asm volatile("vfadd.vf  v8,  v8,  %[A]"  :: [A]  "f"(1.5f));  /* t += 1.5 */
        asm volatile("vfmul.vv  v6,  v6,  v8");                       /* y *= t */
#endif
#if (NR_ITERS >= 2)
        asm volatile("vfmul.vv  v8,  v6,  v6");
        asm volatile("vfmul.vv  v8,  v8,  v4");
        asm volatile("vfmul.vf  v8,  v8,  %[MH]" :: [MH] "f"(-0.5f));
        asm volatile("vfadd.vf  v8,  v8,  %[A]"  :: [A]  "f"(1.5f));
        asm volatile("vfmul.vv  v6,  v6,  v8");
#endif
#if (NR_ITERS >= 3)
        asm volatile("vfmul.vv  v8,  v6,  v6");
        asm volatile("vfmul.vv  v8,  v8,  v4");
        asm volatile("vfmul.vf  v8,  v8,  %[MH]" :: [MH] "f"(-0.5f));
        asm volatile("vfadd.vf  v8,  v8,  %[A]"  :: [A]  "f"(1.5f));
        asm volatile("vfmul.vv  v6,  v6,  v8");
#endif

        asm volatile("vse32.v v6, (%0)" :: "r"(pout) : "memory");

        pin  += vl;
        pout += vl;
        remaining -= (int)vl;
    }
}

/* ------------------ Golden checker (same style) ------------------ */
static void check_result(const float *input, const float *x, const float *ref, int r) {
    int err = 0;
    for (int i = 0; i < r; i++) {
        float diff = fabsf(x[i] - ref[i]);
        printf("At index %d:\t, value %f\t expected %f\t real %f\t error %f\n",
                i, input[i], ref[i], x[i], diff);
        
    }
}

/* ------------------- Kernel selector ------------------- */
static inline void vrsqrt_strip(const float* inp, float* out, int N) {
#if   (LMUL_MODE == 8)
    vrsqrt_m8_strip(inp, out, N);
#elif (LMUL_MODE == 4)
    vrsqrt_m4_strip(inp, out, N);
#elif (LMUL_MODE == 2)
    vrsqrt_m2_strip(inp, out, N);
#else
#   error "LMUL_MODE must be 8, 4, or 2"
#endif
}

/* ----------------------------- Main ----------------------------- */
/* Two-core split with collision-free L1 pointers. */
int main(void) {
    const unsigned int cid   = snrt_cluster_core_idx();
    const unsigned int cores = snrt_cluster_core_num();
    snrt_cluster_hw_barrier();

    const int B = B_Size, C = C_Size, T = T_Size;
    const int N = B * T * C;

    static float *g_in  = NULL;
    static float *g_out = NULL;

    if (cid == 0) {
        g_in  = (float*)snrt_l1alloc(N * sizeof(float));
        g_out = (float*)snrt_l1alloc(N * sizeof(float));
        if (!g_in || !g_out) { printf("alloc failed\n"); return 1; }

        snrt_dma_start_1d(g_in, data1_dram, N * sizeof(float));
        snrt_dma_wait_all();
        memset(g_out, 0, N * sizeof(float));
    }
    snrt_cluster_hw_barrier();  /* publish g_in/g_out */

    /* Split work: core 0 → [0..N/2), core 1 → [N/2..N) */
    const int mid   = N >> 1;
    const int start = (cid == 0) ? 0   : mid;
    const int count = (cid == 0) ? mid : (N - mid);

    /* Time on core 0 for the whole parallel region */
    unsigned t0 = 0;
    if (cid == 0) {
        start_kernel();
        t0 = benchmark_get_cycle();
    }
    snrt_cluster_hw_barrier();

    if (count > 0) {
        vrsqrt_optimized(g_in + start, g_out + start, count);
    }

    snrt_cluster_hw_barrier();

    if (cid == 0) {
        unsigned cycles = benchmark_get_cycle() - t0;
        stop_kernel();

        printf("[LMUL=8] Parallel rsqrt cycles: %u (cores=%u)\n", cycles, cores);
        printf("CHECK RESULTS (rsqrt)\n");
        /* If your golden is named differently (e.g., outRSQ), change here. */
        check_result(g_in, g_out, outR, N);
    }

    snrt_cluster_hw_barrier();
    return 0;
}
