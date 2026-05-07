// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "redundancy_modules.h"
#include "snrt.h"

// Peripheral base = end of TCDM (cluster scratchpad).
uintptr_t err_monitor_peripheral_base(void) {
    return (uintptr_t)snrt_cluster_memory().end;
}

static inline volatile uint32_t *reg32(uint32_t offset) {
    return (volatile uint32_t *)(err_monitor_peripheral_base() + offset);
}

void err_monitor_clear(void) {
    *reg32(SPATZ_CLUSTER_PERIPHERAL_ERR_MONITOR_CLEAR_REG_OFFSET) = 1u;
}

void err_monitor_resume(void) {
    *reg32(SPATZ_CLUSTER_PERIPHERAL_ERR_MONITOR_CLEAR_REG_OFFSET) = 0u;
}

uint32_t err_monitor_vrf_correctable(uint32_t unit) {
    uint32_t offset = SPATZ_CLUSTER_PERIPHERAL_VRF_CORRECTABLE_COUNT_REG_OFFSET
                      + unit * 8u;
    return *reg32(offset);
}

uint32_t err_monitor_vrf_uncorrectable(uint32_t unit) {
    uint32_t offset = SPATZ_CLUSTER_PERIPHERAL_VRF_UNCORRECTABLE_COUNT_REG_OFFSET
                      + unit * 8u;
    return *reg32(offset);
}

uint32_t err_monitor_tcdm_rd_correctable(uint32_t bank) {
    uint32_t offset = SPATZ_CLUSTER_PERIPHERAL_TCDM_RD_CORRECTABLE_COUNT_0_REG_OFFSET
                      + bank * 8u;
    return *reg32(offset);
}

uint32_t err_monitor_tcdm_rd_uncorrectable(uint32_t bank) {
    uint32_t offset = SPATZ_CLUSTER_PERIPHERAL_TCDM_RD_UNCORRECTABLE_COUNT_0_REG_OFFSET
                      + bank * 8u;
    return *reg32(offset);
}

uint32_t err_monitor_tcdm_scrub_correctable(uint32_t bank) {
    uint32_t offset = SPATZ_CLUSTER_PERIPHERAL_TCDM_SCRUB_CORRECTABLE_COUNT_0_REG_OFFSET
                      + bank * 8u;
    return *reg32(offset);
}

uint32_t err_monitor_tcdm_scrub_uncorrectable(uint32_t bank) {
    uint32_t offset = SPATZ_CLUSTER_PERIPHERAL_TCDM_SCRUB_UNCORRECTABLE_COUNT_0_REG_OFFSET
                      + bank * 8u;
    return *reg32(offset);
}
