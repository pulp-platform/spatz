// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P7: GeMM -> softmax -> cross-entropy (LM head loss)
// O-POPE: logits = bias + W^T * X (FP16 input, [V][T]); Spatz (FP32): softmax over the V rows of every token; loss[t] = -log(p[target[t]][t]) (scalar log).
// FP16 GeMM inputs / FP32 outputs; feature-major activations, pair-packed GeMM operands (see
// include/opope_tf16.h). O-POPE job: Z^T[Cout][T] = Y + W^T * A^T (SDOTP, FP16 -> FP32).

#include "opope_tf16.h"

#include DATAHEADER

static uint32_t *xpk;
static uint32_t *wpk;
static float *z;
static uint32_t *tok;
static float *loss;
static float *tgt;    // target logits
static float *cmax;   // per-token max logit
static float *csum;   // per-token sum of exp(logit - max)

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(xpk, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(wpk, uint32_t, (C_SIZE / 2) * V_SIZE);
    OPOPE_L1_ALLOC(z, float, V_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(tok, uint32_t, T_SIZE);
    OPOPE_L1_ALLOC(loss, float, T_SIZE);
    OPOPE_L1_ALLOC(tgt, float, T_SIZE);
    OPOPE_L1_ALLOC(cmax, float, T_SIZE);
    OPOPE_L1_ALLOC(csum, float, T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(xpk, d_xpk);
    TF_DMA(wpk, d_w);
    TF_DMA(tok, d_tok);
    TF_DMA(z, d_zinit);  // Y = bias broadcast (initial placement)
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    opope_gemm_fp16(wpk, xpk, z, V_SIZE, C_SIZE, T_SIZE);
    // Cross entropy through the log-softmax identity: loss[t] = log(sum_t) + max_t - logit[tok[t]][t].
    // Gather the target logits (scalar loads, the GeMM has completed), then softmax (it also returns max_t and
    // sum_t) and one vectorised log: no scalar log, no per-token probability gather.
    opope_vsync(z);
    for (unsigned int t = 0; t < T_SIZE; t++) tgt[t] = z[tok[t] * T_SIZE + t];
    opope_vsync(tgt + T_SIZE - 1);
    tf16_softmax_cols(z, V_SIZE, T_SIZE, cmax, csum);
    tf16_ce_loss(loss, csum, cmax, tgt, T_SIZE);
    opope_vsync(loss);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P7: GeMM -> softmax -> cross-entropy (LM head loss) -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, V_SIZE * T_SIZE, 1e-3f, 1e-3f);
    errors += opope_check_fp32(loss, (const float *)d_loss, T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
