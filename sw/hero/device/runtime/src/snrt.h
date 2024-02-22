// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef SNRT_H
#define SNRT_H

#include <stddef.h>
#include <stdint.h>

// Occamy specific definitions
#include "occamy_defs.h"

// Forward declarations
#include "alloc_decls.h"
#include "cls_decls.h"
#include "sync_decls.h"
#include "team_decls.h"
#include "riscv_decls.h"
#include "memory_decls.h"
#include "global_interrupt_decls.h"

// Implementation
#include "alloc.h"
#include "cls.h"
#include "cluster_interrupts.h"
#include "dm.h"
#include "global_interrupts.h"
#include "occamy_memory.h"
#include "eu.h"
#include "kmp.h"
#include "omp.h"
#include "perf_cnt.h"
#include "riscv.h"
#include "sync.h"
#include "team.h"


#endif  // SNRT_H
