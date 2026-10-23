// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Widening GeMM on the O-POPE HWPE (SDOTP datapath): Z[M][K] = Y + X*W, FP16 X and W (two values per
// word along N), FP32 accumulation and FP32 Y/Z, from a single core.

#include "opope_app.h"

#include DATAHEADER

uint32_t *xt;
uint32_t *w;
float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate the matrices in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(xt, uint32_t, (N_SIZE / 2) * M_SIZE);
    OPOPE_L1_ALLOC(w, uint32_t, (N_SIZE / 2) * K_SIZE);
    OPOPE_L1_ALLOC(z, float, M_SIZE * K_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(xt, wm_xt_dram, (N_SIZE / 2) * M_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(w, wm_w_dram, (N_SIZE / 2) * K_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(z, wm_y_dram, M_SIZE * K_SIZE * sizeof(float));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    opope_gemm_fp16(xt, w, z, M_SIZE, N_SIZE, K_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- (%dx%dx%d) widening FP16->FP32 O-POPE matmul -----\n", M_SIZE, N_SIZE, K_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle.\n", performance);

    errors = opope_check_fp32(z, (const float *)wm_golden, M_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
