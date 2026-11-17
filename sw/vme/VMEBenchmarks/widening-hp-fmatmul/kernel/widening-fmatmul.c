// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "widening-fmatmul.h"

#include <stdint.h>

static inline __attribute__((always_inline)) uintptr_t
widening_mtype(uint32_t tm, uint32_t tk) {
  // MTWIDEN=2: packed FP16 operands and an FP32 accumulator tile.
  return ((uintptr_t)tm << 10) | ((uintptr_t)tk << 5) | 2u;
}

int __attribute__((noinline, aligned(64))) vme_widening_hp_fmatmul_1tile(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K) {
  if (M == 0 || N == 0 || K == 0 || M > VME_WIDENING_TE ||
      N > VME_WIDENING_TE)
    return -1;

  const uintptr_t fp16_vtype = 0xc8u;  // e16, m1, ta, ma
  const uint32_t pairs = (K + 1u) >> 1;

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2\n"
               "vtzero mt0"
               :: "r"(widening_mtype(M, K == 1 ? 1u : 2u)),
                  "r"(fp16_vtype), "r"((uintptr_t)N)
               : "memory");

  for (uint32_t pair = 0; pair < pairs; ++pair) {
    const uint32_t tk = (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
    const uint32_t *const avec = apack + pair * M;
    const uint32_t *const bvec = bpack + pair * N;

    asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                 "vle32.v v0, (%1)\n"
                 "vsetvli zero, %2, e32, m1, ta, ma\n"
                 "vle32.v v16, (%3)\n"
                 "msetmtype %4, %5\n"
                 "msettn zero, %2\n"
                 "vtfmm.tvv mt0, v0, v16"
                 :: "r"((uintptr_t)M), "r"(avec), "r"((uintptr_t)N),
                    "r"(bvec), "r"(widening_mtype(M, tk)),
                    "r"(fp16_vtype)
                 : "v0", "v16", "memory");
  }

  // VTSE preserves the FP32 accumulator representation and bypasses the VRF.
  for (uint32_t row = 0; row < M; ++row) {
    const uintptr_t tss = row;  // TSS_ROW(mt0, row)
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + row * N)
                 : "memory");
  }

  asm volatile("fence rw, rw\nvtdiscard" ::: "memory");
  return 0;
}

static inline __attribute__((always_inline)) int
validate_2tile_shape(uint32_t M, uint32_t N, uint32_t K) {
  return M != 0 && M <= VME_WIDENING_TE && N > VME_WIDENING_TE &&
         N <= 2u * VME_WIDENING_TE && K != 0;
}

