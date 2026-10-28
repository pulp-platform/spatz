// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P10: embedding -> LayerNorm -> GeMM
// The embedding lookup is itself a GeMM: emb^T[C][T] = wpe^T (Y) + wte^T[C][V] * onehot[V][T] (FP16 table, FP32 output, so
// the result is already feature-major and FP32 as the LayerNorm wants it: no gather, no transposition job).
// The one-hot operand is built with T half-word stores into a zero-initialised buffer. Spatz (FP32): LayerNorm statistics
// -> FP16 operand (gamma/beta are folded into W/bias offline); O-POPE: Z = bias' + W'^T * LN(emb^T).
// FP16 GeMM inputs / FP32 outputs; feature-major activations, pair-packed GeMM operands (see
// include/opope_tf16.h). O-POPE job: Z^T[Cout][T] = Y + W^T * A^T (SDOTP, FP16 -> FP32).

#include "opope_tf16.h"

#include DATAHEADER

static uint32_t *tok;
static uint32_t *wte;     // [V/2][C] packed FP16 table (X^T operand)
static uint32_t *onehot;  // [V/2][T] packed one-hot (W operand)
static float *emb;        // [C][T]: Y = wpe^T, then the embeddings
static uint32_t *lnpk;
static uint32_t *wpk;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(tok, uint32_t, T_SIZE);
    OPOPE_L1_ALLOC(wte, uint32_t, (V_SIZE / 2) * C_SIZE);
    OPOPE_L1_ALLOC(onehot, uint32_t, (V_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(emb, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(wpk, uint32_t, (C_SIZE / 2) * COUT_SIZE);
    OPOPE_L1_ALLOC(z, float, COUT_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(tok, d_tok);
    TF_DMA(wte, d_wte);
    TF_DMA(emb, d_wpet);  // Y of the embedding GeMM = positional embeddings (feature-major)
    TF_DMA(wpk, d_w);
    TF_DMA(z, d_zinit);   // Y of the projection GeMM = bias' broadcast
    opope_dma_zero(onehot, (V_SIZE / 2) * T_SIZE * sizeof(uint32_t));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // one-hot W operand: onehot[tok[t]/2][t], half (tok[t] & 1) = 1.0 (FP16)
    for (unsigned int t = 0; t < T_SIZE; t++) {
      const unsigned int v = tok[t];
      ((uint16_t *)onehot)[((v >> 1) * T_SIZE + t) * 2 + (v & 1)] = 0x3C00;
    }
    opope_vsync(onehot);
    // emb^T = wpe^T + wte^T * onehot   (M = C, N = V, K = T)
    opope_gemm_fp16(wte, onehot, emb, C_SIZE, V_SIZE, T_SIZE);
    tf16_layernorm_pack(lnpk, emb, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    opope_gemm_fp16(wpk, lnpk, z, COUT_SIZE, C_SIZE, T_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P10: embedding (GeMM) -> LayerNorm -> GeMM -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
