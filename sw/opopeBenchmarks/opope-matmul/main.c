// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// GEMM (Z = Y + X*W) offloaded to the O-POPE HWPE from a single core.

// Enable snRuntime printf (PRINTF is a no-op otherwise)
#ifndef __SNRT_USE_PRINT
#define __SNRT_USE_PRINT
#endif

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "hal_opope.h"
#include "opope_utils.h"

uint32_t *x;
uint32_t *w;
uint32_t *y;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int x_len = M_SIZE * N_SIZE;
  const unsigned int w_len = N_SIZE * K_SIZE;
  const unsigned int y_len = M_SIZE * K_SIZE;

  const uint8_t comp_fmt = (COMP_FMT == FP8)    ? (uint8_t)Float8
                           : (COMP_FMT == FP16) ? (uint8_t)Float16
                                                : (uint8_t)Float32;
  const uint8_t mem_fmt = (MEM_FMT == FP8)    ? (uint8_t)Float8
                          : (MEM_FMT == FP16) ? (uint8_t)Float16
                                              : (uint8_t)Float32;

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate the matrices in the local tile (O-POPE only accesses the TCDM)
  if (cid == 0) {
    x = (uint32_t *)snrt_l1alloc(x_len * sizeof(uint32_t));
    w = (uint32_t *)snrt_l1alloc(w_len * sizeof(uint32_t));
    y = (uint32_t *)snrt_l1alloc(y_len * sizeof(uint32_t));
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(x, opope_x_dram, x_len * sizeof(uint32_t));
    snrt_dma_start_1d(w, opope_w_dram, w_len * sizeof(uint32_t));
    snrt_dma_start_1d(y, opope_y_dram, y_len * sizeof(uint32_t));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    hwpe_soft_clear();
    while (hwpe_acquire_job() < 0)
      ;

    // O-POPE signals the end of the job with an event routed to the core's external interrupt:
    // enable it in `mie` (global MIE stays off, so the core just resumes after the wfi)
    snrt_interrupt_enable(IRQ_M_EXT);
    opope_cfg((unsigned int)x, (unsigned int)w, (unsigned int)y, M_SIZE, N_SIZE, K_SIZE,
              (uint8_t)OPOPE_GEMM_OP, comp_fmt, mem_fmt);
    hwpe_trigger_job();

    // Sleep until O-POPE is done. The status check only filters spurious wake-ups (and the case
    // in which the event fired before the wfi).
    while (hwpe_get_status() != 0) snrt_wfi();
    snrt_interrupt_disable(IRQ_M_EXT);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2 * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- (%dx%dx%d) O-POPE matmul -----\n", M_SIZE, N_SIZE, K_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle.\n", performance);

    // Check results (O-POPE writes Z in place of Y)
    if (mem_fmt == Float32)
      errors = opope32_compare_int(y, opope_golden, y_len);
    else if (mem_fmt == Float16)
      errors = opope16_compare_int(y, opope_golden, y_len / 2);

    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