int __attribute__((noinline, aligned(64)))
vme_widening_hp_fmatmul_2tile_pingpong(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K) {
  if (!validate_2tile_shape(M, N, K))
    return -1;

  const uintptr_t fp16_vtype = 0xc8u;
  const uint32_t n0 = VME_WIDENING_TE;
  const uint32_t n1 = N - n0;
  const uint32_t pairs = (K + 1u) >> 1;
  const uint32_t first_tk = K == 1 ? 1u : 2u;

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2\n"
               "vtzero mt0"
               :: "r"(widening_mtype(M, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0)
               : "memory");

  // Finish the first output tile before reusing the operand registers.
  for (uint32_t pair = 0; pair < pairs; ++pair) {
    const uint32_t tk = (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
    const uint32_t *const avec = apack + pair * M;
    const uint32_t *const bvec = bpack + pair * N;
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                 "vle32.v v0, (%1)\n"
                 "vsetvli zero, %2, e32, m1, ta, ma\n"
                 "vle32.v v16, (%3)\n"
                 "msetmtype %4, %5\n"
                 "msettn zero, %2\n"
                 "vtfmm.tvv mt0, v0, v16"
                 :: "r"((uintptr_t)M), "r"(avec), "r"((uintptr_t)n0),
                    "r"(bvec), "r"(widening_mtype(M, tk)),
                    "r"(fp16_vtype)
                 : "v0", "v16", "memory");
  }

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2\n"
               "vtzero mt4"
               :: "r"(widening_mtype(M, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n1)
               : "memory");

  // While mt4 reduces the second N tile, drain one mt0 row per K pair.
  // VTSE uses the VLSU and accesses mt0, so it can overlap mt4 MAC work.
  uint32_t stored_rows = 0;
  for (uint32_t pair = 0; pair < pairs; ++pair) {
    const uint32_t tk = (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
    const uint32_t *const avec = apack + pair * M;
    const uint32_t *const bvec = bpack + pair * N + n0;
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                 "vle32.v v0, (%1)\n"
                 "vsetvli zero, %2, e32, m1, ta, ma\n"
                 "vle32.v v16, (%3)\n"
                 "msetmtype %4, %5\n"
                 "msettn zero, %2\n"
                 "vtfmm.tvv mt4, v0, v16"
                 :: "r"((uintptr_t)M), "r"(avec), "r"((uintptr_t)n1),
                    "r"(bvec), "r"(widening_mtype(M, tk)),
                    "r"(fp16_vtype)
                 : "v0", "v16", "memory");

    if (stored_rows < M) {
      const uintptr_t tss = stored_rows;
      asm volatile("msettn zero, %0\n"
                   "vtse32 %1, (%2)"
                   :: "r"((uintptr_t)n0), "r"(tss),
                      "r"(c + stored_rows * N)
                   : "memory");
      ++stored_rows;
    }
  }

  asm volatile("msettn zero, %0" :: "r"((uintptr_t)n0) : "memory");
  while (stored_rows < M) {
    const uintptr_t tss = stored_rows;
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + stored_rows * N)
                 : "memory");
    ++stored_rows;
  }

  asm volatile("msettn zero, %0" :: "r"((uintptr_t)n1) : "memory");
  for (uint32_t row = 0; row < M; ++row) {
    const uintptr_t tss = 0x20000000u | row;  // TSS_ROW(mt4, row)
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + row * N + n0)
                 : "memory");
  }

  asm volatile("fence rw, rw\nvtdiscard" ::: "memory");
  return 0;
}

int __attribute__((noinline, aligned(64))) vme_widening_hp_fmatmul_2tile_reuse(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K) {
  if (!validate_2tile_shape(M, N, K))
    return -1;

  const uintptr_t fp16_vtype = 0xc8u;
  const uint32_t n0 = VME_WIDENING_TE;
  const uint32_t n1 = N - n0;
  const uint32_t pairs = (K + 1u) >> 1;
  const uint32_t first_tk = K == 1 ? 1u : 2u;

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2\n"
               "vtzero mt0\n"
               "msettn zero, %3\n"
               "vtzero mt4"
               :: "r"(widening_mtype(M, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0), "r"((uintptr_t)n1)
               : "memory");

  // One A load feeds both tiles. B0 and B1 occupy independent vector groups.
  for (uint32_t pair = 0; pair < pairs; ++pair) {
    const uint32_t tk = (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
    const uint32_t *const avec = apack + pair * M;
    const uint32_t *const bvec0 = bpack + pair * N;
    const uint32_t *const bvec1 = bvec0 + n0;
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                 "vle32.v v0, (%1)\n"
                 "vsetvli zero, %2, e32, m1, ta, ma\n"
                 "vle32.v v16, (%3)\n"
                 "vsetvli zero, %4, e32, m1, ta, ma\n"
                 "vle32.v v24, (%5)\n"
                 "msetmtype %6, %7\n"
                 "msettn zero, %2\n"
                 "vtfmm.tvv mt0, v0, v16\n"
                 "msettn zero, %4\n"
                 "vtfmm.tvv mt4, v0, v24"
                 :: "r"((uintptr_t)M), "r"(avec), "r"((uintptr_t)n0),
                    "r"(bvec0), "r"((uintptr_t)n1), "r"(bvec1),
                    "r"(widening_mtype(M, tk)), "r"(fp16_vtype)
                 : "v0", "v16", "v24", "memory");
  }

  asm volatile("msettn zero, %0" :: "r"((uintptr_t)n0) : "memory");
  for (uint32_t row = 0; row < M; ++row) {
    const uintptr_t tss = row;
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + row * N)
                 : "memory");
  }
  asm volatile("msettn zero, %0" :: "r"((uintptr_t)n1) : "memory");
  for (uint32_t row = 0; row < M; ++row) {
    const uintptr_t tss = 0x20000000u | row;
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + row * N + n0)
                 : "memory");
  }

  asm volatile("fence rw, rw\nvtdiscard" ::: "memory");
  return 0;
}

