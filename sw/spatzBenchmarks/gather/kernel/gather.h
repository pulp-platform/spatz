// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#ifndef GATHER_H
#define GATHER_H

#include <stdint.h>

// Baseline indexed gather. For each of the G indices, core 0 computes the DRAM
// source address of the selected DIM-element __fp16 row and issues one scattered
// 1D DMA that copies it into `dst` at slot i (G separate DMA requests total).
void gather_baseline(__fp16 *dst, const __fp16 *matrix, const uint32_t *index,
                     uint32_t G, uint32_t DIM);

// Optimized variant: inlined offload, hoisted invariants, running pointers.
void gather_opt(__fp16 *dst, const __fp16 *matrix, const uint32_t *index,
                uint32_t G, uint32_t DIM);

#endif // GATHER_H
