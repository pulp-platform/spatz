// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "snitch_cluster_peripheral.h"

#define CLINT_BASE_ADDR 0x04000000

#define SNRT_CLUSTER_NUM 1
#define SNRT_CLUSTER_CORE_NUM 3
#define SNRT_CLUSTER_DM_CORE_NUM 1
#define SNRT_BASE_HARTID 16

#define SNRT_TCDM_START_ADDR 0x51000000
#define SNRT_CLUSTER_OFFSET 0x40000
#define SNRT_PERIPH_BASE_ADDR 0x51020000
#define SNRT_TCDM_SIZE (SNRT_PERIPH_BASE_ADDR - SNRT_TCDM_START_ADDR)
#define SNRT_LOG2_STACK_SIZE 10
#define SNRT_PERF_COUNTER_ADDR (SNRT_PERIPH_BASE_ADDR + 0x0)