static inline __attribute__((always_inline)) int
validate_4tile_shape(uint32_t M, uint32_t N, uint32_t K) {
  return M > VME_WIDENING_TE && M <= 2u * VME_WIDENING_TE &&
         N > VME_WIDENING_TE && N <= 2u * VME_WIDENING_TE && K != 0;
}

int __attribute__((noinline, aligned(64)))
vme_widening_hp_fmatmul_4tile_pingpong(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K) {
  if (!validate_4tile_shape(M, N, K))
    return -1;

  const uintptr_t fp16_vtype = 0xc8u;
  const uint32_t m0 = VME_WIDENING_TE;
  const uint32_t m1 = M - m0;
  const uint32_t n0 = VME_WIDENING_TE;
  const uint32_t n1 = N - n0;
  const uint32_t pairs = (K + 1u) >> 1;
  const uint32_t first_tk = K == 1 ? 1u : 2u;

  // Top output pair: mt0=A0*B0, mt4=A0*B1.
  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2\n"
               "vtzero mt0\n"
               "msettn zero, %3\n"
               "vtzero mt4"
               :: "r"(widening_mtype(m0, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0), "r"((uintptr_t)n1)
               : "memory");
  for (uint32_t pair = 0; pair < pairs; ++pair) {
    const uint32_t tk = (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
    const uint32_t *const avec = apack + pair * M;
    const uint32_t *const bvec0 = bpack + pair * N;
    const uint32_t *const bvec1 = bvec0 + n0;
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                 "vle32.v v0, (%1)\n"
                 "vsetvli zero, %2, e32, m1, ta, ma\n"
                 "vle32.v v16, (%3)\n"
                 "vsetvli zero, %4, e32, m1, ta, ma\n"
                 "vle32.v v24, (%5)\n"
                 "msetmtype %6, %7\n"
                 "msettn zero, %2\n"
                 "vtfmm.tvv mt0, v0, v16\n"
                 "msettn zero, %4\n"
                 "vtfmm.tvv mt4, v0, v24"
                 :: "r"((uintptr_t)m0), "r"(avec), "r"((uintptr_t)n0),
                    "r"(bvec0), "r"((uintptr_t)n1), "r"(bvec1),
                    "r"(widening_mtype(m0, tk)), "r"(fp16_vtype)
                 : "v0", "v16", "v24", "memory");
  }

  // Bottom output pair: mt8=A1*B0, mt12=A1*B1.
  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2\n"
               "vtzero mt8\n"
               "msettn zero, %3\n"
               "vtzero mt12"
               :: "r"(widening_mtype(m1, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0), "r"((uintptr_t)n1)
               : "memory");

  // Interleave top-pair stores with bottom-pair reduction. For K64 there are
  // exactly 32 pair iterations, enough to hide all 32 top-tile row stores.
  uint32_t stored_rows = 0;
  for (uint32_t pair = 0; pair < pairs; ++pair) {
    const uint32_t tk = (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
    const uint32_t *const avec = apack + pair * M + m0;
    const uint32_t *const bvec0 = bpack + pair * N;
    const uint32_t *const bvec1 = bvec0 + n0;
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                 "vle32.v v8, (%1)\n"
                 "vsetvli zero, %2, e32, m1, ta, ma\n"
                 "vle32.v v16, (%3)\n"
                 "vsetvli zero, %4, e32, m1, ta, ma\n"
                 "vle32.v v24, (%5)\n"
                 "msetmtype %6, %7\n"
                 "msettn zero, %2\n"
                 "vtfmm.tvv mt8, v8, v16\n"
                 "msettn zero, %4\n"
                 "vtfmm.tvv mt12, v8, v24"
                 :: "r"((uintptr_t)m1), "r"(avec), "r"((uintptr_t)n0),
                    "r"(bvec0), "r"((uintptr_t)n1), "r"(bvec1),
                    "r"(widening_mtype(m1, tk)), "r"(fp16_vtype)
                 : "v8", "v16", "v24", "memory");

    if (stored_rows < 2u * m0) {
      const uint32_t right = stored_rows >= m0;
      const uint32_t row = right ? stored_rows - m0 : stored_rows;
      const uintptr_t tss = (right ? 0x20000000u : 0u) | row;
      const uint32_t tn = right ? n1 : n0;
      float *const dst = c + row * N + (right ? n0 : 0u);
      asm volatile("msettn zero, %0\n"
                   "vtse32 %1, (%2)"
                   :: "r"((uintptr_t)tn), "r"(tss), "r"(dst)
                   : "memory");
      ++stored_rows;
    }
  }

  while (stored_rows < 2u * m0) {
    const uint32_t right = stored_rows >= m0;
    const uint32_t row = right ? stored_rows - m0 : stored_rows;
    const uintptr_t tss = (right ? 0x20000000u : 0u) | row;
    const uint32_t tn = right ? n1 : n0;
    float *const dst = c + row * N + (right ? n0 : 0u);
    asm volatile("msettn zero, %0\n"
                 "vtse32 %1, (%2)"
                 :: "r"((uintptr_t)tn), "r"(tss), "r"(dst)
                 : "memory");
    ++stored_rows;
  }

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2"
               :: "r"(widening_mtype(m1, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0)
               : "memory");
  for (uint32_t row = 0; row < m1; ++row) {
    const uintptr_t tss = 0x40000000u | row;  // mt8
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + (m0 + row) * N)
                 : "memory");
  }
  asm volatile("msettn zero, %0" :: "r"((uintptr_t)n1) : "memory");
  for (uint32_t row = 0; row < m1; ++row) {
    const uintptr_t tss = 0x60000000u | row;  // mt12
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + (m0 + row) * N + n0)
                 : "memory");
  }

  asm volatile("fence rw, rw\nvtdiscard" ::: "memory");
  return 0;
}

