// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P11: transformer block (LN, QKV, causal attention, projection, MLP), single head
// x -> LN1 -> Q^T,K^T (feature-major), V (token-major, X/W operands swapped) -> attention -> proj + residual
// -> LN2 -> GeMM1 + GELU -> GeMM2 + residual. 7 O-POPE jobs + 8 Spatz stages, everything alive in the TCDM.
// Feature-major layout (see include/opope_transformer.h): O-POPE job Z^T[Cout][T] = Y + W^T * A^T.

#include "opope_transformer.h"

#include DATAHEADER

static float *x;
static float *lnb;
static float *g1;
static float *be1;
static float *g2;
static float *be2;
static float *wq;
static float *wk;
static float *wv;
static float *wp;
static float *bq;
static float *bk;
static float *bv;
static float *bp;
static float *q;
static float *kk;
static float *v;
static float *s;
static float *att;
static float *z;
static float *w1;
static float *bm1;
static float *h;
static float *w2;
static float *bm2;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(x, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnb, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(g1, float, C_SIZE);
    OPOPE_L1_ALLOC(be1, float, C_SIZE);
    OPOPE_L1_ALLOC(g2, float, C_SIZE);
    OPOPE_L1_ALLOC(be2, float, C_SIZE);
    OPOPE_L1_ALLOC(wq, float, C_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(wk, float, C_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(wv, float, C_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(wp, float, C_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(bq, float, C_SIZE);
    OPOPE_L1_ALLOC(bk, float, C_SIZE);
    OPOPE_L1_ALLOC(bv, float, C_SIZE);
    OPOPE_L1_ALLOC(bp, float, C_SIZE);
    OPOPE_L1_ALLOC(q, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(kk, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(v, float, T_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(s, float, T_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(att, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(z, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(w1, float, C_SIZE * HID_SIZE);
    OPOPE_L1_ALLOC(bm1, float, HID_SIZE);
    OPOPE_L1_ALLOC(h, float, HID_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(w2, float, HID_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(bm2, float, C_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(x, d_x);
    TF_DMA(g1, d_g1);
    TF_DMA(be1, d_be1);
    TF_DMA(g2, d_g2);
    TF_DMA(be2, d_be2);
    TF_DMA(wq, d_wq);
    TF_DMA(wk, d_wk);
    TF_DMA(wv, d_wv);
    TF_DMA(wp, d_wp);
    TF_DMA(bq, d_bq);
    TF_DMA(bk, d_bk);
    TF_DMA(bv, d_bv);
    TF_DMA(bp, d_bp);
    TF_DMA(w1, d_w1);
    TF_DMA(bm1, d_bm1);
    TF_DMA(w2, d_w2);
    TF_DMA(bm2, d_bm2);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // LN1 -> Q^T, K^T (feature-major) and V (token-major: X = LN^T, W = Wv, Y0 = bv per column)
    tf_layernorm(lnb, x, g1, be1, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    tf_fill_rows(q, bq, C_SIZE, T_SIZE);
    opope_gemm(wq, lnb, q, C_SIZE, C_SIZE, T_SIZE);
    tf_fill_rows(kk, bk, C_SIZE, T_SIZE);
    opope_gemm(wk, lnb, kk, C_SIZE, C_SIZE, T_SIZE);
    tf_fill_cols(v, bv, T_SIZE, C_SIZE);
    opope_gemm(lnb, wv, v, T_SIZE, C_SIZE, C_SIZE);
    // attention (single head)
    opope_vzero(s, T_SIZE * T_SIZE);
    opope_gemm(kk, q, s, T_SIZE, C_SIZE, T_SIZE);
    tf_softmax_cols(s, T_SIZE, T_SIZE, SCALE_F, 1);
    opope_vzero(att, C_SIZE * T_SIZE);
    opope_gemm(v, s, att, C_SIZE, T_SIZE, T_SIZE);
    // projection + residual: Y0 = x + bp
    tf_fill_rows(z, bp, C_SIZE, T_SIZE);
    tf_add(z, x, C_SIZE * T_SIZE);
    opope_gemm(wp, att, z, C_SIZE, C_SIZE, T_SIZE);
    // MLP: LN2 -> GeMM1 + GELU -> GeMM2 + residual (Y0 = z + bm2)
    tf_layernorm(lnb, z, g2, be2, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    tf_fill_rows(h, bm1, HID_SIZE, T_SIZE);
    opope_gemm(w1, lnb, h, HID_SIZE, C_SIZE, T_SIZE);
    tf_gelu(h, HID_SIZE * T_SIZE);
    tf_add_bias_rows(z, bm2, C_SIZE, T_SIZE);
    opope_gemm(w2, h, z, C_SIZE, HID_SIZE, T_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P11: transformer block (LN, QKV, causal attention, projection, MLP), single head -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, C_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
