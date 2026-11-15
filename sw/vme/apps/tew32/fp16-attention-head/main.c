// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P8: attention head, VME (Spatz VFU + OPE). Derived from
// baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p8-attention/main.c: same input values and placement outside the
// measured region, same check after it; only the measured region differs.
// S^T[key][query] = K * Q^T + mask; causal softmax over the keys, P narrowed + packed; O^T[d][query] = V^T * P^T.

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/p8_kernels.c"

static uint32_t *qb;
static uint32_t *ka;
static uint32_t *va;
static float *s;
static uint32_t *ppk;
static float *o;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    OPOPE_L1_ALLOC(qb, uint32_t, sizeof(d_q_b) / 4 + 64);
    OPOPE_L1_ALLOC(ka, uint32_t, sizeof(d_k_a) / 4);
    OPOPE_L1_ALLOC(va, uint32_t, sizeof(d_v_a) / 4);
    OPOPE_L1_ALLOC(s, float, T_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(ppk, uint32_t, (T_SIZE / 2) * T_SIZE + 4 * T_SIZE);
    OPOPE_L1_ALLOC(o, float, D_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(qb, d_q_b);
    TF_DMA(ka, d_k_a);
    TF_DMA(va, d_v_a);
    TF_DMA(s, d_mask);  // Y of the score GeMM = causal mask
    opope_dma_zero(ppk, (T_SIZE / 2) * T_SIZE * sizeof(uint32_t));  // skipped (fully masked) P rows stay 0
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    p8_gemm_s(ka, qb, s, s);
    p8_softmax_pack(ppk, s);
    p8_gemm_o(va, ppk, o, 0);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P8: attention head, VME -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(o, (const float *)d_golden, D_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
