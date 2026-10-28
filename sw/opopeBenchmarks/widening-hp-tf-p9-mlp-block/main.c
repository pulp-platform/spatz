// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P9: MLP block (LN -> GeMM1 -> GELU -> GeMM2 + residual)
// Spatz (FP32): LN -> FP16; O-POPE: GeMM1 (Y0 = bias1); Spatz (FP32): GELU -> FP16; O-POPE: GeMM2 (Y0 = residual + bias2, z holds the FP32 input).
// FP16 GeMM inputs / FP32 outputs; feature-major activations, pair-packed GeMM operands (see
// include/opope_tf16.h). O-POPE job: Z^T[Cout][T] = Y + W^T * A^T (SDOTP, FP16 -> FP32).

#include "opope_tf16.h"

#include DATAHEADER

static float *z;
static uint32_t *lnpk;
static uint32_t *w1;
static float *h;
static uint32_t *hpk;
static uint32_t *w2;
static float *b2;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(z, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(w1, uint32_t, (C_SIZE / 2) * HID_SIZE);
    OPOPE_L1_ALLOC(h, float, HID_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(hpk, uint32_t, (HID_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(w2, uint32_t, (HID_SIZE / 2) * C_SIZE);
    OPOPE_L1_ALLOC(b2, float, C_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(z, d_x);
    TF_DMA(w1, d_w1);
    TF_DMA(h, d_hinit);  // Y of GeMM1 = bias1' broadcast (initial placement)
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

    tf16_layernorm_pack(lnpk, z, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    opope_gemm_fp16(w1, lnpk, h, HID_SIZE, C_SIZE, T_SIZE);
    tf16_gelu_pack(hpk, h, HID_SIZE, T_SIZE);
    tf_add_bias_rows(z, b2, C_SIZE, T_SIZE);  // Y0 = residual + bias2
    opope_gemm_fp16(w2, hpk, z, C_SIZE, HID_SIZE, T_SIZE);

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
