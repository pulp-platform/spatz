// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.

// SPDX-License-Identifier: Apache-2.0

#include <l1cache.h>

void l1d_xbar_config(uint32_t size, uint32_t core) {
  uint32_t offset = 31 - __builtin_clz(size/core);

  // 5 is the cacheline width (log2(256b/8))
  // granularity cannot be less than cacheline width
  offset = (offset > 5) ? offset : 5;

  uint32_t *cfg =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_XBAR_OFFSET_REG_OFFSET);
  *cfg = offset;
  l1d_xbar_commit();
}


void l1d_xbar_commit() {
  uint32_t *commit =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_XBAR_OFFSET_COMMIT_REG_OFFSET);
  *commit = 1;
}

void l1d_commit() {
  uint32_t *commit =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_L1D_INSN_COMMIT_REG_OFFSET);
  *commit = 1;
}

void l1d_init(uint32_t size) {
  uint32_t *insn =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_CFG_L1D_INSN_REG_OFFSET);
  *insn = 3;
  l1d_commit();
  l1d_wait();
  // Write in the default config immediately after initialization
  // No need to call outside unless need a different config
  l1d_spm_config(size);
}

void l1d_flush() {
  uint32_t *insn =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_CFG_L1D_INSN_REG_OFFSET);
  *insn = 0;
  l1d_commit();
}

void l1d_wait() {
  volatile uint32_t *busy =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_L1D_FLUSH_STATUS_REG_OFFSET);
  // wait until flush finished
  while (*busy) {

  }
}

void l1d_spm_config (uint32_t size) {
  // flush the cache before reconfiguration
  l1d_flush();
  l1d_wait();
  // free all allocated region
  snrt_l1alloc_reset();
  // set the pointers
  volatile uint32_t *cfg_size =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_CFG_L1D_SPM_REG_OFFSET);
  volatile uint32_t *commit =
      (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                   SPATZ_CLUSTER_PERIPHERAL_L1D_SPM_COMMIT_REG_OFFSET);
  // Make sure dummy region will not be optimized away
  volatile double *dummy;
  // Should be (L1_size - size) * 128
  int cache_region = (128 - size) * 128;
  dummy = (volatile double *)snrt_l1alloc(cache_region * sizeof(double));
  // change size and commit the change
  *cfg_size = size;
  *commit   = 1;
}

void set_eoc () {
    volatile uint32_t *eoc_reg =
    (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                SPATZ_CLUSTER_PERIPHERAL_CLUSTER_EOC_EXIT_REG_OFFSET);
    *eoc_reg = 1;
}