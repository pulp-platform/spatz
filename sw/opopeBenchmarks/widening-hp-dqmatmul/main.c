// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Dequantised-weight GeMM (block codebook), FP16 inputs and FP32 output: Z[M][K] = X * decode(W). The
// weights are stored as 16-bit codebook indices (2 additive FP16 codebooks of CBN entries of CB_D = 8,
// one code per CB_D consecutive columns of a row, no scale). Spatz decodes W in FP16 (linear op:
// add of the codewords, no widening needed) straight into the packed W operand of the
// widening GeMM: word (p,k) = {W[2p][k], W[2p+1][k]}. The even and odd weight rows of a pair are
// decoded one after the other and written with two stride-4 16-bit stores (low / high half of the
// words), so the packing costs nothing extra. Activations are provided packed (X^T [N/2][M]) and Z is
// row-major [M][K], zero-filled first (plain matmul).

#include "opope_app.h"

#include DATAHEADER

uint32_t *xt;
uint32_t *wd;  // decoded + packed W operand [N/2][K]
float *z;
uint16_t *cb;
uint16_t *idx;

// Decode W[n][g*CB_D .. +CB_D) = sum_c cb[c][idx[c][n][g]][0..CB_D) into the packed layout
static void decode_weights(void) {
  unsigned int vl;
  asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(CB_D));  // CB_D = 16
  for (unsigned int p = 0; p < N_SIZE / 2; p++) {
    for (unsigned int g = 0; g < GROUPS; g++) {
      for (unsigned int par = 0; par < 2; par++) {
        const unsigned int n = 2 * p + par;
        const uint16_t *e0 = cb + (idx[n * GROUPS + g]) * CB_D;
        asm volatile("vle16.v v0, (%0)" ::"r"(e0) : "memory");
#if NCB == 2
        const uint16_t *e1 = cb + (CBN + idx[N_SIZE * GROUPS + n * GROUPS + g]) * CB_D;
        asm volatile("vle16.v v1, (%0)" ::"r"(e1) : "memory");
        asm volatile("vfadd.vv v0, v0, v1");
#endif
        asm volatile("vsse16.v v0, (%0), %1"
                     ::"r"((uint16_t *)(wd + p * K_SIZE + g * CB_D) + par), "r"(4)
                     : "memory");
        opope_vsync(wd);  // one strided access in flight at a time
      }
    }
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(xt, uint32_t, (N_SIZE / 2) * M_SIZE);
    OPOPE_L1_ALLOC(wd, uint32_t, (N_SIZE / 2) * K_SIZE);
    OPOPE_L1_ALLOC(z, float, M_SIZE * K_SIZE);
    OPOPE_L1_ALLOC(cb, uint16_t, NCB * CBN * CB_D);
    OPOPE_L1_ALLOC(idx, uint16_t, NCB * N_SIZE * GROUPS);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (compressed weights, codebooks, activations)
  if (cid == 0) {
    snrt_dma_start_1d(xt, dq_xt_dram, (N_SIZE / 2) * M_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(cb, dq_cb_dram, NCB * CBN * CB_D * sizeof(uint16_t));
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

    // Pre-processing (Spatz, FP16): decode W into the packed operand, Y = 0
    decode_weights();
    opope_vzero(z, M_SIZE * K_SIZE);

    // GeMM (O-POPE, FP16 -> FP32)
    opope_gemm_fp16(xt, wd, z, M_SIZE, N_SIZE, K_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- widening DQ-GeMM %dx%dx%d (CB_D=%d, %d codebooks of %d) -----\n", M_SIZE,
           N_SIZE, K_SIZE, CB_D, NCB, CBN);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (decode included).\n", performance);

    errors = opope_check_fp32(z, (const float *)dq_golden, M_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