int __attribute__((noinline, aligned(64))) vme_widening_hp_fmatmul_4tile_reuse(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K) {
  if (!validate_4tile_shape(M, N, K))
    return -1;

  const uintptr_t fp16_vtype = 0xc8u;
  const uint32_t m0 = VME_WIDENING_TE;
  const uint32_t m1 = M - m0;
  const uint32_t n0 = VME_WIDENING_TE;
  const uint32_t n1 = N - n0;
  const uint32_t pairs = (K + 1u) >> 1;
  const uint32_t first_tk = K == 1 ? 1u : 2u;

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2\n"
               "vtzero mt0\n"
               "msettn zero, %3\n"
               "vtzero mt4\n"
               "msetmtype %4, %1\n"
               "msettn zero, %2\n"
               "vtzero mt8\n"
               "msettn zero, %3\n"
               "vtzero mt12"
               :: "r"(widening_mtype(m0, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0), "r"((uintptr_t)n1),
                  "r"(widening_mtype(m1, first_tk))
               : "memory");

  // Two A and two B loads feed four independent tile MACs per K pair.
  for (uint32_t pair = 0; pair < pairs; ++pair) {
    const uint32_t tk = (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
    const uint32_t *const avec0 = apack + pair * M;
    const uint32_t *const avec1 = avec0 + m0;
    const uint32_t *const bvec0 = bpack + pair * N;
    const uint32_t *const bvec1 = bvec0 + n0;
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                 "vle32.v v0, (%1)\n"
                 "vsetvli zero, %2, e32, m1, ta, ma\n"
                 "vle32.v v8, (%3)\n"
                 "vsetvli zero, %4, e32, m1, ta, ma\n"
                 "vle32.v v16, (%5)\n"
                 "vsetvli zero, %6, e32, m1, ta, ma\n"
                 "vle32.v v24, (%7)\n"
                 "msetmtype %8, %9\n"
                 "msettn zero, %4\n"
                 "vtfmm.tvv mt0, v0, v16\n"
                 "msettn zero, %6\n"
                 "vtfmm.tvv mt4, v0, v24\n"
                 "msetmtype %10, %9\n"
                 "msettn zero, %4\n"
                 "vtfmm.tvv mt8, v8, v16\n"
                 "msettn zero, %6\n"
                 "vtfmm.tvv mt12, v8, v24"
                 :: "r"((uintptr_t)m0), "r"(avec0), "r"((uintptr_t)m1),
                    "r"(avec1), "r"((uintptr_t)n0), "r"(bvec0),
                    "r"((uintptr_t)n1), "r"(bvec1),
                    "r"(widening_mtype(m0, tk)), "r"(fp16_vtype),
                    "r"(widening_mtype(m1, tk))
                 : "v0", "v8", "v16", "v24", "memory");
  }

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2"
               :: "r"(widening_mtype(m0, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0)
               : "memory");
  for (uint32_t row = 0; row < m0; ++row) {
    const uintptr_t tss = row;
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + row * N)
                 : "memory");
  }
  asm volatile("msettn zero, %0" :: "r"((uintptr_t)n1) : "memory");
  for (uint32_t row = 0; row < m0; ++row) {
    const uintptr_t tss = 0x20000000u | row;
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + row * N + n0)
                 : "memory");
  }

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2"
               :: "r"(widening_mtype(m1, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0)
               : "memory");
  for (uint32_t row = 0; row < m1; ++row) {
    const uintptr_t tss = 0x40000000u | row;
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + (m0 + row) * N)
                 : "memory");
  }
  asm volatile("msettn zero, %0" :: "r"((uintptr_t)n1) : "memory");
  for (uint32_t row = 0; row < m1; ++row) {
    const uintptr_t tss = 0x60000000u | row;
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + (m0 + row) * N + n0)
                 : "memory");
  }

  asm volatile("fence rw, rw\nvtdiscard" ::: "memory");
  return 0;
}

