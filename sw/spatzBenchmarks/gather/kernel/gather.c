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

// Hardware indexed-gather: configure the on-chip gather engine ONCE and launch a
// single transfer. The engine reads the 16-bit index stream from L1 over its TCDM
// port and generates G scattered row reads (src = matrix + idx[i]*stride_src),
// packing them into dst (dst + i*stride_dst). No per-row core loop. The whole
// gather is a single DMA transfer id, drained by one wait_all.
//   idx16 : L1 address of the G 16-bit indices (values must fit in 16 bits)
// stride_src (index scale) = stride_dst (dst pitch) = row_bytes (power of two).
void gather_hw(__fp16 *dst, const __fp16 *matrix, const uint16_t *idx16,
               uint32_t G, uint32_t DIM) {
  const uint32_t row_bytes = DIM * (uint32_t)sizeof(__fp16);
  register uint32_t s_lo  asm("a2") = (uint32_t)matrix;  // DMSRC low  (DRAM base)
  register uint32_t s_hi  asm("a3") = 0;                 // DMSRC high (32-bit addr)
  register uint32_t d_lo  asm("a0") = (uint32_t)dst;     // DMDST low  (L1 base)
  register uint32_t d_hi  asm("a1") = 0;                 // DMDST high
  register uint32_t idx   asm("a5") = (uint32_t)idx16;   // DMIDX index-stream base
  register uint32_t str_s asm("a6") = row_bytes;         // DMSTR stride_src (index scale)
  register uint32_t str_d asm("a7") = row_bytes;         // DMSTR stride_dst (dst pitch)
  register uint32_t reps  asm("t0") = G;                 // DMREP index count
  register uint32_t sz    asm("a4") = row_bytes;         // DMCPYI bytes per gathered row
  register uint32_t txid  asm("a0");                     // DMCPYI returns the transfer id

  // DMSRC a2,a3 : source base (matrix) in DRAM
  asm volatile(".word (0b0000000<<25)|((13)<<20)|((12)<<15)|(0b000<<12)|(0b0101011)\n"
               ::"r"(s_hi), "r"(s_lo));
  // DMDST a0,a1 : destination base in L1
  asm volatile(".word (0b0000001<<25)|((11)<<20)|((10)<<15)|(0b000<<12)|(0b0101011)\n"
               ::"r"(d_hi), "r"(d_lo));
  // DMIDX a5, imm5=0b00001 : index-stream base (a5), element width = 16-bit
  asm volatile(".word (0b0001000<<25)|((0b00001)<<20)|((15)<<15)|(0b000<<12)|(0b0101011)\n"
               ::"r"(idx));
  // DMSTR a6,a7 : stride_src (a6, rs1) = index scale, stride_dst (a7, rs2) = dst pitch
  asm volatile(".word (0b0000110<<25)|((17)<<20)|((16)<<15)|(0b000<<12)|(0b0101011)\n"
               ::"r"(str_s), "r"(str_d));
  // DMREP t0 : index count G
  asm volatile(".word (0b0000111<<25)|(0b00000<<20)|((5)<<15)|(0b000<<12)|(0b0101011)\n"
               ::"r"(reps));
  // DMCPYI a0 <- a4, imm5=cfg : num_bytes=row_bytes, cfg=is_gather(bit2) -> launch
  // (decouple_rw=cfg[0] was measured to make no difference for this gather.)
  asm volatile(".word (0b0000010<<25)|((0b00100)<<20)|((14)<<15)|(0b000<<12)|((10)<<7)|(0b0101011)\n"
               : "=r"(txid)
               : "r"(sz));
  (void)txid;

  snrt_dma_wait_all();
}
