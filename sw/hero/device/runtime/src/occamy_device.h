// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

typedef enum { SYNC_ALL, SYNC_CLUSTERS, SYNC_NONE } sync_t;

// Could become cluster-local to save storage
extern __thread volatile uint32_t ct_barrier_cnt __attribute__((aligned(8)));

inline uint32_t __attribute__((const)) snrt_quadrant_idx() {
    return snrt_cluster_idx() / N_CLUSTERS_PER_QUAD;
}

inline void post_wakeup_cl() { snrt_int_clr_mcip(); }

inline uint32_t elect_director(uint32_t num_participants) {
    uint32_t loser;
    uint32_t prev_val;
    uint32_t winner;

    prev_val = __atomic_fetch_add(snrt_mutex(), 1, __ATOMIC_RELAXED);
    winner = prev_val == (num_participants - 1);

    // last core must reset counter
    if (winner) *snrt_mutex() = 0;

    return winner;
}
