// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P9: MLP block (LN -> GeMM1 -> GELU -> GeMM2 + residual)
// Spatz: LN; O-POPE: GeMM1 (Y0 = bias1); Spatz: GELU; O-POPE: GeMM2 (Y0 = residual + bias2, z holds the input).
// Feature-major layout (see include/opope_transformer.h): O-POPE job Z^T[Cout][T] = Y + W^T * A^T.

#include "opope_transformer.h"

#include DATAHEADER

static float *z;
static float *lnb;
static float *gm;
static float *bt;
static float *w1;
static float *b1;
static float *h;
static float *w2;
static float *b2;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(z, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnb, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(gm, float, C_SIZE);
    OPOPE_L1_ALLOC(bt, float, C_SIZE);
    OPOPE_L1_ALLOC(w1, float, C_SIZE * HID_SIZE);
    OPOPE_L1_ALLOC(b1, float, HID_SIZE);
    OPOPE_L1_ALLOC(h, float, HID_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(w2, float, HID_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(b2, float, C_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(z, d_x);
    TF_DMA(gm, d_gamma);
    TF_DMA(bt, d_beta);
    TF_DMA(w1, d_w1);
    TF_DMA(b1, d_b1);
    TF_DMA(w2, d_w2);
    TF_DMA(b2, d_b2);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    tf_layernorm(lnb, z, gm, bt, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    tf_fill_rows(h, b1, HID_SIZE, T_SIZE);
    opope_gemm(w1, lnb, h, HID_SIZE, C_SIZE, T_SIZE);
    tf_gelu(h, HID_SIZE * T_SIZE);
    tf_add_bias_rows(z, b2, C_SIZE, T_SIZE);  // Y0 = residual + bias2
    opope_gemm(w2, h, z, C_SIZE, HID_SIZE, T_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P9: MLP block (LN -> GeMM1 -> GELU -> GeMM2 + residual) -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, C_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
