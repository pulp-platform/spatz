// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>
#include "spatz_cluster_peripheral.h"

#define ERR_MONITOR_NUM_VRF_UNITS  SPATZ_CLUSTER_PERIPHERAL_PARAM_NUM_VRF_UNITS
#define ERR_MONITOR_NUM_TCDM_BANKS SPATZ_CLUSTER_PERIPHERAL_PARAM_NUM_TCDM_BANKS

// Returns the peripheral base address (TCDM end = peripheral start).
uintptr_t err_monitor_peripheral_base(void);

// Assert the clear signal (hold to keep counters at zero).
void err_monitor_clear(void);

// Deassert the clear signal (resume counting).
void err_monitor_resume(void);

// Read a VRF correctable-error counter (unit index 0..NUM_VRF_UNITS-1).
uint32_t err_monitor_vrf_correctable(uint32_t unit);

// Read a VRF uncorrectable-error counter.
uint32_t err_monitor_vrf_uncorrectable(uint32_t unit);

// Read a TCDM read-path correctable-error counter (bank 0..NUM_TCDM_BANKS-1).
uint32_t err_monitor_tcdm_rd_correctable(uint32_t bank);

// Read a TCDM read-path uncorrectable-error counter.
uint32_t err_monitor_tcdm_rd_uncorrectable(uint32_t bank);

// Read a TCDM scrub-path correctable-error counter.
uint32_t err_monitor_tcdm_scrub_correctable(uint32_t bank);

// Read a TCDM scrub-path uncorrectable-error counter.
uint32_t err_monitor_tcdm_scrub_uncorrectable(uint32_t bank);
