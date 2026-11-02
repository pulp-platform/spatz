// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Dequantised-weight GeMM, FP16 inputs and FP32 output: Z[M][K] = X * decode(W). The weights are stored as indices of
// two additive FP16 PAIR-BLOCK codebooks (a codeword covers two consecutive weight rows x CB_W = 16 columns and is
// stored as packed words {row 2p, row 2p+1}, see common/pairblock.py). Spatz decodes W straight into the packed W operand
// of the widening GeMM (word (p,k) = {W[2p][k], W[2p+1][k]}) with, per group of 16 columns: 2 vector loads, 1 FP16 add
// and 1 store (one scalar load per two groups, include/opope_decode.h). Activations are provided packed (X^T [N/2][M]) and Z is row-major
// [M][K], zero-filled first (plain matmul).

#include "opope_decode.h"

#include DATAHEADER

uint32_t *xt;
uint32_t *wd;   // decoded + packed W operand [N/2][K]
float *z;
uint32_t *cbw;  // codebooks [NCB][CBN][CB_W] words
uint32_t *idxw; // [N/2][GROUPS/2] words: 7-bit indices of two groups

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(xt, uint32_t, (N_SIZE / 2) * M_SIZE);
    OPOPE_L1_ALLOC(wd, uint32_t, (N_SIZE / 2) * K_SIZE);
    OPOPE_L1_ALLOC(z, float, M_SIZE * K_SIZE);
    OPOPE_L1_ALLOC(cbw, uint32_t, NCB * CBN * CB_W);
    OPOPE_L1_ALLOC(idxw, uint32_t, (N_SIZE / 2) * (GROUPS / 2));
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (compressed weights, codebooks, activations)
  if (cid == 0) {
    snrt_dma_start_1d(xt, dq_xt_dram, (N_SIZE / 2) * M_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(cbw, dq_cb_dram, NCB * CBN * CB_W * sizeof(uint32_t));
    snrt_dma_start_1d(idxw, dq_idx_dram, (N_SIZE / 2) * (GROUPS / 2) * sizeof(uint32_t));
    opope_dma_zero(z, M_SIZE * K_SIZE * sizeof(float));  // Y = 0
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // Pre-processing (Spatz, FP16): decode W into the packed operand (Y = 0 was placed by the initial DMA)
    decode_pairblock(wd, (const char *)cbw, (const char *)(cbw + CBN * CB_W), idxw, N_SIZE / 2, GROUPS, K_SIZE);

    // GeMM (O-POPE, FP16 -> FP32)
    opope_gemm_fp16(xt, wd, z, M_SIZE, N_SIZE, K_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- widening Dequantised-weight-GeMM %dx%dx%d (pair-block CB_W=%d, %d codebooks of %d) -----\n", M_SIZE,
           N_SIZE, K_SIZE, CB_W, NCB, CBN);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (decode included).\n", performance);

    errors = opope_check_fp32(z, (const float *)dq_golden, M_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
