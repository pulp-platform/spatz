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

static __fp16 *g_in  = NULL;
static __fp16 *g_out = NULL;


static inline void vrecip_sw(const __fp16* inp, __fp16* out, int N) {
    const __fp16 *pin  = inp;
    __fp16       *pout = out;
    int remaining = N;
    
    // Setup constants in scalar registers before the loop
    // FP16 reciprocal magic: adapted from FP32 0x7EFFFFFF for 16-bit format
    uint32_t magic = 0x7BFFu;
    __fp16 two = 2.0f;

    while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");

        // 1. Load input 'a'
        asm volatile("vle16.v v8, (%0)" :: "r"(pin) : "memory");

        // 2. SEED: x = magic - bits(a)
        asm volatile("vrsub.vx v16, v8, %0" :: "r"(magic));

        // 3. NEWTON-RAPHSON STEP 1
        // v16 = v16 * (2.0 - v8 * v16)
        
        asm volatile("vfmv.v.f  v24, %0" :: "f"(two));  // Splat 2.0 properly as FP16
        
        // --- NR Step 1 ---
        asm volatile("vfnmsac.vv v24, v8, v16"); // v24 = 2.0 - (a * x)
        asm volatile("vfmul.vv   v16, v16, v24"); // x = x * bracket

        // --- NR Step 2 ---
        asm volatile("vfmv.v.f   v24, %0" :: "f"(two));  // Reset v24 to 2.0
        asm volatile("vfnmsac.vv v24, v8, v16"); // v24 = 2.0 - (a * x_new)
        asm volatile("vfmul.vv   v16, v16, v24"); // x = x_new * bracket

        // 4. Store and Update
        asm volatile("vse16.v v16, (%0)" :: "r"(pout) : "memory");

        pin       += vl;
        pout      += vl;
        remaining -= (int)vl;
    }
}
void vrec_hw(const __fp16* inp, __fp16* out,  int N) {
    const __fp16 *pin = inp;
    __fp16 *pout = out;
    int remaining = N;
    // N = 2048
    
    unsigned long vl;
    // Configure VTYPE for max length (16 elements per group)


    // Loop 32 times to process 2048 elements
   while (remaining > 0) {
        unsigned long vl;
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma"
                     : "=r"(vl) : "r"(remaining) : "memory");
        // --- STEP 1: LOAD PHASE (Fill the Register File) ---
        asm volatile("vle16.v v0,  (%0)" :: "r"(pin)         : "memory");
        asm volatile("vle16.v v8,  (%0)" :: "r"(pin + vl)    : "memory");
        asm volatile("vle16.v v16, (%0)" :: "r"(pin + vl*2)  : "memory");
        asm volatile("vle16.v v24, (%0)" :: "r"(pin + vl*3)  : "memory");

        // Memory Barrier (
        asm volatile("" ::: "memory"); 

        // --- STEP 2: EXECUTION PHASE (Clean Burst) ---
        // Operands in v0-v31 are fully resident in VRF.
        // There are NO dependencies between these instructions.
        // In the waveform, you will see these issue back-to-back.
        asm volatile("vfrec7.v v0,  v0");
        asm volatile("vfrec7.v v8,  v8");
        asm volatile("vfrec7.v v16,  v16");
        asm volatile("vfrec7.v v24,  v24");

        // --- STEP 3: STORE PHASE (Drain) ---
        asm volatile("vse16.v v0,  (%0)" :: "r"(pout)        : "memory");
        asm volatile("vse16.v v8,  (%0)" :: "r"(pout + vl)   : "memory");
        asm volatile("vse16.v v16, (%0)" :: "r"(pout + vl*2) : "memory");
        asm volatile("vse16.v v24, (%0)" :: "r"(pout + vl*3) : "memory");

        // Move pointers for the next batch of 512
        pin  += (vl * 4);
        pout += (vl * 4);
        remaining -= (int)(vl * 4);
    }
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

        vrec_hw(g_in + start, g_out + start, count);

        unsigned cycles = benchmark_get_cycle() - t0;
        stop_kernel();
        printf("[rec core %u cycles: %u\n", cid, cycles);
    }

    snrt_cluster_hw_barrier();

    // Validate on core 0
    if (cid == 0) {
        printf("CHECK RESULTS (rec)\n");
        check_result_fp16(g_in, g_out, outE, N);
    }

    snrt_cluster_hw_barrier();
    return 0;
}
