// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Embedding-bag / neighbour aggregation followed by an MLP layer (DLRM, or GCN layer), FP16 inputs and
// FP32 output:
//   pooled[b][d] = scale * sum_l table[idx[b][l]][d]     (Spatz, FP16: linear, no widening needed;
//                                                         scalar row address + unit-stride row loads,
//                                                         no indexed vector ops)
// The GeMM wants N16-tiled packed activations. The table rows are stored
// offline as [even features | odd features], so two strided 16-bit stores per
// bag form Bpack[n_tile][K/2][16] directly, with no transpose or repack pass.

#include "runtime.h"

#include DATAHEADER

uint16_t *table;
uint32_t *idx;
uint32_t *pool;  // packed W operand [D/2][B]
uint32_t *w1;    // packed X^T operand [D/2][Dout]
float *z;
#ifndef APP_FUSED
#define APP_FUSED 1
#endif

#define PG_GEMM_ATTRIBUTES static __attribute__((always_inline)) inline
#include "kernel/gemm.c"

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    L1_ALLOC(table, uint16_t, R_SIZE * D_SIZE);
    L1_ALLOC(idx, uint32_t, VALID_B_SIZE * (L_SIZE / 4));
    L1_ALLOC(pool, uint32_t, (D_SIZE / 2) * B_SIZE);
    L1_ALLOC(w1, uint32_t, (D_SIZE / 2) * DOUT_SIZE);
    L1_ALLOC(z, float, DOUT_SIZE * B_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(table, gm_table_dram, R_SIZE * D_SIZE * sizeof(uint16_t));
    snrt_dma_start_1d(idx, gm_idx_dram,
                      VALID_B_SIZE * (L_SIZE / 4) * sizeof(uint32_t));
    snrt_dma_start_1d(w1, gm_w1_dram, (D_SIZE / 2) * DOUT_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(z, gm_zinit_dram, DOUT_SIZE * B_SIZE * sizeof(float));
    snrt_dma_wait_all();
    // O-POPE also clears its dummy column before the measured region.
    snrt_memset(pool, 0, (D_SIZE / 2) * B_SIZE * sizeof(uint32_t));
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    run_gather_pipeline(APP_FUSED);
    if (!APP_FUSED) {
      asm volatile("vsetvli zero,%0,e32,m8,ta,ma"::"r"((uintptr_t)128):"memory");
      for(unsigned i=0;i<DOUT_SIZE*B_SIZE;i+=128)
        asm volatile("vle32.v v0,(%0)\n" RELU_ASM("v0") "vse32.v v0,(%0)"
          ::"r"(z+i),RELU_INPUTS:"v0","v1","v2","v3","v4","v5","v6","v7","memory");
      asm volatile("lw zero,0(%0)"::"r"(z):"memory");
    }

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- VME widening %s: %d bags x %d lookups, D=%d -> MLP %dx%dx%d -----\n",
           MODE_GCN ? "GCN layer" : "DLRM embedding-bag + MLP", VALID_B_SIZE, L_SIZE, D_SIZE, DOUT_SIZE,
           D_SIZE, VALID_B_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);

    for (unsigned o = 0; o < DOUT_SIZE; ++o)
      errors += transformer_check_fp32(z + o * B_SIZE,
                               (const float *)gm_golden + o * VALID_B_SIZE,
                               VALID_B_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
