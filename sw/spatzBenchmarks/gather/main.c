// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>
//
// Baseline indexed gather (KV-cache-like). A dense ROWS x DIM fp16 matrix lives
// in DRAM; an index stream of G unique, sorted indices selects the rows to
// gather. Core 0 (with its DMA engine) computes the scattered source addresses
// and issues G separate 1D DMAs to load the selected rows into L1, then checks
// the gathered data bit-for-bit against the matrix rows in DRAM.

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER // selected by CMake via -DDATAHEADER="data/data_gather_...h"
#include "data/layer.h"
#include "kernel/gather.c"

static __fp16 *l1_gather;  // G x DIM gathered rows
static uint32_t *l1_index; // G indices

int main(void) {
  const unsigned int cid = snrt_cluster_core_idx();
  const uint32_t G = gather_l.G;
  const uint32_t DIM = gather_l.DIM;

  // Allocate on-chip buffers.
  if (cid == 0) {
    l1_index = (uint32_t *)snrt_l1alloc(G * sizeof(uint32_t));
    l1_gather = (__fp16 *)snrt_l1alloc((size_t)G * DIM * sizeof(__fp16));
  }

  // Bring the index stream on-chip with one contiguous DMA.
  if (cid == 0) {
    snrt_dma_start_1d(l1_index, gather_index_dram, G * sizeof(uint32_t));
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  // Baseline gather: core 0 issues G scattered 1D DMAs (timed).
  unsigned int timer = (unsigned int)-1;
  if (cid == 0) {
    start_kernel();
    timer = benchmark_get_cycle();
#ifdef GATHER_OPT
    gather_opt(l1_gather, gather_matrix_dram, l1_index, G, DIM);
#else
    gather_baseline(l1_gather, gather_matrix_dram, l1_index, G, DIM);
#endif
    timer = benchmark_get_cycle() - timer;
    stop_kernel();
  }

  snrt_cluster_hw_barrier();

  // Verify against the source rows in DRAM. The DMA is a byte copy, so the
  // gathered data must match the matrix bit-for-bit (compare raw fp16 bits to
  // avoid float promotion / NaN pitfalls).
  if (cid == 0) {
    uint32_t errors = 0;
    const uint16_t *mat_bits = (const uint16_t *)gather_matrix_dram;
    const uint16_t *got_bits = (const uint16_t *)l1_gather;
    for (uint32_t i = 0; i < G; i++) {
      const uint16_t *ref = mat_bits + (size_t)l1_index[i] * DIM;
      const uint16_t *got = got_bits + (size_t)i * DIM;
      for (uint32_t j = 0; j < DIM; j++) {
        if (got[j] != ref[j]) {
          if (errors < 8)
            printf("Mismatch gather[%u][%u]: got 0x%04x exp 0x%04x (row %u)\n", i,
                   j, got[j], ref[j], l1_index[i]);
          errors++;
        }
      }
    }

    printf("\n----- KV gather baseline (%u x %u fp16, G=%u) -----\n",
           gather_l.ROWS, DIM, G);
    printf("The gather took %u cycles (%u scattered DMA requests).\n", timer, G);
    if (errors)
      printf("WRONG! %u mismatches\n", errors);
    else
      printf("CORRECT!\n");
    printf("DONE\n");
  }

  snrt_cluster_hw_barrier();
  return 0;
}
