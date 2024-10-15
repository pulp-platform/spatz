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

void l1d_commit() {
  uint32_t *commit =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_L1D_INSN_COMMIT_REG_OFFSET);
  *commit = 1;
}

void l1d_init() {
  uint32_t *insn =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_CFG_L1D_INSN_REG_OFFSET);
  *insn = 3;
  l1d_commit();
}

void l1d_flush() {
  uint32_t *insn =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_CFG_L1D_INSN_REG_OFFSET);
  *insn = 0;
  l1d_commit();
}

void l1d_wait() {
  // uint32_t *busy =
  //     (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
  //                  SPATZ_CLUSTER_PERIPHERAL_CFG_L1D_COMMIT_REG_OFFSET);
  // // wait until flush finished
  // while (*busy) {
  //   *busy =
  //     (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
  //                  SPATZ_CLUSTER_PERIPHERAL_CFG_L1D_COMMIT_REG_OFFSET);
  // }
                   
  volatile uint32_t cyc = 100;
  while (cyc > 0) {
    cyc --;
  }
}
