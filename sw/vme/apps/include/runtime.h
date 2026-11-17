// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifndef __SNRT_USE_PRINT
#define __SNRT_USE_PRINT
#endif
#include <stdint.h>
#include <snrt.h>
#include <debug.h>

#define L1_ALLOC(ptr, type, count) do {                          \
  (ptr) = (type *)snrt_l1alloc((count) * sizeof(type));              \
  if (!(ptr)) { PRINTF("L1 allocation failed\n"); return -1; }     \
} while (0)
#define DMA(ptr, arr) snrt_dma_start_1d((ptr), (arr), sizeof(arr))
#define benchmark_get_cycle get_cycle
#define start_kernel() ((void)0)
#define stop_kernel() ((void)0)

static inline unsigned get_cycle(void) {
  unsigned c;
  asm volatile("csrr %0, mcycle" : "=r"(c) :: "memory");
  return c;
}

// Wait until prior vector stores to this buffer are visible before a
// producer/consumer phase reuses it.
static inline void wait_for_vector_store(const void *ptr) {
  (void)*(volatile const uint32_t *)ptr;
}

static inline int transformer_check_fp32(const float *actual, const float *golden,
                                  unsigned count, float rel_tol, float abs_tol) {
  int errors = 0;
  for (unsigned i = 0; i < count; ++i) {
    float diff = actual[i] - golden[i];
    if (diff < 0) diff = -diff;
    float mag = golden[i] < 0 ? -golden[i] : golden[i];
    if (!(diff <= abs_tol + rel_tol * mag)) {
      if (errors < 8)
        PRINTF("Mismatch @ %u: got 0x%x, expected 0x%x\n", i,
               ((const uint32_t *)actual)[i], ((const uint32_t *)golden)[i]);
      ++errors;
    }
  }
  return errors;
}

static inline void transformer_vzero(float *dst, unsigned n) {
  while (n) {
    unsigned vl;
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vmv.v.i v0, 0\nvse32.v v0, (%0)" :: "r"(dst) : "memory");
    dst += vl;
    n -= vl;
  }
}
