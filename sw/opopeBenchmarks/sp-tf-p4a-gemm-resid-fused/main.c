// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P4a: GeMM with the residual folded into Y
// Spatz builds Y0 = residual + bias (z is initialised with the residual); O-POPE: Z = Y0 + W^T * H.
// Feature-major layout (see include/opope_transformer.h): O-POPE job Z^T[Cout][T] = Y + W^T * A^T.

#include "opope_transformer.h"

#include DATAHEADER

static float *h;
static float *w;
static float *bias;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(h, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(w, float, C_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(bias, float, C_SIZE);
    OPOPE_L1_ALLOC(z, float, C_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(h, d_h);
    TF_DMA(w, d_w);
    TF_DMA(bias, d_bias);
    TF_DMA(z, d_res);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    tf_add_bias_rows(z, bias, C_SIZE, T_SIZE);
    opope_gemm(w, h, z, C_SIZE, C_SIZE, T_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P4a: GeMM with the residual folded into Y -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, C_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
