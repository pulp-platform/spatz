// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P3: LayerNorm -> GeMM -> GELU (full MLP up-projection), FUSED on VME (Spatz VFU + OPE).
// Derived from baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p3-ln-gemm-gelu/main.c: same input values (inputs and
// constant operand rows placed outside the measured region), same check after it. LayerNorm, then one kernel in which
// the GeMM K-groups run in register windows of the GELU (schedule: script/gen_ln_gemm_gelu.py).

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/p3_ln.c"
#include "kernel/p3_fused.c"

static uint32_t *a;
static uint32_t *lnpk;
static float *x;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    OPOPE_L1_ALLOC(a, uint32_t, sizeof(d_a) / 4);
    OPOPE_L1_ALLOC(x, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnpk, uint32_t, 2 * KP_SIZE * 32);
    OPOPE_L1_ALLOC(z, float, COUT_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(a, d_a);
    TF_DMA(x, d_x);
    // constant reduction row of the bias K-pair: lnpk K-pair C/2 = {1, 1} in both token halves
    snrt_dma_start_1d(lnpk + (KP_SIZE - 1) * 32, d_lnone, sizeof(d_lnone));
    snrt_dma_start_1d(lnpk + (2 * KP_SIZE - 1) * 32, d_lnone, sizeof(d_lnone));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    p3_ln(lnpk, x);
    p3_fused(a, lnpk, z);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P3: LayerNorm -> GeMM -> GELU, fused VME -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
