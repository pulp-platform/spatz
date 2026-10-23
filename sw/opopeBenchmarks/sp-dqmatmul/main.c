// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Dequantised-weight GeMM (block codebook): Z[M][K] = X * decode(W). The weights are stored as
// 16-bit codebook indices (2 additive codebooks of CBN entries of CB_D=8 FP32, one code per CB_D
// consecutive columns of a row, no scale). Spatz decodes W into an L1 buffer with a scalar-addressed
// unit-stride row load per code (no indexed vector accesses), then O-POPE computes the GeMM.
// Layout (no transposes): activations are provided transposed (X^T[N][M]), W is decoded
// row-major [N][K], Z is row-major [M][K] and is zero-filled first (plain matmul).

#include "opope_app.h"

#include DATAHEADER

float *xt;
float *wd;
float *z;
float *cb;
uint16_t *idx;

// Decode W[n][g*CB_D .. +CB_D) = sum_c cb[c][idx[c][n][g]][0..CB_D)
static void decode_weights(void) {
  unsigned int vl;
  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(CB_D));
  for (unsigned int n = 0; n < N_SIZE; n++) {
    for (unsigned int g = 0; g < GROUPS; g++) {
      const float *e0 = cb + (idx[n * GROUPS + g]) * CB_D;
      asm volatile("vle32.v v0, (%0)" ::"r"(e0) : "memory");
#if NCB == 2
      const float *e1 = cb + (CBN + idx[N_SIZE * GROUPS + n * GROUPS + g]) * CB_D;
      asm volatile("vle32.v v8, (%0)" ::"r"(e1) : "memory");
      asm volatile("vfadd.vv v0, v0, v8");
#endif
      asm volatile("vse32.v v0, (%0)" ::"r"(wd + n * K_SIZE + g * CB_D) : "memory");
    }
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(xt, float, N_SIZE * M_SIZE);
    OPOPE_L1_ALLOC(wd, float, N_SIZE * K_SIZE);
    OPOPE_L1_ALLOC(z, float, M_SIZE * K_SIZE);
    OPOPE_L1_ALLOC(cb, float, NCB * CBN * CB_D);
    OPOPE_L1_ALLOC(idx, uint16_t, NCB * N_SIZE * GROUPS);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (compressed weights, codebooks, activations)
  if (cid == 0) {
    snrt_dma_start_1d(xt, dq_xt_dram, N_SIZE * M_SIZE * sizeof(float));
    snrt_dma_start_1d(cb, dq_cb_dram, NCB * CBN * CB_D * sizeof(float));
    snrt_dma_start_1d(idx, dq_idx_dram, NCB * N_SIZE * GROUPS * sizeof(uint16_t));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // Pre-processing (Spatz): decode W, Y = 0
    decode_weights();
    opope_vzero(z, M_SIZE * K_SIZE);

    // GeMM (O-POPE)
    opope_gemm(xt, wd, z, M_SIZE, N_SIZE, K_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- DQ-GeMM %dx%dx%d (CB_D=%d, %d codebooks of %d) -----\n", M_SIZE, N_SIZE,
           K_SIZE, CB_D, NCB, CBN);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (decode included).\n", performance);

    errors = opope_check_fp32(z, (const float *)dq_golden, M_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
