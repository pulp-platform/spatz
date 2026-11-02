// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// CNN layer (valid KHxKW convolution + bias + ReLU) as KH*KW accumulating O-POPE widening GeMMs -> ReLU,
// FP16 activations/weights, FP32 accumulation/output. No im2col buffer and no copy:
//   Z[M][K] += X_tap * W_tap for each tap (ky,kx), M = H*W output positions (the positions with y >= Ho or x >= Wo are
//   computed but unused), N = Cin, K = Cout.
// The input is stored as channel-pair planes (two FP16 channels per word) of H*W words, i.e. the pitch of the planes equals
// M, the pitch the O-POPE expects between the rows of the X^T operand [N/2][M]: the operand of the tap (ky,kx) is simply
// the input at the word address ky*W + kx, read in place by the engine. The packed per-tap weights are prepared offline,
// Y is initialised with the broadcast FP32 bias and every tap job accumulates into Z in place.

#include "opope_tf16.h"

#include DATAHEADER

uint32_t *in;
uint32_t *wm;
float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(in, uint32_t, CIN / 2 * H_SIZE * W_SIZE + IN_PAD);
    OPOPE_L1_ALLOC(wm, uint32_t, KH_SIZE * KW_SIZE * (CIN / 2) * K_SIZE);
    OPOPE_L1_ALLOC(z, float, M_SIZE * K_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(in, cv_in_dram, (CIN / 2 * H_SIZE * W_SIZE + IN_PAD) * sizeof(uint32_t));
    snrt_dma_start_1d(wm, cv_w_dram, KH_SIZE * KW_SIZE * (CIN / 2) * K_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(z, cv_zinit_dram, M_SIZE * K_SIZE * sizeof(float));  // Y = bias broadcast
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // GeMM (O-POPE, FP16 -> FP32): one job per filter tap, accumulating into Z in place; the operand is the input at the
    // shifted address (no im2col, no copy). Y = broadcast bias was placed by the initial DMA.
    for (unsigned int ky = 0; ky < KH_SIZE; ky++)
      for (unsigned int kx = 0; kx < KW_SIZE; kx++)
        opope_gemm_fp16(in + ky * W_SIZE + kx, wm + (ky * KW_SIZE + kx) * (CIN / 2) * K_SIZE, z, M_SIZE, N_SIZE, K_SIZE);

    // Post-processing (Spatz, FP32): ReLU
    tf16_relu(z, HO_SIZE * W_SIZE * K_SIZE);  // only the valid rows

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- widening conv %dx%dx%d -> %d (3x3) as GeMM %dx%dx%d -----\n", CIN, H_SIZE,
           W_SIZE, COUT, M_SIZE, N_SIZE, K_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (over the computed GeMM).\n", performance);

    // Check only the valid output positions
    for (unsigned int oy = 0; oy < HO_SIZE; oy++)
      errors += opope_check_fp32(z + oy * W_SIZE * K_SIZE,
                                 (const float *)cv_golden + oy * WO_SIZE * K_SIZE,
                                 WO_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
