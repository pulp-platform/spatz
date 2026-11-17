// Copyright 2026 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0

#include "gemm.h"

static inline __attribute__((always_inline)) void gemm_gelu_body(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K, unsigned post_op) {
  gather_gemm_1x4((const uint32_t *)Atpack, (const uint32_t *)Bpack,
                  C, M, N, K, post_op);
}

__attribute__((noinline, aligned(64))) void gemm_gelu_fused(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K) {
  gemm_gelu_body(Atpack, Bpack, C, M, N, K, 2);
}

__attribute__((noinline, aligned(64))) void gemm_gelu_non_fused(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C,
    uint32_t M, uint32_t N, uint32_t K) {
  gemm_gelu_body(Atpack, Bpack, C, M, N, K, 0);
}

__attribute__((noinline, aligned(64))) void gelu_non_fused(
    float *C, uint32_t elements) {
  uintptr_t p = (uintptr_t)C;
  uintptr_t q = p + 256;
  uintptr_t groups = elements / 128;
  asm volatile(
      "vsetvli zero, %[vl], e32, m4, ta, ma\n"
      "1:\n"
      "vle32.v v8, (%[p])\n"
      "vle32.v v12, (%[q])\n"
      GELU_PAIR_ASM("v8", "v12", "v0", "v4", "v24", "v28")
      "vse32.v v8, (%[p])\n"
      "vse32.v v12, (%[q])\n"
      "addi %[p], %[p], 512\n"
      "addi %[q], %[q], 512\n"
      "addi %[groups], %[groups], -1\n"
      "bnez %[groups], 1b\n"
      : [p] "+&r"(p), [q] "+&r"(q), [groups] "+&r"(groups)
      : [vl] "r"((uintptr_t)64), GELU_INPUTS
      : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
        "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
        "memory");
}
