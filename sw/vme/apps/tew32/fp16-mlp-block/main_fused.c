// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P9: MLP block (LN -> GeMM1 + b1 -> GELU -> GeMM2 + b2 + residual), FUSED on VME (Spatz VFU + OPE).
// Derived from baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p9-mlp-block/main.c: same input values, inputs
// and constant operand rows placed outside the measured region, same check after it. One fused kernel interleaves
// the VFU (LayerNorm, GELU) and the OPE (both GeMMs) at K-group granularity (schedule: script/gen_mlp_block_fused.py).

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/p9_fused.c"

static float *z;
static float *h;
static uint32_t *lnpk;
static uint32_t *hpk;
static uint32_t *a1;
static uint32_t *a2;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    OPOPE_L1_ALLOC(z, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(h, float, HID_PAD * T_SIZE + 64);
    OPOPE_L1_ALLOC(lnpk, uint32_t, 2 * (KP1 + 1) * 32);
    OPOPE_L1_ALLOC(hpk, uint32_t, 2 * (KP2 + 1) * 32);
    OPOPE_L1_ALLOC(a1, uint32_t, sizeof(d_a1) / 4);
    OPOPE_L1_ALLOC(a2, uint32_t, sizeof(d_a2) / 4);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(z, d_x);
    TF_DMA(a1, d_a1);
    TF_DMA(a2, d_a2);
    // constant reduction rows: lnpk K-pair 32 = {1, 1} (bias1), hpk K-pair 72 = {1, 0} (bias2)
    snrt_dma_start_1d(lnpk + (KP1 - 1) * 32, d_lnone, sizeof(d_lnone));
    snrt_dma_start_1d(lnpk + (KP1 + (KP1 - 1)) * 32, d_lnone, sizeof(d_lnone));
    snrt_dma_start_1d(hpk + (KP2 - 1) * 32, d_hone, sizeof(d_hone));
    snrt_dma_start_1d(hpk + (KP2 + (KP2 - 1)) * 32, d_hone, sizeof(d_hone));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    p9_fused(a1, a2, z, h, lnpk, hpk);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P9: MLP block, fused VME -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, C_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
