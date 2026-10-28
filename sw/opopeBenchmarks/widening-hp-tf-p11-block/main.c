// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P11: transformer block (LN, QKV, causal attention, projection, MLP), single head
// x (FP32) -> LN1 -> Q^T,K^T (feature-major) and V (token-major: operands swapped) -> attention -> proj + residual
// -> LN2 -> GeMM1 + GELU -> GeMM2 + residual. 8 O-POPE FP16->FP32 jobs, every FP32 stage narrowed + packed for the next GeMM.
// FP16 GeMM inputs / FP32 outputs; feature-major activations, pair-packed GeMM operands (see
// include/opope_tf16.h). O-POPE job: Z^T[Cout][T] = Y + W^T * A^T (SDOTP, FP16 -> FP32).

#include "opope_tf16.h"

#include DATAHEADER

_Static_assert(T_SIZE == 32, "tf16_gelu_pack_t32 needs T == 32");

static float *x;
static uint32_t *lnpk;
static uint32_t *wq;
static uint32_t *wk;
static uint32_t *wv;
static uint32_t *wp;
static float *q;
static float *kk;
static float *v;
static uint32_t *qpk;
static uint32_t *kpk;
static uint32_t *vpk;
static float *s;
static uint32_t *ppk;
static float *att;
static uint32_t *attpk;
static float *z;
static uint32_t *w1;
static float *h;
static uint32_t *hpk;
static uint32_t *w2;
static float *bm2;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(x, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(wq, uint32_t, (C_SIZE / 2) * C_SIZE);
    OPOPE_L1_ALLOC(wk, uint32_t, (C_SIZE / 2) * C_SIZE);
    OPOPE_L1_ALLOC(wv, uint32_t, (C_SIZE / 2) * C_SIZE);
    OPOPE_L1_ALLOC(wp, uint32_t, (C_SIZE / 2) * C_SIZE);
    OPOPE_L1_ALLOC(q, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(kk, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(v, float, T_SIZE * C_SIZE);
    OPOPE_L1_ALLOC(qpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(kpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(vpk, uint32_t, (T_SIZE / 2) * C_SIZE);
    OPOPE_L1_ALLOC(s, float, T_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(ppk, uint32_t, (T_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(att, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(attpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(z, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(w1, uint32_t, (C_SIZE / 2) * HID_SIZE);
    OPOPE_L1_ALLOC(h, float, HID_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(hpk, uint32_t, (HID_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(w2, uint32_t, (HID_SIZE / 2) * C_SIZE);
    OPOPE_L1_ALLOC(bm2, float, C_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(x, d_x);
    TF_DMA(wq, d_wq);
    TF_DMA(q, d_qinit);    // Y of the Q / K / V GeMMs = their (LN-folded, scaled) biases broadcast
    TF_DMA(kk, d_kinit);
    TF_DMA(v, d_vinit);
    TF_DMA(s, d_mask);     // Y of the score GeMM = causal mask
    TF_DMA(z, d_zinit);    // Y of the projection GeMM = x + bias_p
    TF_DMA(h, d_hinit);    // Y of the first MLP GeMM = bias1' broadcast
    opope_dma_zero(att, C_SIZE * T_SIZE * sizeof(float));
    TF_DMA(wk, d_wk);
    TF_DMA(wv, d_wv);
    TF_DMA(wp, d_wp);
    TF_DMA(w1, d_w1);
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

    // LN1 (statistics only) -> Q^T, K^T (feature-major) and V (token-major: X = LN^T, W = Wv)
    tf16_layernorm_pack(lnpk, x, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    opope_gemm_fp16(wq, lnpk, q, C_SIZE, C_SIZE, T_SIZE);
    opope_gemm_fp16(wk, lnpk, kk, C_SIZE, C_SIZE, T_SIZE);
    opope_gemm_fp16(lnpk, wv, v, T_SIZE, C_SIZE, C_SIZE);
    // cast + pack the attention operands
    tf_pack_rows(qpk, q, C_SIZE, T_SIZE);
    tf_pack_rows(kpk, kk, C_SIZE, T_SIZE);
    tf_pack_rows(vpk, v, T_SIZE, C_SIZE);
    // attention (single head): the mask is the Y of the score GeMM, the 1/sqrt(C) scale is folded into Wq
    opope_gemm_fp16(kpk, qpk, s, T_SIZE, C_SIZE, T_SIZE);
    tf16_softmax_pack_causal(ppk, s, T_SIZE, T_SIZE);
    opope_gemm_fp16(vpk, ppk, att, C_SIZE, T_SIZE, T_SIZE);
    tf_pack_rows(attpk, att, C_SIZE, T_SIZE);
    // projection + residual (Y0 = x + bp from the header)
    opope_gemm_fp16(wp, attpk, z, C_SIZE, C_SIZE, T_SIZE);
    // MLP: LN2 -> GeMM1 + GELU -> GeMM2 + residual (Y0 = z + bm2)
    tf16_layernorm_pack(lnpk, z, C_SIZE, 1.0f / C_SIZE, T_SIZE);
    opope_gemm_fp16(w1, lnpk, h, HID_SIZE, C_SIZE, T_SIZE);
    tf16_gelu_pack_t32(hpk, h, HID_SIZE);
    tf_add_bias_rows(z, bm2, C_SIZE, T_SIZE);
    opope_gemm_fp16(w2, hpk, z, C_SIZE, HID_SIZE, T_SIZE);

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
