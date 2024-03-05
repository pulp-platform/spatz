// Copyright 2022 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

#include "occamy_base_addr.h"
#include "occamy_cfg.h"

// Auto-generated headers
#include "spatz_cluster_peripheral.h"

//===============================================================
// Reggen
//===============================================================

//===============================================================
// Base addresses
//===============================================================

#define cluster_clint_set_base               \
    (QUADRANT_0_CLUSTER_0_PERIPH_BASE_ADDR + \
    SPATZ_CLUSTER_PERIPHERAL_CL_CLINT_SET_REG_OFFSET)

#define cluster_clint_clr_base               \
    (QUADRANT_0_CLUSTER_0_PERIPH_BASE_ADDR + \
    SPATZ_CLUSTER_PERIPHERAL_CL_CLINT_CLEAR_REG_OFFSET)

#define cluster_hw_barrier_base              \
    (QUADRANT_0_CLUSTER_0_PERIPH_BASE_ADDR + \
    SPATZ_CLUSTER_PERIPHERAL_HW_BARRIER_REG_OFFSET)

//===============================================================
// Replicated address spaces
//===============================================================

#define cluster_offset 0x40000

inline uintptr_t translate_address(uintptr_t address, uint32_t instance,
                                   uint32_t offset) {
    return address + instance * offset;
}

inline uintptr_t translate_cluster_address(uintptr_t address,
                                           uint32_t cluster_idx) {
    return translate_address(address, cluster_idx, cluster_offset);
}

//===============================================================
// Derived addresses
//===============================================================

inline uintptr_t cluster_clint_clr_addr(uint32_t cluster_idx) {
    return translate_cluster_address(cluster_clint_clr_base, cluster_idx);
}

inline uintptr_t cluster_clint_set_addr(uint32_t cluster_idx) {
    return translate_cluster_address(cluster_clint_set_base, cluster_idx);
}

inline uintptr_t cluster_tcdm_start_addr(uint32_t cluster_idx) {
    return translate_cluster_address(QUADRANT_0_CLUSTER_0_TCDM_BASE_ADDR,
                                     cluster_idx);
}

inline uintptr_t cluster_tcdm_end_addr(uint32_t cluster_idx) {
    return translate_cluster_address(QUADRANT_0_CLUSTER_0_PERIPH_BASE_ADDR,
                                     cluster_idx);
}

inline uintptr_t cluster_hw_barrier_addr(uint32_t cluster_idx) {
    return translate_cluster_address(cluster_hw_barrier_base, cluster_idx);
}

//===============================================================
// Pointers
//===============================================================

// Don't mark as volatile pointer to favour compiler optimizations.
// Despite in our multicore scenario this value could change unexpectedly
// we can make some assumptions which prevent this.
// Namely, we assume that whenever this register is written to, cores
// synchronize (and execute a memory fence) before reading it. This is usually
// the case, as this register would only be written by CVA6 during
// initialization and never changed.
inline uint32_t* soc_ctrl_scratch_ptr(uint32_t reg_idx) {
    return NULL;
}

inline volatile uint32_t* cluster_clint_clr_ptr(uint32_t cluster_idx) {
    return (volatile uint32_t*)cluster_clint_clr_addr(cluster_idx);
}

inline volatile uint32_t* cluster_clint_set_ptr(uint32_t cluster_idx) {
    return (volatile uint32_t*)cluster_clint_set_addr(cluster_idx);
}

inline volatile uint32_t* cluster_hw_barrier_ptr(uint32_t cluster_idx) {
    return (volatile uint32_t*)cluster_hw_barrier_addr(cluster_idx);
}

inline volatile uint32_t* cluster_zero_memory_ptr(uint32_t cluster_idx) {
    return NULL;
}

inline volatile uint32_t* clint_msip_ptr(uint32_t hartid) {
    return NULL;
}

inline volatile uint32_t* quad_cfg_reset_n_ptr(uint32_t quad_idx) {
    return NULL;
}

inline volatile uint32_t* quad_cfg_clk_ena_ptr(uint32_t quad_idx) {
    return NULL;
}

inline volatile uint32_t* quad_cfg_isolate_ptr(uint32_t quad_idx) {
    return NULL;
}

inline volatile uint32_t* quad_cfg_isolated_ptr(uint32_t quad_idx) {
    return NULL;
}

inline volatile uint32_t* quad_cfg_ro_cache_enable_ptr(uint32_t quad_idx) {
    return NULL;
}

inline volatile uint64_t* quad_cfg_ro_cache_addr_rule_ptr(uint32_t quad_idx,
                                                          uint32_t rule_idx) {
    return NULL;
}
