// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.

// SPDX-License-Identifier: Apache-2.0
#include "benchmark.h"
#include "encoding.h"
#include "spatz_cluster_peripheral.h"
#include "team.h"
#include "perf_cnt.h"

extern __thread struct snrt_team *_snrt_team_current;

size_t benchmark_get_cycle() { return read_csr(mcycle); }

void start_kernel() {
  uint32_t *bench =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_SPATZ_STATUS_REG_OFFSET);
  *bench = 1;
  snrt_start_perf_counter(SNRT_PERF_CNT0, SNRT_PERF_CNT_CYCLES, 0);
}

void stop_kernel() {
  snrt_stop_perf_counter(SNRT_PERF_CNT0);
  uint32_t *bench =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_SPATZ_STATUS_REG_OFFSET);
  *bench = 0;
}


size_t get_perf() {
  volatile uint32_t *perf =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_PERF_COUNTER_0_REG_OFFSET);
  // There is a constant delay of using performance counter for cycle recording
  // substract the constant delay
  return (*perf-60);
}
