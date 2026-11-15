// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P6: GeMM -> softmax (logits), VME (Spatz VFU + OPE). Derived from
// baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p6-gemm-softmax/main.c: same input values and placement
// outside the measured region, same check after it; only the measured region differs.
// logits = bias + W^T * X (FP16 input, [V][T]); softmax over the V rows of every token (FP32).

#include "opope_tf16.h"

#include DATAHEADER

#ifndef P6_KERNEL
#define P6_KERNEL 0
#endif

void gemm_bias_kernel(const uint32_t *astream, const uint32_t *bpan, float *z, const uint32_t *bpair);
#include "kernel/gemm_bias.c"
void vme_softmax_cols(float *buf);
#include "kernel/softmax_cols.c"

static uint32_t *apan;
static uint32_t *bpan;
static uint32_t *bpair;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    OPOPE_L1_ALLOC(bpan, uint32_t, (C_SIZE / 2) * T_SIZE);
    OPOPE_L1_ALLOC(apan, uint32_t, (C_SIZE / 2) * COUT_SIZE);
    OPOPE_L1_ALLOC(z, float, COUT_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(bpair, uint32_t, sizeof(d_bpair) / 4);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(bpan, d_bpan);
    TF_DMA(apan, d_apan);
    TF_DMA(z, d_zinit);  // Y = bias broadcast (initial placement, as O-POPE)
    TF_DMA(bpair, d_bpair);
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    gemm_bias_kernel(apan, bpan, z, bpair);
#if P6_KERNEL
    vme_softmax_cols(z);
#else
    tf16_softmax_cols(z, COUT_SIZE, T_SIZE, 0, 0);
#endif

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P6: GeMM -> softmax (logits), VME -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