static inline __attribute__((always_inline)) uint32_t min_u32(uint32_t a,
                                                               uint32_t b) {
  return a < b ? a : b;
}

static inline __attribute__((always_inline)) void widening_block_auto(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t block_m,
    uint32_t block_n, uint32_t K, uint32_t a_stride, uint32_t b_stride,
    uint32_t c_stride) {
  const uintptr_t fp16_vtype = 0xc8u;
  const uint32_t m0 = min_u32(block_m, VME_WIDENING_TE);
  const uint32_t m1 = block_m - m0;
  const uint32_t n0 = min_u32(block_n, VME_WIDENING_TE);
  const uint32_t n1 = block_n - n0;
  const uint32_t pairs = (K + 1u) >> 1;
  const uint32_t first_tk = K == 1 ? 1u : 2u;

  // Every panel owns mt0. The active tails determine whether mt4, mt8 and
  // mt12 are also part of this panel.
  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2\n"
               "vtzero mt0"
               :: "r"(widening_mtype(m0, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0)
               : "memory");
  if (n1) {
    asm volatile("msettn zero, %0\nvtzero mt4"
                 :: "r"((uintptr_t)n1)
                 : "memory");
  }
  if (m1) {
    asm volatile("msetmtype %0, %1\n"
                 "msettn zero, %2\n"
                 "vtzero mt8"
                 :: "r"(widening_mtype(m1, first_tk)), "r"(fp16_vtype),
                    "r"((uintptr_t)n0)
                 : "memory");
    if (n1) {
      asm volatile("msettn zero, %0\nvtzero mt12"
                   :: "r"((uintptr_t)n1)
                   : "memory");
    }
  }

  // Select the panel topology once. Keeping these conditions outside the K
  // loop prevents every reduction pair from re-testing m1/n1.
  if (!m1 && !n1) {
    for (uint32_t pair = 0; pair < pairs; ++pair) {
      const uint32_t tk =
          (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
      const uint32_t *const avec0 = apack + pair * a_stride;
      const uint32_t *const bvec0 = bpack + pair * b_stride;
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                   "vle32.v v0, (%1)\n"
                   "vsetvli zero, %2, e32, m1, ta, ma\n"
                   "vle32.v v16, (%3)\n"
                   "msetmtype %4, %5\n"
                   "msettn zero, %2\n"
                   "vtfmm.tvv mt0, v0, v16"
                   :: "r"((uintptr_t)m0), "r"(avec0),
                      "r"((uintptr_t)n0), "r"(bvec0),
                      "r"(widening_mtype(m0, tk)), "r"(fp16_vtype)
                   : "v0", "v16", "memory");
    }
  } else if (!m1) {
    for (uint32_t pair = 0; pair < pairs; ++pair) {
      const uint32_t tk =
          (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
      const uint32_t *const avec0 = apack + pair * a_stride;
      const uint32_t *const bvec0 = bpack + pair * b_stride;
      const uint32_t *const bvec1 = bvec0 + n0;
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                   "vle32.v v0, (%1)\n"
                   "vsetvli zero, %2, e32, m1, ta, ma\n"
                   "vle32.v v16, (%3)\n"
                   "vsetvli zero, %4, e32, m1, ta, ma\n"
                   "vle32.v v24, (%5)\n"
                   "msetmtype %6, %7\n"
                   "msettn zero, %2\n"
                   "vtfmm.tvv mt0, v0, v16\n"
                   "msettn zero, %4\n"
                   "vtfmm.tvv mt4, v0, v24"
                   :: "r"((uintptr_t)m0), "r"(avec0),
                      "r"((uintptr_t)n0), "r"(bvec0),
                      "r"((uintptr_t)n1), "r"(bvec1),
                      "r"(widening_mtype(m0, tk)), "r"(fp16_vtype)
                   : "v0", "v16", "v24", "memory");
    }
  } else if (!n1) {
    for (uint32_t pair = 0; pair < pairs; ++pair) {
      const uint32_t tk =
          (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
      const uint32_t *const avec0 = apack + pair * a_stride;
      const uint32_t *const avec1 = avec0 + m0;
      const uint32_t *const bvec0 = bpack + pair * b_stride;
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                   "vle32.v v0, (%1)\n"
                   "vsetvli zero, %2, e32, m1, ta, ma\n"
                   "vle32.v v8, (%3)\n"
                   "vsetvli zero, %4, e32, m1, ta, ma\n"
                   "vle32.v v16, (%5)\n"
                   "msetmtype %6, %7\n"
                   "msettn zero, %4\n"
                   "vtfmm.tvv mt0, v0, v16\n"
                   "msetmtype %8, %7\n"
                   "msettn zero, %4\n"
                   "vtfmm.tvv mt8, v8, v16"
                   :: "r"((uintptr_t)m0), "r"(avec0),
                      "r"((uintptr_t)m1), "r"(avec1),
                      "r"((uintptr_t)n0), "r"(bvec0),
                      "r"(widening_mtype(m0, tk)), "r"(fp16_vtype),
                      "r"(widening_mtype(m1, tk))
                   : "v0", "v8", "v16", "memory");
    }
  } else if (m0 == m1 && m0 == n0 && m0 == n1) {
    // A full 2x2 panel uses VL=TE for all four operands. One vsetvli feeds
    // all loads instead of redundantly issuing four identical settings.
    for (uint32_t pair = 0; pair < pairs; ++pair) {
      const uint32_t tk =
          (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
      const uint32_t *const avec0 = apack + pair * a_stride;
      const uint32_t *const avec1 = avec0 + m0;
      const uint32_t *const bvec0 = bpack + pair * b_stride;
      const uint32_t *const bvec1 = bvec0 + n0;
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                   "vle32.v v0, (%1)\n"
                   "vle32.v v8, (%2)\n"
                   "vle32.v v16, (%3)\n"
                   "vle32.v v24, (%4)\n"
                   "msetmtype %5, %6\n"
                   "msettn zero, %0\n"
                   "vtfmm.tvv mt0, v0, v16\n"
                   "vtfmm.tvv mt4, v0, v24\n"
                   "vtfmm.tvv mt8, v8, v16\n"
                   "vtfmm.tvv mt12, v8, v24"
                   :: "r"((uintptr_t)m0), "r"(avec0), "r"(avec1),
                      "r"(bvec0), "r"(bvec1),
                      "r"(widening_mtype(m0, tk)), "r"(fp16_vtype)
                   : "v0", "v8", "v16", "v24", "memory");
    }
  } else {
    for (uint32_t pair = 0; pair < pairs; ++pair) {
      const uint32_t tk =
          (pair + 1u == pairs && (K & 1u)) ? 1u : 2u;
      const uint32_t *const avec0 = apack + pair * a_stride;
      const uint32_t *const avec1 = avec0 + m0;
      const uint32_t *const bvec0 = bpack + pair * b_stride;
      const uint32_t *const bvec1 = bvec0 + n0;
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma\n"
                   "vle32.v v0, (%1)\n"
                   "vsetvli zero, %2, e32, m1, ta, ma\n"
                   "vle32.v v8, (%3)\n"
                   "vsetvli zero, %4, e32, m1, ta, ma\n"
                   "vle32.v v16, (%5)\n"
                   "vsetvli zero, %6, e32, m1, ta, ma\n"
                   "vle32.v v24, (%7)\n"
                   "msetmtype %8, %9\n"
                   "msettn zero, %4\n"
                   "vtfmm.tvv mt0, v0, v16\n"
                   "msettn zero, %6\n"
                   "vtfmm.tvv mt4, v0, v24\n"
                   "msetmtype %10, %9\n"
                   "msettn zero, %4\n"
                   "vtfmm.tvv mt8, v8, v16\n"
                   "msettn zero, %6\n"
                   "vtfmm.tvv mt12, v8, v24"
                   :: "r"((uintptr_t)m0), "r"(avec0),
                      "r"((uintptr_t)m1), "r"(avec1),
                      "r"((uintptr_t)n0), "r"(bvec0),
                      "r"((uintptr_t)n1), "r"(bvec1),
                      "r"(widening_mtype(m0, tk)), "r"(fp16_vtype),
                      "r"(widening_mtype(m1, tk))
                   : "v0", "v8", "v16", "v24", "memory");
    }
  }

  asm volatile("msetmtype %0, %1\n"
               "msettn zero, %2"
               :: "r"(widening_mtype(m0, first_tk)), "r"(fp16_vtype),
                  "r"((uintptr_t)n0)
               : "memory");
  for (uint32_t row = 0; row < m0; ++row) {
    const uintptr_t tss = row;
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(tss), "r"(c + row * c_stride)
                 : "memory");
  }
  if (n1) {
    asm volatile("msettn zero, %0" :: "r"((uintptr_t)n1) : "memory");
    for (uint32_t row = 0; row < m0; ++row) {
      const uintptr_t tss = 0x20000000u | row;
      asm volatile("vtse32 %0, (%1)"
                   :: "r"(tss), "r"(c + row * c_stride + n0)
                   : "memory");
    }
  }
  if (m1) {
    asm volatile("msetmtype %0, %1\n"
                 "msettn zero, %2"
                 :: "r"(widening_mtype(m1, first_tk)), "r"(fp16_vtype),
                    "r"((uintptr_t)n0)
                 : "memory");
    for (uint32_t row = 0; row < m1; ++row) {
      const uintptr_t tss = 0x40000000u | row;
      asm volatile("vtse32 %0, (%1)"
                   :: "r"(tss), "r"(c + (m0 + row) * c_stride)
                   : "memory");
    }
    if (n1) {
      asm volatile("msettn zero, %0" :: "r"((uintptr_t)n1) : "memory");
      for (uint32_t row = 0; row < m1; ++row) {
        const uintptr_t tss = 0x60000000u | row;
        asm volatile("vtse32 %0, (%1)"
                     :: "r"(tss),
                        "r"(c + (m0 + row) * c_stride + n0)
                     : "memory");
      }
    }
  }

  asm volatile("fence rw, rw\nvtdiscard" ::: "memory");
}

int __attribute__((noinline, aligned(64))) vme_widening_hp_fmatmul_auto(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K) {
  if (M == 0 || N == 0 || K == 0)
    return -1;

  // A and B remain pair-major over the complete matrices. Panel base pointers
  // select the active rows/columns while a_stride and b_stride advance to the
  // next reduction pair in the original packed layout.
  for (uint32_t m = 0; m < M; m += 2u * VME_WIDENING_TE) {
    const uint32_t block_m = min_u32(2u * VME_WIDENING_TE, M - m);
    for (uint32_t n = 0; n < N; n += 2u * VME_WIDENING_TE) {
      const uint32_t block_n = min_u32(2u * VME_WIDENING_TE, N - n);
      widening_block_auto(c + m * N + n, apack + m, bpack + n, block_m,
                          block_n, K, M, N, N);
    }
  }
  return 0;
}
