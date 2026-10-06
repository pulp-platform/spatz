// Copyright 2026 University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "vector_macros.h"

#include <stdint.h>

#define DIMC_INSN(vd, vs1, ci, kernel_group, imm, funct3) \
  (((0x2e) << 26) | ((imm) << 25) | ((kernel_group) << 23) | \
   ((ci) << 20) | ((vs1) << 15) | ((funct3) << 12) | ((vd) << 7) | 0x77)

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define SF_VQMMACC(vd, kernel_group, vs1, ci, imm) \
  asm volatile(".word " STR(DIMC_INSN(vd, vs1, ci, kernel_group, imm, 0)) \
               ::: "v16", "memory")

static void load_kernel_rows(uint8_t *kernel) {
  asm volatile("vle8.v v0, (%0)" :: "r"(kernel + 0 * 64) : "memory");
  asm volatile("vle8.v v1, (%0)" :: "r"(kernel + 1 * 64) : "memory");
  asm volatile("vle8.v v2, (%0)" :: "r"(kernel + 2 * 64) : "memory");
  asm volatile("vle8.v v3, (%0)" :: "r"(kernel + 3 * 64) : "memory");
  asm volatile("vle8.v v4, (%0)" :: "r"(kernel + 4 * 64) : "memory");
  asm volatile("vle8.v v5, (%0)" :: "r"(kernel + 5 * 64) : "memory");
  asm volatile("vle8.v v6, (%0)" :: "r"(kernel + 6 * 64) : "memory");
  asm volatile("vle8.v v7, (%0)" :: "r"(kernel + 7 * 64) : "memory");
  asm volatile("vle8.v v8, (%0)" :: "r"(kernel + 8 * 64) : "memory");
  asm volatile("vle8.v v9, (%0)" :: "r"(kernel + 9 * 64) : "memory");
  asm volatile("vle8.v v10, (%0)" :: "r"(kernel + 10 * 64) : "memory");
  asm volatile("vle8.v v11, (%0)" :: "r"(kernel + 11 * 64) : "memory");
  asm volatile("vle8.v v12, (%0)" :: "r"(kernel + 12 * 64) : "memory");
  asm volatile("vle8.v v13, (%0)" :: "r"(kernel + 13 * 64) : "memory");
  asm volatile("vle8.v v14, (%0)" :: "r"(kernel + 14 * 64) : "memory");
  asm volatile("vle8.v v15, (%0)" :: "r"(kernel + 15 * 64) : "memory");
}

void TEST_CASE1(void) {
  uint8_t *feature = (uint8_t *)snrt_l1alloc(64);
  uint8_t *kernel = (uint8_t *)snrt_l1alloc(16 * 64);
  uint32_t *result = (uint32_t *)snrt_l1alloc(16 * sizeof(uint32_t));
  if (feature == 0 || kernel == 0 || result == 0) {
    printf("sf.vqmmacc allocation failed\n");
    num_failed++;
    return;
  }

  for (uint32_t i = 0; i < 64; i++) {
    feature[i] = 1;
  }
  for (uint32_t row = 0; row < 16; row++) {
    for (uint32_t col = 0; col < 64; col++) {
      kernel[row * 64 + col] = row + 1;
    }
  }
  for (uint32_t i = 0; i < 16; i++) {
    result[i] = 0;
  }

  uint32_t vl;
  asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(64));
  asm volatile("vmv.v.i v16, 0" ::: "v16", "memory");
  load_kernel_rows(kernel);
  asm volatile("vle8.v v31, (%0)" :: "r"(feature) : "v31", "memory");

  uint32_t kernel_load = 1;
  uint32_t feature_reuse = 0;
  uint32_t compute_reuse = 0;
  asm volatile("csrw 0x7D3, %0" :: "r"(kernel_load) : "memory");
  asm volatile("csrw 0x7D4, %0" :: "r"(feature_reuse) : "memory");
  asm volatile("csrw 0x7D5, %0" :: "r"(compute_reuse) : "memory");

  SF_VQMMACC(16, 0, 31, 3, 0);
  // A read must preserve the one-shot kernel-load sequence between the pair.
  uint32_t kernel_load_mid;
  asm volatile("csrr %0, 0x7D3" : "=r"(kernel_load_mid) :: "memory");
  if (kernel_load_mid != 1) {
    printf("[TC 1] kernel-load CSR changed before the second instruction.\n");
    num_failed++;
    return;
  }
  SF_VQMMACC(16, 1, 31, 7, 0);
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));
  asm volatile("vse32.v v16, (%0)" :: "r"(result) : "memory");

  uint32_t kernel_load_after;
  asm volatile("csrr %0, 0x7D3" : "=r"(kernel_load_after) :: "memory");
  if (kernel_load_after != 0) {
    printf("[TC 1] dimc_kernel CSR FAILED. Got %u, expected 0.\n", kernel_load_after);
    num_failed++;
    return;
  }

  for (uint32_t row = 0; row < 16; row++) {
    uint32_t expected = 64 * (row + 1);
    if (result[row] != expected) {
      printf("[TC 1] row %u FAILED. Got %u, expected %u.\n",
             row, result[row], expected);
      num_failed++;
      return;
    }
  }

  printf("PASSED.\n");
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();

  EXIT_CHECK();
}
