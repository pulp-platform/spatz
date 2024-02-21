// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Luca Colagrande <colluca@iis.ee.ethz.ch>
// Viviane Potocnik <vivianep@iis.ee.ethz.ch>

#include <math.h>

//================================================================================
// Mutex functions
//================================================================================

inline volatile uint32_t *snrt_mutex() { return &_snrt_mutex; }

/**
 * @brief lock a mutex, blocking
 * @details declare mutex with `static volatile uint32_t mtx = 0;`
 */
inline void snrt_mutex_acquire(volatile uint32_t *pmtx) {
    asm volatile(
        "li            t0,1          # t0 = 1\n"
        "1:\n"
        "  amoswap.w.aq  t0,t0,(%0)   # t0 = oldlock & lock = 1\n"
        "  bnez          t0,1b      # Retry if previously set)\n"
        : "+r"(pmtx)
        :
        : "t0");
}

/**
 * @brief lock a mutex, blocking
 * @details test and test-and-set (ttas) implementation of a lock.
 *          Declare mutex with `static volatile uint32_t mtx = 0;`
 */
inline void snrt_mutex_ttas_acquire(volatile uint32_t *pmtx) {
    asm volatile(
        "1:\n"
        "  lw t0, 0(%0)\n"
        "  bnez t0, 1b\n"
        "  li t0,1          # t0 = 1\n"
        "2:\n"
        "  amoswap.w.aq  t0,t0,(%0)   # t0 = oldlock & lock = 1\n"
        "  bnez          t0,2b      # Retry if previously set)\n"
        : "+r"(pmtx)
        :
        : "t0");
}

/**
 * @brief Release the mutex
 */
inline void snrt_mutex_release(volatile uint32_t *pmtx) {
    asm volatile("amoswap.w.rl  x0,x0,(%0)   # Release lock by storing 0\n"
                 : "+r"(pmtx));
}

//================================================================================
// Barrier functions
//================================================================================

/// Synchronize cores in a cluster with a hardware barrier
inline void snrt_cluster_hw_barrier() {
    asm volatile("csrr x0, 0x7C2" ::: "memory");
}

/// Synchronize clusters globally with a global software barrier
inline void snrt_global_barrier() {
    snrt_cluster_hw_barrier();

    // Synchronize all DM cores in software
    if (snrt_is_dm_core()) {
        // Remember previous iteration
        uint32_t prev_barrier_iteration = _snrt_barrier.iteration;
        uint32_t cnt =
            __atomic_add_fetch(&(_snrt_barrier.cnt), 1, __ATOMIC_RELAXED);

        // Increment the barrier counter
        if (cnt == snrt_cluster_num()) {
            _snrt_barrier.cnt = 0;
            __atomic_add_fetch(&(_snrt_barrier.iteration), 1, __ATOMIC_RELAXED);
        } else {
            while (prev_barrier_iteration == _snrt_barrier.iteration)
                ;
        }
    }
    // Synchronize cores in a cluster with the HW barrier
    snrt_cluster_hw_barrier();
}

inline uint32_t snrt_global_all_to_all_reduction(uint32_t value) {
    __atomic_add_fetch(&_reduction_result, value, __ATOMIC_RELAXED);
    snrt_global_barrier();
    return _reduction_result;
}

/**
 * @brief Generic barrier
 *
 * @param barr pointer to a barrier
 * @param n number of harts that have to enter before released
 */
inline void snrt_partial_barrier(snrt_barrier_t *barr, uint32_t n) {
    // Remember previous iteration
    uint32_t prev_it = barr->iteration;
    uint32_t cnt = __atomic_add_fetch(&barr->cnt, 1, __ATOMIC_RELAXED);

    // Increment the barrier counter
    if (cnt == n) {
        barr->cnt = 0;
        __atomic_add_fetch(&barr->iteration, 1, __ATOMIC_RELAXED);
    } else {
        // Some threads have not reached the barrier --> Let's wait
        while (prev_it == barr->iteration)
            ;
    }
}

//================================================================================
// Reduction functions
//================================================================================

// Assumes the dst and src buffers are at the same offset in the TCDM of every
// cluster
inline void snrt_global_reduction_dma(double *dst_buffer, double *src_buffer,
                                      size_t len) {
    // If we have a single cluster the reduction degenerates to a memcpy
    if (snrt_cluster_num() == 1) {
        if (!snrt_is_compute_core()) {
            snrt_dma_start_1d(dst_buffer, src_buffer, len * sizeof(double));
            snrt_dma_wait_all();
        }
        snrt_cluster_hw_barrier();
    } else {
        // Iterate levels in the binary reduction tree
        int num_levels = ceil(log2(snrt_cluster_num()));
        for (unsigned int level = 0; level < num_levels; level++) {
            // Determine whether the current cluster is an active cluster.
            // An active cluster is a cluster that participates in the current
            // level of the reduction tree. Every second cluster among the
            // active ones is a sender.
            uint32_t is_active = (snrt_cluster_idx() % (1 << level)) == 0;
            uint32_t is_sender = (snrt_cluster_idx() % (1 << (level + 1))) != 0;

            // If the cluster is a sender, it sends the data in its source
            // buffer to the respective receiver's destination buffer
            if (is_active && is_sender) {
                if (!snrt_is_compute_core()) {
                    void *dst =
                        (void *)dst_buffer - (1 << level) * SNRT_CLUSTER_OFFSET;
                    snrt_dma_start_1d(dst, src_buffer, len * sizeof(double));
                    snrt_dma_wait_all();
                }
            }

            // Synchronize senders and receivers
            snrt_global_barrier();

            // Every cluster which is not a sender performs the reduction
            if (is_active && !is_sender) {
                // Computation is parallelized over the compute cores
                if (snrt_is_compute_core()) {
                    uint32_t items_per_core =
                        len / snrt_cluster_compute_core_num();
                    uint32_t core_offset =
                        snrt_cluster_core_idx() * items_per_core;
                    for (uint32_t i = 0; i < items_per_core; i++) {
                        uint32_t abs_i = core_offset + i;
                        dst_buffer[abs_i] += src_buffer[abs_i];
                    }
                }
            }

            // Synchronize compute and DM cores for next tree level
            snrt_cluster_hw_barrier();
        }
    }
}
