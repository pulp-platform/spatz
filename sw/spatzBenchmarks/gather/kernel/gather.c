// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#include <snrt.h>
#include <stdint.h>

#include "gather.h"

void gather_baseline(__fp16 *dst, const __fp16 *matrix, const uint32_t *index,
                     uint32_t G, uint32_t DIM) {
  const size_t row_bytes = (size_t)DIM * sizeof(__fp16);
  // The core drives the whole gather: for each index it computes the row's DRAM
  // source address and launches a 1D DMA. The DMA request FIFO back-pressures,
  // so the core blocks naturally if it runs ahead of the engine; a single
  // wait_all at the end drains all G transfers.
  for (uint32_t i = 0; i < G; i++) {
    const __fp16 *src = matrix + (size_t)index[i] * DIM;
    snrt_dma_start_1d(dst + (size_t)i * DIM, src, row_bytes);
  }
  snrt_dma_wait_all();
}

// Inlined single offload of one 1D transfer: emits dmsrc/dmdst/dmcpyi directly
// (no function call). 32-bit addresses -> high halves are 0. Encodings match
// snRuntime's snrt_dma_start_1d_wideptr.
__attribute__((always_inline)) static inline void
dma_issue_1d(uint32_t dst, uint32_t src, uint32_t size) {
  register uint32_t s_lo asm("a2") = src;
  register uint32_t s_hi asm("a3") = 0;
  register uint32_t d_lo asm("a0") = dst;
  register uint32_t d_hi asm("a1") = 0;
  register uint32_t sz asm("a4") = size;
  register uint32_t txid asm("a0");
  asm volatile(".word (0b0000000<<25)|((13)<<20)|((12)<<15)|(0b000<<12)|"
               "(0b0101011<<0)\n" ::"r"(s_hi), "r"(s_lo));
  asm volatile(".word (0b0000001<<25)|((11)<<20)|((10)<<15)|(0b000<<12)|"
               "(0b0101011<<0)\n" ::"r"(d_hi), "r"(d_lo));
  asm volatile(".word (0b0000010<<25)|(0b00000<<20)|((14)<<15)|(0b000<<12)|"
               "((10)<<7)|(0b0101011<<0)\n"
               : "=r"(txid)
               : "r"(sz));
  (void)txid;
}

// Optimized baseline: same 128 scattered 1D DMAs, but with the offload inlined
// and loop invariants hoisted (size constant, dst/src via running pointers).
// Isolates how much of the per-access cost is pure software overhead vs the
// irreducible address-gen + 3 offload instructions.
void gather_opt(__fp16 *dst, const __fp16 *matrix, const uint32_t *index,
                uint32_t G, uint32_t DIM) {
  const uint32_t row_bytes = DIM * (uint32_t)sizeof(__fp16);
  uint32_t d = (uint32_t)dst;
  for (uint32_t i = 0; i < G; i++) {
    uint32_t src = (uint32_t)matrix + index[i] * row_bytes;
    dma_issue_1d(d, src, row_bytes);
    d += row_bytes;
  }
  snrt_dma_wait_all();
}
