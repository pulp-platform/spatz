// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P10: embedding -> LayerNorm -> GeMM
// Spatz (FP32, the pre-processing ends in a LayerNorm): emb[t] = wte[token[t]] + wpe[t] (scalar row address + unit-stride row loads);
// O-POPE (FP32 identity GeMM) transposes emb to feature-major; Spatz: LN -> FP16 operand; O-POPE: Z = bias + W^T * LN(emb^T).
// FP16 GeMM inputs / FP32 outputs; feature-major activations, pair-packed GeMM operands (see
// include/opope_tf16.h). O-POPE job: Z^T[Cout][T] = Y + W^T * A^T (SDOTP, FP16 -> FP32).

#include "opope_tf16.h"

#include DATAHEADER

static uint32_t *tok;
static float *wte;
static float *wpe;
static float *emb;
static float *ident;
static float *embt;
static uint32_t *lnpk;
static float *gm;
static float *bt;
static uint32_t *wpk;
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
    OPOPE_L1_ALLOC(lnpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(gm, float, C_SIZE);
    OPOPE_L1_ALLOC(bt, float, C_SIZE);
    OPOPE_L1_ALLOC(wpk, uint32_t, (C_SIZE / 2) * COUT_SIZE);
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
    TF_DMA(wpk, d_w);
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
    tf_layernorm_pack(lnpk, embt, gm, bt, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    tf_fill_rows(z, bias, COUT_SIZE, T_SIZE);
    opope_gemm_fp16(wpk, lnpk, z, COUT_SIZE, C_SIZE, T_SIZE);

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
