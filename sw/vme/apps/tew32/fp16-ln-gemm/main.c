// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P1: LayerNorm -> GeMM (QKV-like projection) on VME (Spatz VFU + OPE).
// Derived from baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p1-ln-gemm/main.c: same input values (inputs and
// placed outside the measured region), same check after it. LayerNorm (VFU) -> packed operand -> GeMM (OPE)
// (kernels: script/gen_ln_gemm.py).

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/p1_ln.c"
#include "kernel/p1_fused.c"

static uint32_t *a;
static uint32_t *bv;
static float *x;
static uint32_t *lnpk;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    OPOPE_L1_ALLOC(x, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnpk, uint32_t, (C_SIZE / 2) * T_SIZE + 64);
    OPOPE_L1_ALLOC(a, uint32_t, sizeof(d_w_a) / 4);
    OPOPE_L1_ALLOC(bv, uint32_t, sizeof(d_bv) / 4);
    OPOPE_L1_ALLOC(z, float, COUT_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(x, d_x);
    TF_DMA(a, d_w_a);
    TF_DMA(bv, d_bv);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    p1_ln(lnpk, x);
    p1_gemm(a, lnpk, z, bv);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P1: LayerNorm -> GeMM (QKV-like projection), VME -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
