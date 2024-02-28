// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Could become cluster-local to save storage
// Aligned to 8-byte boundary so we can reuse the multicast function
// based on the SD instruction
__thread volatile uint32_t ct_barrier_cnt __attribute__((aligned(8)));

extern void post_wakeup_cl();

extern comm_buffer_t* get_communication_buffer();

extern uint32_t elect_director(uint32_t num_participants);

extern void return_to_cva6(sync_t sync);