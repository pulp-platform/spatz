// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.

// SPDX-License-Identifier: Apache-2.0

#include "encoding.h"
#include "spatz_cluster_peripheral.h"
#include "team.h"

extern __thread struct snrt_team *_snrt_team_current;

// Used to configure cache xbar's arbitration algorithm
// Size in Bytes
void l1d_xbar_config(uint32_t size, uint32_t core);
void l1d_xbar_commit();

void l1d_commit();
void l1d_init(uint32_t size);
void l1d_flush();
void l1d_wait();
void l1d_spm_config (uint32_t size);

void set_eoc();
