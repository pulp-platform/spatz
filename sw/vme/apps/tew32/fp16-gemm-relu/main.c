// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// GeMM -> ReLU (O-POPE P5 shape C=128, T=64, COUT=192), fused on Spatz VFU + OPE (VME).
// Derived from baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p5-gemm-relu/main.c: same inputs (values),
// same placement outside the measured region, same check after it. Only the measured region differs:
// one fused kernel (GeMM on the OPE, ReLU on the VFU, tile drains on the VLSU, all interleaved).
// HAS_BIAS (from the data header) selects Z^T = relu(W^T X + bias) (O-POPE P5) or relu(W^T X).

#include <snrt.h>
#include <printf.h>
#include "benchmark.h"

#include DATAHEADER

void mr_kernel(const uint32_t *astream, const uint32_t *bpan, float *z, const uint32_t *bpair);
#if HAS_BIAS
#include "kernel/gemm_relu.c"
#else
#include "kernel/matmul_relu.c"
#endif

static uint32_t *apan;
static uint32_t *bpan;
static float *z;
static uint32_t *bpair;

static int check_fp32(const float *actual, const float *golden, unsigned n, float rel, float abs_tol) {
  int errors = 0;
  for (unsigned i = 0; i < n; ++i) {
    float d = actual[i] - golden[i];
    if (d < 0) d = -d;
    float m = golden[i] < 0 ? -golden[i] : golden[i];
    if (!(d <= abs_tol + rel * m)) {
      if (errors < 8)
        printf("Mismatch @ %u: got 0x%08x, expected 0x%08x\n", i, ((const uint32_t *)actual)[i],
               ((const uint32_t *)golden)[i]);
      ++errors;
    }
  }
  return errors;
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    apan = (uint32_t *)snrt_l1alloc(sizeof(d_apan));
    bpan = (uint32_t *)snrt_l1alloc(sizeof(d_bpan));
    z = (float *)snrt_l1alloc(sizeof(d_zinit));
    bpair = (uint32_t *)snrt_l1alloc(sizeof(d_bpair));
    if (!apan || !bpan || !z || !bpair) {
      printf("L1 allocation failed\n");
      return 1;
    }
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    snrt_dma_start_1d(apan, d_apan, sizeof(d_apan));
    snrt_dma_start_1d(bpan, d_bpan, sizeof(d_bpan));
    snrt_dma_start_1d(z, d_zinit, sizeof(d_zinit));  // Y = bias broadcast (initial placement, as O-POPE)
    snrt_dma_start_1d(bpair, d_bpair, sizeof(d_bpair));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    mr_kernel(apan, bpan, z, bpair);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

#if HAS_BIAS
    printf("\n----- P5: GeMM -> ReLU, fused VME -----\n");
#else
    printf("\n----- P5 (no bias): GeMM -> ReLU, fused VME -----\n");
#endif
    printf("The execution took %u cycles.\n", timer);

    errors = check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) printf("Error: %d mismatches\n", errors);
    else printf("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
