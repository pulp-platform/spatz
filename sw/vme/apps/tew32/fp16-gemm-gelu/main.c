// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P2: GeMM -> GELU (MLP up-projection), FUSED on VME (Spatz VFU + OPE).
// Derived from baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p2-gemm-gelu/main.c: same input values (inputs and
// constant operand rows placed outside the measured region), same check after it. One kernel: the GeMM K-groups run
// in register windows of the GELU (schedule: script/gen_gemm_gelu.py).

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/p2_fused.c"

static uint32_t *a;
static uint32_t *xpk;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    OPOPE_L1_ALLOC(a, uint32_t, sizeof(d_a) / 4);
    OPOPE_L1_ALLOC(xpk, uint32_t, sizeof(d_xpk) / 4);
    OPOPE_L1_ALLOC(z, float, COUT_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(a, d_a);
    TF_DMA(xpk, d_xpk);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    p2_fused(a, xpk, z);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P2: GeMM -> GELU (MLP up-projection), fused VME -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
