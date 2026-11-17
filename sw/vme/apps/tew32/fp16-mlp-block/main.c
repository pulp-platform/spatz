// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P9: MLP block (LN -> GeMM1 -> GELU -> GeMM2 + residual)
// FP16 GEMM inputs and FP32 outputs with pair-packed feature-major operands.

#include "runtime.h"
#include "kernel/mlp.h"

#ifndef APP_FUSED
#define APP_FUSED 1
#endif

#include DATAHEADER
_Static_assert(T_SIZE==64 && C_SIZE==64 && HID_SIZE==144, "fixed MLP shape");
_Static_assert(VME_BYTES<=120*1024, "TCDM application budget");
static float *z;
static uint32_t *lnpk;
static uint32_t *w1;
static float *h;
static uint32_t *hpk;
static uint32_t *w2;

#include "kernel/pipeline.c"

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    L1_ALLOC(z, float, C_SIZE * T_SIZE);
    L1_ALLOC(lnpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    L1_ALLOC(w1, uint32_t, (C_SIZE / 2) * HID_SIZE);
    L1_ALLOC(h, float, HID_SIZE * T_SIZE);
    L1_ALLOC(hpk, uint32_t, (GEMM2_K_SIZE / 2) * T_SIZE);
    L1_ALLOC(w2, uint32_t, (GEMM2_K_SIZE / 2) * C_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    DMA(z, d_x);
    DMA(lnpk, d_offsets64);
    DMA(w1, d_w1_vme);
    DMA(h, d_hinit);
    DMA(hpk, d_hpk_seed);
    DMA(w2, d_w2_vme);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    run_mlp_pipeline();

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- VME P9: MLP block (LN -> GeMM1 -> GELU -> GeMM2 + residual) -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = transformer_check_fp32(z, (const float *)d_golden, C_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
