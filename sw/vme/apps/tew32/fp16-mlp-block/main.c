// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P9: MLP block (LN -> GeMM1 -> GELU -> GeMM2 + residual), VME (Spatz VFU + OPE). Derived from
// baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p9-mlp-block/main.c: same input values and placement outside
// the measured region, same check after it; only the measured region differs.
// LN -> FP16; GeMM1 (bias1' as an extra K-pair); GELU -> FP16; GeMM2 (Y = the FP32 input = residual, seeded into
// the tiles; bias2 is an extra reduction row pair: hpk row HID/2 = {1.0, 0}, w2 row HID/2 = {bias2, 0}).

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/p9_gemms.c"
#include "kernel/ln_pack.c"
#include "kernel/gelu_pack.c"

static float *z;
static uint32_t *lnpk;
static uint32_t *w1;
static uint32_t *bv1;
static float *h;
static uint32_t *hpk;
static uint32_t *w2;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    OPOPE_L1_ALLOC(z, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnpk, uint32_t, (C_SIZE / 2) * T_SIZE + 64);
    OPOPE_L1_ALLOC(w1, uint32_t, sizeof(d_w1_a) / 4);
    OPOPE_L1_ALLOC(bv1, uint32_t, sizeof(d_bv1) / 4);
    OPOPE_L1_ALLOC(h, float, HID_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(hpk, uint32_t, (HID_SIZE / 2 + 1) * T_SIZE + 3 * 64);
    OPOPE_L1_ALLOC(w2, uint32_t, sizeof(d_w2_a) / 4);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(z, d_x);
    TF_DMA(w1, d_w1_a);
    TF_DMA(bv1, d_bv1);   // bias1' (one extra K-pair of GeMM1)
    TF_DMA(w2, d_w2_a);
    TF_DMA(hpk + (HID_SIZE / 2) * T_SIZE, d_hone);  // constant row {1.0, 0} of the bias reduction
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    vme_ln_pack(lnpk, z);
    p9_gemm1(w1, lnpk, h, bv1);
    vme_gelu_pack(hpk, h);
    p9_gemm2(w2, hpk, z, z);  // Z = residual (Y, seeded) + W2^T h + bias2

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P9: MLP block (LN -> GeMM1 -> GELU -> GeMM2 + residual), VME -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, C_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
