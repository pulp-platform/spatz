// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P3: LayerNorm -> GeMM -> GELU (full MLP up-projection)
// Spatz (FP32): LayerNorm -> FP16 operand; O-POPE: GeMM; Spatz (FP32): GELU.
// FP16 GeMM inputs / FP32 outputs; feature-major activations, pair-packed GeMM operands (see
// include/opope_tf16.h). O-POPE job: Z^T[Cout][T] = Y + W^T * A^T (SDOTP, FP16 -> FP32).

#include "opope_tf16.h"

#include DATAHEADER

static float *x;
static uint32_t *actpk;
static uint32_t *wpk;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(x, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(actpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(wpk, uint32_t, (C_SIZE / 2) * COUT_SIZE);
    OPOPE_L1_ALLOC(z, float, COUT_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(x, d_x);
    TF_DMA(wpk, d_w);
    TF_DMA(z, d_zinit);  // Y = bias broadcast (initial placement)
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    tf16_layernorm_pack(actpk, x, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    opope_gemm_fp16(wpk, actpk, z, COUT_SIZE, C_SIZE, T_SIZE);
    tf16_gelu(z, COUT_SIZE * T_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P3: LayerNorm -> GeMM -> GELU (full MLP up-projection) -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
