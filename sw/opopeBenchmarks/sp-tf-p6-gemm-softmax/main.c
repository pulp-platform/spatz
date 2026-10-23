// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P6: GeMM -> softmax (logits)
// O-POPE: logits = bias + W^T * X ([V][T], tokens along the columns); Spatz: softmax over the V rows of every
// token (max, exp, sum, 1/sum by Newton-Raphson).
// Feature-major layout (see include/opope_transformer.h): O-POPE job Z^T[Cout][T] = Y + W^T * A^T.

#include "opope_transformer.h"

#include DATAHEADER

static float *x;
static float *w;
static float *bias;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(x, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(w, float, C_SIZE * V_SIZE);
    OPOPE_L1_ALLOC(bias, float, V_SIZE);
    OPOPE_L1_ALLOC(z, float, V_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(x, d_x);
    TF_DMA(w, d_w);
    TF_DMA(bias, d_bias);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    tf_fill_rows(z, bias, V_SIZE, T_SIZE);
    opope_gemm(w, x, z, V_SIZE, C_SIZE, T_SIZE);
    tf_softmax_cols(z, V_SIZE, T_SIZE, 1.0f, 0);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P6: GeMM -> softmax (logits) -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, V_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
