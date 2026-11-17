// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <printf.h>
#include <snrt.h>
#include <stdint.h>

#include DATAHEADER
#include "kernel/widening-fmatmul.h"

static inline uint32_t get_cycle(void) {
  uint32_t cycle;
  asm volatile("csrr %0, mcycle" : "=r"(cycle) :: "memory");
  return cycle;
}

static uint32_t float_bits(float value) {
  union {
    float f;
    uint32_t u;
  } bits = {.f = value};
  return bits.u;
}

static float float_from_bits(uint32_t value) {
  union {
    float f;
    uint32_t u;
  } bits = {.u = value};
  return bits.f;
}

int main(void) {
  if (snrt_cluster_core_idx() != 0)
    return 0;

  enum {
    A_BYTES = sizeof(gemm_Apack_dram),
    B_BYTES = sizeof(gemm_Bpack_dram),
    C_BYTES = sizeof(gemm_result_bits),
    BUFFER_BYTES = A_BYTES + B_BYTES + C_BYTES,
  };
  _Static_assert(BUFFER_BYTES <= SNRT_TCDM_SIZE,
                 "four-tile widening MatMul buffers exceed TCDM");

  uint8_t *const buffer = (uint8_t *)snrt_l1alloc(BUFFER_BYTES);
  if (buffer == 0)
    return 2;
  uint32_t *const apack = (uint32_t *)buffer;
  uint32_t *const bpack = (uint32_t *)(buffer + A_BYTES);
  float *const c = (float *)(buffer + A_BYTES + B_BYTES);

  snrt_dma_start_1d(apack, gemm_Apack_dram, A_BYTES);
  snrt_dma_start_1d(bpack, gemm_Bpack_dram, B_BYTES);
  snrt_dma_wait_all();

  const uint32_t begin = get_cycle();
#if WIDENING_4TILE_REUSE
  const int kernel_status = vme_widening_hp_fmatmul_4tile_reuse(
      c, apack, bpack, gemm_l.M, gemm_l.N, gemm_l.K);
  const char *const variant = "simultaneous-reduction/A+B-reuse";
#else
  const int kernel_status = vme_widening_hp_fmatmul_4tile_pingpong(
      c, apack, bpack, gemm_l.M, gemm_l.N, gemm_l.K);
  const char *const variant = "pair-compute-store-ping-pong";
#endif
  const uint32_t cycles = get_cycle() - begin;
  if (kernel_status != 0) {
    printf("Unsupported four-tile shape M%u_N%u_K%u\n", gemm_l.M,
           gemm_l.N, gemm_l.K);
    return 2;
  }

  uint32_t errors = 0;
  for (uint32_t i = 0; i < gemm_l.M * gemm_l.N; ++i) {
    const float got = c[i];
    const float ref = float_from_bits(gemm_result_bits[i]);
    const float delta = got > ref ? got - ref : ref - got;
    const float abs_ref = ref < 0.0f ? -ref : ref;
    if (delta > 0.001f + 0.001f * abs_ref) {
      if (errors < 8)
        printf("mismatch %u: got=0x%08x ref=0x%08x\n", i,
               float_bits(got), float_bits(ref));
      ++errors;
    }
  }

  printf("\n=== VME widening FP16->FP32 MatMul: four tiles %s, "
         "M%u_N%u_K%u ===\n", variant, gemm_l.M, gemm_l.N, gemm_l.K);
  printf("Cycles: %u\nErrors: %u\nStatus: %s\n", cycles, errors,
         errors ? "FAIL" : "PASS");
  return errors ? 1 : 0;
}
