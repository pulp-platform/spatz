// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

extern __thread uint32_t _snrt_cluster_hw_barrier;

inline uint32_t __attribute__((const)) snrt_l1_start_addr() {
    return SNRT_TCDM_START_ADDR;
}

inline uint32_t __attribute__((const)) snrt_l1_end_addr() {
    return SNRT_PERIPH_BASE_ADDR;
}

inline volatile uint32_t* __attribute__((const)) snrt_clint_mutex_ptr() {
    // TODO CHANGEME
    return snrt_mutex();
}

inline volatile uint32_t* __attribute__((const)) snrt_cluster_clint_set_ptr() {
    return (uint32_t*)(SNRT_PERIPH_BASE_ADDR + 0x30);
}

inline volatile uint32_t* __attribute__((const)) snrt_cluster_clint_clr_ptr() {
    return (uint32_t*)(SNRT_PERIPH_BASE_ADDR + 0x38);
}

inline volatile uint32_t* __attribute__((const))
snrt_clint_msip_ptr(uint32_t hartid) {
    return (uint32_t *) CLINT_BASE_ADDR + 0x0;
}

inline uint32_t __attribute__((const)) snrt_cluster_hw_barrier_addr() {
    return _snrt_cluster_hw_barrier;
}

inline uint32_t __attribute__((const)) snrt_cluster_perf_counters_addr() {
    return SNRT_PERF_COUNTER_ADDR;
}

inline volatile uint32_t* __attribute__((const)) snrt_zero_memory_ptr() {
    return NULL;
}

static inline void snrt_exit(int exit_code) {
}
