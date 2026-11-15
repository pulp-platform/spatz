// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// CNN layer on VME (Spatz VFU + OPE, FP16 -> FP32): valid 3x3 convolution + bias + ReLU, one GeMM per filter tap on
// the shifted input (no im2col, as the O-POPE version). Derived from baseline_spatz/sw/opopeBenchmarks/
// widening-hp-conv/main.c: same input values, inputs placed outside the measured region (the weights in the
// operand layout of the VME kernel), same check after it (valid output positions). Kernel: script/gen_conv.py.

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/conv_vme.c"

uint32_t *in;
uint32_t *wm;
uint32_t *bias;
float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(in, uint32_t, CIN / 2 * H_SIZE * W_SIZE + IN_PAD);
    OPOPE_L1_ALLOC(wm, uint32_t, sizeof(cv_w) / 4);
    OPOPE_L1_ALLOC(bias, uint32_t, COUT);
    OPOPE_L1_ALLOC(z, float, M_SIZE * K_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(in, cv_in, sizeof(cv_in));
    snrt_dma_start_1d(wm, cv_w, sizeof(cv_w));
    snrt_dma_start_1d(bias, cv_bias, sizeof(cv_bias));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    conv_vme(z, in, wm, bias);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- widening conv %dx%dx%d -> %d (3x3), VME -----\n", CIN, H_SIZE, W_SIZE, COUT);
    PRINTF("The execution took %u cycles.\n", timer);

    // Check only the valid output positions
    for (unsigned int oy = 0; oy < HO_SIZE; oy++)
      errors += opope_check_fp32(z + oy * W_SIZE * K_SIZE, (const float *)cv_golden + oy * WO_SIZE * K_SIZE,
                                 WO_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
