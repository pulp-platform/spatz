// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Vector-quantised GeMM, FP16 inputs and FP32 output: Z[M][K] = X * decode(W). The weights are stored
// as codebook indices (NCB residual FP16 codebooks of CBN entries of CB_D, one code per CB_D
// consecutive columns of a row, plus a per-row FP16 scale). Spatz decodes W in FP16 (linear ops:
// add of the codewords and scale, no widening needed) straight into the packed W operand of the
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
_Float16 *scale;

// Decode W[n][g*CB_D .. +CB_D) = scale[n] * sum_c cb[c][idx[c][n][g]][0..CB_D) into the packed layout
// The even row 2p and the odd row 2p+1 of a pair are decoded together: their codewords are zero-extended to 32 bit
// (widening add with x0), the odd ones shifted into the high half and OR-ed, giving for each codebook ONE vector of
// words {even, odd}; one FP16 add on these vectors (SEW = 16, 2*CB_D lanes) then sums the codebooks of both rows at
// once and the result is already the packed word. One unit-stride store, no strided accesses, no drains.
// (The four codeword loads are separated by compute instructions: four back-to-back vector loads hang this cluster.)
static void decode_weights(void) {
  unsigned int vl;
  const uint16_t *const cbl = cb, *const idl = idx;
  uint32_t *const wdl = wd;
  for (unsigned int p = 0; p < N_SIZE / 2; p++) {
    // per-row scales of the pair as one word {scale[2p] (lo), scale[2p+1] (hi)}, splatted into v12
    const uint32_t scpair = (uint32_t)((const uint16_t *)scale)[2 * p] | ((uint32_t)((const uint16_t *)scale)[2 * p + 1] << 16);
    asm volatile("vsetvli zero, %0, e32, m2, ta, ma" ::"r"(CB_D));
    asm volatile("vmv.v.x v12, %0" ::"r"(scpair));
    for (unsigned int g = 0; g < GROUPS; g++) {
      const uint16_t *a0 = cbl + (idl[(2 * p) * GROUPS + g]) * CB_D;
      const uint16_t *a1 = cbl + (CBN + idl[N_SIZE * GROUPS + (2 * p) * GROUPS + g]) * CB_D;
      const uint16_t *b0 = cbl + (idl[(2 * p + 1) * GROUPS + g]) * CB_D;
      const uint16_t *b1 = cbl + (CBN + idl[N_SIZE * GROUPS + (2 * p + 1) * GROUPS + g]) * CB_D;
      asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(CB_D));
      asm volatile("vle16.v v0, (%0)" ::"r"(a0) : "memory");
      asm volatile("vle16.v v1, (%0)" ::"r"(b0) : "memory");
      asm volatile("vwaddu.vx v4, v0, zero");
      asm volatile("vwaddu.vx v6, v1, zero");
      asm volatile("vle16.v v2, (%0)" ::"r"(a1) : "memory");
      asm volatile("vle16.v v3, (%0)" ::"r"(b1) : "memory");
      asm volatile("vwaddu.vx v8, v2, zero");
      asm volatile("vwaddu.vx v10, v3, zero");
      asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(CB_D));
      asm volatile("vsll.vi v6, v6, 16");
      asm volatile("vsll.vi v10, v10, 16");
      asm volatile("vor.vv v4, v4, v6");
      asm volatile("vor.vv v8, v8, v10");
      asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(2 * CB_D));
      asm volatile("vfadd.vv v4, v4, v8");
      asm volatile("vfmul.vv v4, v4, v12");   // (a0 + a1) * scale[2p] in the low halves, (b0 + b1) * scale[2p+1] in the high halves
      asm volatile("vse16.v v4, (%0)" ::"r"((uint16_t *)(wdl + p * K_SIZE + g * CB_D)) : "memory");
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
    OPOPE_L1_ALLOC(scale, _Float16, N_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (compressed weights, codebooks, activations)
  if (cid == 0) {
    snrt_dma_start_1d(xt, vq_xt_dram, (N_SIZE / 2) * M_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(cb, vq_cb_dram, NCB * CBN * CB_D * sizeof(uint16_t));
    snrt_dma_start_1d(idx, vq_idx_dram, NCB * N_SIZE * GROUPS * sizeof(uint16_t));
    snrt_dma_start_1d(scale, vq_scale_dram, N_SIZE * sizeof(uint16_t));
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
    decode_weights();

    // GeMM (O-POPE, FP16 -> FP32)
    opope_gemm_fp16(xt, wd, z, M_SIZE, N_SIZE, K_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- widening VQ-GeMM %dx%dx%d (CB_D=%d, %d codebooks of %d) -----\n", M_SIZE,
           N_SIZE, K_SIZE, CB_D, NCB, CBN);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (decode included).\n", performance);

    errors = opope_check_fp32(z, (const float *)vq_golden, M_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
