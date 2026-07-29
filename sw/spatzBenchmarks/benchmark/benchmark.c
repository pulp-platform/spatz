// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.

// SPDX-License-Identifier: Apache-2.0
#include "benchmark.h"
#include "encoding.h"
#include "spatz_cluster_peripheral.h"
#include "team.h"

extern __thread struct snrt_team *_snrt_team_current;

size_t benchmark_get_cycle() { return read_csr(mcycle); }

void start_kernel() {
  uint32_t *bench =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_SPATZ_STATUS_REG_OFFSET);
  *bench = 1;
}

void stop_kernel() {
  uint32_t *bench =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_SPATZ_STATUS_REG_OFFSET);
  *bench = 0;
}

// Write a free-form phase marker. The testbench prints a timestamped
// "[PHASE] <t> ns mark=<value>" line on every change, letting a post-processor
// align DRAM traffic against software phase boundaries. Call from a single core
// (by convention, core 0) to keep markers ordered.
void benchmark_mark(uint32_t value) {
  uint32_t *mark =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_BENCHMARK_MARK_REG_OFFSET);
  *mark = value;
}
