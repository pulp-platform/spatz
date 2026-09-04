// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.

// SPDX-License-Identifier: Apache-2.0

#include <l1cache.h>

#ifdef HPDCACHE
// HPDcache variant: no cache_sync_* side-channel/peripheral registers exist
// (see hpd_spatz_cache_ctrl.sv) -- HPDcache takes CMOs as ordinary op-coded
// requests on the normal core request channel. We trigger this with a
// single store to a reserved "magic" address (the first 8-byte chunk of
// cluster_mem, reserved by snrt_alloc_init under #ifdef HPDCACHE in
// alloc.c), whose *data value* encodes which CMO -- decoded on the RTL side
// in hpd_spatz_cache_ctrl.sv. This mirrors InSitu's old cache_sync_insn_i
// encoding so the mapping stays familiar:
//   0 -> flush + invalidate all   1 -> flush all
//   2 -> invalidate all           3 -> invalidate all (closest to InSitu's
//                                       "full tag init", which HPDcache has
//                                       no direct equivalent for)
// The store's own completion (need_rsp=1 on the underlying HPDcache
// request) is what gates readiness -- l1d_commit()/l1d_wait() become no-ops
// since there's nothing left to separately trigger or poll.
#define HPD_CACHE_INSN_FLUSH_INVAL_ALL 0u
#define HPD_CACHE_INSN_FLUSH_ALL 1u
#define HPD_CACHE_INSN_INVAL_ALL 2u
#define HPD_CACHE_INSN_FULL_INIT 3u

static inline void hpd_cache_insn(uint32_t insn) {
    volatile uint32_t *trigger =
        (volatile uint32_t *)(_snrt_team_current->root->cluster_mem.start);
    *trigger = insn;
}

void l1d_commit() {
    // No-op: the triggering store in l1d_init()/l1d_flush() already blocks
    // on its own response, nothing left to separately commit.
}

void l1d_init(uint32_t size) {
    hpd_cache_insn(HPD_CACHE_INSN_FULL_INIT);
    // Write in the default config immediately after initialization
    // No need to call outside unless need a different config
    l1d_spm_config(size);
}

void l1d_flush() { hpd_cache_insn(HPD_CACHE_INSN_FLUSH_INVAL_ALL); }

void l1d_wait() {
    // No-op: see l1d_commit().
}

#else  // !HPDCACHE -- InSitu-Cache, unchanged

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

#endif  // HPDCACHE

// Shared between both variants -- only touches the SPM-size peripheral
// registers (l1d_spm_size_o/cfg_spm_size), which are unaffected by the
// HPDcache variant's l1d_insn_* changes above. Calls l1d_flush()/l1d_wait(),
// which resolve to whichever variant's implementation above was compiled.
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
    // volatile uint32_t *eoc_reg =
    // (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
    //             SPATZ_CLUSTER_PERIPHERAL_CLUSTER_EOC_EXIT_REG_OFFSET);
    // *eoc_reg = 1;
}

// eoc_and_return_code[31:0] = {return_code[30:0], eoc};
void set_eoc_and_return_code (int eoc_and_return_code) {
    volatile uint32_t *eoc_reg =
    (uint32_t *)(_snrt_team_current->root->cluster_mem.end +
                SPATZ_CLUSTER_PERIPHERAL_CLUSTER_EOC_EXIT_REG_OFFSET);
    if (eoc_and_return_code != -1)
      *eoc_reg = eoc_and_return_code + 1;
    else
      *eoc_reg = eoc_and_return_code;
}
