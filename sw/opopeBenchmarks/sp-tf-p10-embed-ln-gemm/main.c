// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P10: embedding -> LayerNorm -> GeMM
// Spatz: emb[t] = wte[token[t]] + wpe[t] (scalar row address + unit-stride row loads, no indexed vector ops);
// O-POPE: transposes emb to feature-major (identity GeMM); Spatz: LN; O-POPE: Z = bias + W^T * LN(emb^T).
// Feature-major layout (see include/opope_transformer.h): O-POPE job Z^T[Cout][T] = Y + W^T * A^T.

#include "opope_transformer.h"

#include DATAHEADER

static uint32_t *tok;
static float *wte;
static float *wpe;
static float *emb;
static float *ident;
static float *embt;
static float *gm;
static float *bt;
static float *w;
static float *bias;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(tok, uint32_t, T_SIZE);
    OPOPE_L1_ALLOC(wte, float, V_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(wpe, float, T_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(emb, float, T_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(ident, float, T_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(embt, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(gm, float, C_SIZE);
    OPOPE_L1_ALLOC(bt, float, C_SIZE);
    OPOPE_L1_ALLOC(w, float, C_SIZE * COUT_SIZE);
    OPOPE_L1_ALLOC(bias, float, COUT_SIZE);
    OPOPE_L1_ALLOC(z, float, COUT_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(tok, d_tok);
    TF_DMA(wte, d_wte);
    TF_DMA(wpe, d_wpe);
    TF_DMA(gm, d_gamma);
    TF_DMA(bt, d_beta);
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

    tf_encoder(emb, tok, wte, wpe, T_SIZE, C_SIZE);
    tf_identity(ident, T_SIZE);
    opope_vsync(emb);
    tf_transpose(embt, emb, ident, T_SIZE, C_SIZE);
    tf_layernorm(embt, embt, gm, bt, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    tf_fill_rows(z, bias, COUT_SIZE, T_SIZE);
    opope_gemm(w, embt, z, COUT_SIZE, C_SIZE, T_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P10: embedding -> LayerNorm -> GeMM -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
