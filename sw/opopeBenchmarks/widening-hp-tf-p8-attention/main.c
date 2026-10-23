// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P8: attention head (GeMM -> scale+mask+softmax -> GeMM)
// O-POPE: S^T[key][query] = K * Q^T; Spatz (FP32): scale, causal mask, softmax over the keys, P narrowed + packed;
// O-POPE: Out^T[d][query] = V^T * P^T. Q, K, V FP16 pre-packed pairs along their GeMM contraction dimension.
// FP16 GeMM inputs / FP32 outputs; feature-major activations, pair-packed GeMM operands (see
// include/opope_tf16.h). O-POPE job: Z^T[Cout][T] = Y + W^T * A^T (SDOTP, FP16 -> FP32).

#include "opope_tf16.h"

#include DATAHEADER

static uint32_t *qpk;
static uint32_t *kpk;
static uint32_t *vpk;
static float *s;
static uint32_t *ppk;
static float *o;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    OPOPE_L1_ALLOC(qpk, uint32_t, (D_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(kpk, uint32_t, (D_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(vpk, uint32_t, (T_SIZE / 2) * D_SIZE);
    OPOPE_L1_ALLOC(s, float, T_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(ppk, uint32_t, (T_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(o, float, D_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(qpk, d_qpk);
    TF_DMA(kpk, d_kpk);
    TF_DMA(vpk, d_vpk);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    opope_vzero(s, T_SIZE * T_SIZE);
    opope_gemm_fp16(kpk, qpk, s, T_SIZE, D_SIZE, T_SIZE);
    tf_softmax_pack(ppk, s, T_SIZE, T_SIZE, SCALE_F, 1);
    opope_vzero(o, D_SIZE * T_SIZE);
    opope_gemm_fp16(vpk, ppk, o, D_SIZE, T_SIZE, T_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P8: attention head (GeMM -> scale+mask+softmax -> GeMM) -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(o, (const float *)d_golden, D_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
