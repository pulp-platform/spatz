// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "snrt.h"

uint32_t cluster_global_to_local_address(uint32_t global_addr) {
    return global_addr - snrt_l1_start_addr() + ALIAS_TCDM_BASE_ADDR;
}

const uint32_t n_inputs = 16;
volatile int errors = 2 * n_inputs;

int main() {
    // Get global and local memory aliases
    volatile uint32_t *buffer_global = snrt_l1_next();
    volatile uint32_t *buffer_local =
        (uint32_t *)cluster_global_to_local_address((uint32_t)buffer_global);

    // Test narrow cluster XBAR
    if (snrt_cluster_core_idx() == 0) {
        // Write to global buffer
        for (uint32_t i = 0; i < n_inputs; i++) buffer_global[i] = i;
        // Read from local buffer
        for (uint32_t i = 0; i < n_inputs; i++)
            if (buffer_local[i] == i) errors--;
    }

    snrt_cluster_hw_barrier();

    // Test wide DMA XBAR
    if (snrt_is_dm_core()) {
        // Read from local buffer using DMA
        buffer_global += n_inputs;
        snrt_dma_start_1d((void *)buffer_global, (void *)buffer_local,
                          n_inputs * sizeof(uint32_t));
        snrt_dma_wait_all();
        // Check results
        for (uint32_t i = 0; i < n_inputs; i++)
            if (buffer_global[i] == i) errors--;
    }

    snrt_cluster_hw_barrier();

    return errors;
}
