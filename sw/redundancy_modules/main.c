// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Error monitor peripheral functional tests.
//
// t1: All counter registers and ERR_MONITOR_CLEAR read as 0 at reset.
// t2: ERR_MONITOR_CLEAR write-readback: write 1 reads back 1, write 0 reads back 0.
// t3: All counters read as 0 after an explicit software clear.
// t4: Clean TCDM write+read cycle produces no fault counts.

#include <snrt.h>
#include "printf.h"

#include "include/redundancy_modules.h"

#define REG32(base, off) (*(volatile uint32_t *)((uintptr_t)(base) + (off)))

static volatile uint32_t scratch[64];

// -------------------------------------------------------------------------
// t1: reset values
// -------------------------------------------------------------------------
static int test_reset_values(void) {
  int err = 0;
  uintptr_t base = err_monitor_peripheral_base();

  uint32_t clr = REG32(base, SPATZ_CLUSTER_PERIPHERAL_ERR_MONITOR_CLEAR_REG_OFFSET);
  if (clr != 0) {
    printf("FAIL t1: ERR_MONITOR_CLEAR reset = 0x%x (expected 0)\n", clr);
    err = -1;
  }

  for (uint32_t u = 0; u < ERR_MONITOR_NUM_VRF_UNITS; u++) {
    uint32_t v;
    v = err_monitor_vrf_correctable(u);
    if (v != 0) { printf("FAIL t1: vrf_corr[%u] = 0x%x\n", u, v); err = -1; }
    v = err_monitor_vrf_uncorrectable(u);
    if (v != 0) { printf("FAIL t1: vrf_uncorr[%u] = 0x%x\n", u, v); err = -1; }
  }

  for (uint32_t b = 0; b < ERR_MONITOR_NUM_TCDM_BANKS; b++) {
    uint32_t v;
    v = err_monitor_tcdm_rd_correctable(b);
    if (v != 0) { printf("FAIL t1: tcdm_rd_corr[%u] = 0x%x\n", b, v); err = -1; }
    v = err_monitor_tcdm_rd_uncorrectable(b);
    if (v != 0) { printf("FAIL t1: tcdm_rd_uncorr[%u] = 0x%x\n", b, v); err = -1; }
    v = err_monitor_tcdm_scrub_correctable(b);
    if (v != 0) { printf("FAIL t1: tcdm_scrub_corr[%u] = 0x%x\n", b, v); err = -1; }
    v = err_monitor_tcdm_scrub_uncorrectable(b);
    if (v != 0) { printf("FAIL t1: tcdm_scrub_uncorr[%u] = 0x%x\n", b, v); err = -1; }
  }

  if (!err) printf("PASS t1: all registers at reset value\n");
  return err;
}

// -------------------------------------------------------------------------
// t2: ERR_MONITOR_CLEAR write-readback
// -------------------------------------------------------------------------
static int test_clear_readback(void) {
  int err = 0;
  uintptr_t base = err_monitor_peripheral_base();

  err_monitor_clear();
  uint32_t v = REG32(base, SPATZ_CLUSTER_PERIPHERAL_ERR_MONITOR_CLEAR_REG_OFFSET);
  if (v != 1) {
    printf("FAIL t2: clear=1 readback = 0x%x (expected 1)\n", v);
    err = -1;
  }

  err_monitor_resume();
  v = REG32(base, SPATZ_CLUSTER_PERIPHERAL_ERR_MONITOR_CLEAR_REG_OFFSET);
  if (v != 0) {
    printf("FAIL t2: clear=0 readback = 0x%x (expected 0)\n", v);
    err = -1;
  }

  if (!err) printf("PASS t2: ERR_MONITOR_CLEAR write-readback\n");
  return err;
}

// -------------------------------------------------------------------------
// t3: all counters zero after explicit software clear
// -------------------------------------------------------------------------
static int test_counters_after_clear(void) {
  int err = 0;

  err_monitor_clear();
  err_monitor_resume();

  for (uint32_t u = 0; u < ERR_MONITOR_NUM_VRF_UNITS; u++) {
    uint32_t v;
    v = err_monitor_vrf_correctable(u);
    if (v != 0) { printf("FAIL t3: vrf_corr[%u] = 0x%x after clear\n", u, v); err = -1; }
    v = err_monitor_vrf_uncorrectable(u);
    if (v != 0) { printf("FAIL t3: vrf_uncorr[%u] = 0x%x after clear\n", u, v); err = -1; }
  }

  for (uint32_t b = 0; b < ERR_MONITOR_NUM_TCDM_BANKS; b++) {
    uint32_t v;
    v = err_monitor_tcdm_rd_correctable(b);
    if (v != 0) { printf("FAIL t3: tcdm_rd_corr[%u] = 0x%x after clear\n", b, v); err = -1; }
    v = err_monitor_tcdm_rd_uncorrectable(b);
    if (v != 0) { printf("FAIL t3: tcdm_rd_uncorr[%u] = 0x%x after clear\n", b, v); err = -1; }
    v = err_monitor_tcdm_scrub_correctable(b);
    if (v != 0) { printf("FAIL t3: tcdm_scrub_corr[%u] = 0x%x after clear\n", b, v); err = -1; }
    v = err_monitor_tcdm_scrub_uncorrectable(b);
    if (v != 0) { printf("FAIL t3: tcdm_scrub_uncorr[%u] = 0x%x after clear\n", b, v); err = -1; }
  }

  if (!err) printf("PASS t3: all counters zero after software clear\n");
  return err;
}

// -------------------------------------------------------------------------
// t4: clean TCDM write+read produces no fault counts
// -------------------------------------------------------------------------
static int test_clean_tcdm_access(void) {
  int err = 0;

  // Write first so the SRAM ECC check bits are consistent with the data.
  for (uint32_t i = 0; i < 64; i++)
    scratch[i] = (uint32_t)(0xA5A5A500u | i);

  // Clear counters, then read back the buffer.
  err_monitor_clear();
  err_monitor_resume();

  for (uint32_t i = 0; i < 64; i++) {
    uint32_t expected = (uint32_t)(0xA5A5A500u | i);
    if (scratch[i] != expected) {
      printf("FAIL t4: scratch[%u] = 0x%x (expected 0x%x)\n", i, scratch[i], expected);
      err = -1;
    }
  }

  // No TCDM read-path faults should have been counted.
  for (uint32_t b = 0; b < ERR_MONITOR_NUM_TCDM_BANKS; b++) {
    uint32_t v;
    v = err_monitor_tcdm_rd_correctable(b);
    if (v != 0) { printf("FAIL t4: tcdm_rd_corr[%u] = 0x%x after clean access\n", b, v); err = -1; }
    v = err_monitor_tcdm_rd_uncorrectable(b);
    if (v != 0) { printf("FAIL t4: tcdm_rd_uncorr[%u] = 0x%x after clean access\n", b, v); err = -1; }
  }

  if (!err) printf("PASS t4: clean TCDM access, no fault counts\n");
  return err;
}

// -------------------------------------------------------------------------

int main() {
  const unsigned int cid = snrt_cluster_core_idx();
  int err = 0;

  if (cid == 0) {
    err |= test_reset_values();
    err |= test_clear_readback();
    err |= test_counters_after_clear();
    err |= test_clean_tcdm_access();

    if (err)
      printf("OVERALL: FAIL\n");
    else
      printf("OVERALL: PASS\n");
  }

  snrt_cluster_hw_barrier();
  return err;
}
