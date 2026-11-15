// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Gather-MLP on VME (Spatz VFU + OPE, FP16 -> FP32): DLRM embedding-bag (MODE_GCN = 0) or GCN layer (MODE_GCN = 1):
// gather + sum pooling (FP16, O-POPE order) -> widening GeMM + bias -> ReLU, fused in one kernel (the GeMM of a
// bag tile runs while the next bag tile is gathered; schedule: ../../script/tf/gathermlp.py). Derived from
// baseline_spatz/sw/opopeBenchmarks/widening-hp-gathermlp/main.c: same input values (table, index words, weights),
// inputs and constant operand rows placed outside the measured region, same check after it (z row pitch BPAD).

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/gathermlp.c"

uint16_t *table;
uint32_t *idx;
uint32_t *pool;  // [BPAD/32][KP][32] packed pooled rows (K-pair D/2 = {1, 1}: bias)
uint32_t *w;     // W1 (+ bias K-pair) operand stream
#if GM_V2
uint32_t *bv;    // bias vectors of the GeMM bias K-pair
#endif
float *z;        // [Dout][BPAD]

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(table, uint16_t, R_SIZE * D_SIZE);
    OPOPE_L1_ALLOC(idx, uint32_t, B_SIZE * (L_SIZE / 4));
    OPOPE_L1_ALLOC(pool, uint32_t, POOL_WORDS);
#if GM_V2
    OPOPE_L1_ALLOC(bv, uint32_t, sizeof(gm_bv) / 4);
#endif
    OPOPE_L1_ALLOC(w, uint32_t, sizeof(gm_w) / 4);
    OPOPE_L1_ALLOC(z, float, DOUT_SIZE * BPAD);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs and constant operand rows, outside of the measured region)
  if (cid == 0) {
    snrt_dma_start_1d(table, gm_table, sizeof(gm_table));
    snrt_dma_start_1d(idx, gm_idx, sizeof(gm_idx));
    snrt_dma_start_1d(w, gm_w, sizeof(gm_w));
#if GM_V2
    snrt_dma_start_1d(bv, gm_bv, sizeof(gm_bv));
#endif
    snrt_dma_wait_all();
    for (unsigned int i = 0; i < POOL_WORDS; i++) pool[i] = 0;          // padding bag column
#if !GM_V2
    for (unsigned int c = 0; c < BPAD / 32; c++)
      for (unsigned int i = 0; i < 32; i++) pool[(c * KP_SIZE + KP_SIZE - 1) * 32 + i] = 0x3C003C00u;  // {1, 1}
#endif
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    gm_vme(z, table, idx, pool, w);
#if GM_V2
    gm_gemm(w, pool, z, bv);  // GeMM + bias + ReLU
#endif

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- widening %s: %d bags x %d lookups, D=%d -> MLP %dx%dx%d, VME -----\n",
           MODE_GCN ? "GCN layer" : "DLRM embedding-bag + MLP", B_SIZE, L_SIZE, D_SIZE, DOUT_SIZE, D_SIZE, B_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);

    for (unsigned int o = 0; o < DOUT_SIZE; o++)
      errors += opope_check_fp32(z + o * BPAD, (const float *)gm_golden + o * B_SIZE, B_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
