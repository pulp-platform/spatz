// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P8: attention head (GeMM -> scale+mask+softmax -> GeMM)
// O-POPE: S^T[key][query] = K * Q^T; Spatz: scale, causal mask, softmax over the keys of every query;
// O-POPE: Out^T[d][query] = V^T * P^T. Q^T,K^T are feature-major [d][T], V is token-major [T][d].
// Feature-major layout (see include/opope_transformer.h): O-POPE job Z^T[Cout][T] = Y + W^T * A^T.

#include "opope_transformer.h"

#include DATAHEADER

static float *q;
static float *k;
static float *v;
static float *s;
static float *o;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(q, float, D_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(k, float, D_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(v, float, T_SIZE * D_SIZE);
    OPOPE_L1_ALLOC(s, float, T_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(o, float, D_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(q, d_q);
    TF_DMA(k, d_k);
    TF_DMA(v, d_v);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    opope_vzero(s, T_SIZE * T_SIZE);
    opope_gemm(k, q, s, T_SIZE, D_SIZE, T_SIZE);
    tf_softmax_cols(s, T_SIZE, T_SIZE, SCALE_F, 1);
    opope_vzero(o, D_SIZE * T_SIZE);
    opope_gemm(v, s, o, D_SIZE, T_SIZE, T_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P8: attention head (GeMM -> scale+mask+softmax -> GeMM) -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(o, (const float *)d_golden, D_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
