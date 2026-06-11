// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// vtzero — Tile-zero instruction tests across different SEW/TEW configurations
//
// -------------------------------------------------------
// Test cases:
//    TC1: SEW=e8,  TEW=32 (mtwiden=3→TWIDEN=4), 4×4 — vtmmu then vtzero → zeros
//    TC2: SEW=e8,  TEW=32, two tiles — vtzero only mt0, mt4 stays intact
//    TC3: SEW=e16, TEW=32 (mtwiden=3, vsew=1), 4×4 — vtmmu then vtzero → zeros
//    TC4: SEW=e32, TEW=32 (mtwiden=0, vsew=2), 4×4 — I*I then vtzero → zeros

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// =========================================================
// TC1: SEW=e8/TEW=32, 4×4 — vtmmu then vtzero → all zeros
// =========================================================
void TEST_CASE1(void) {
  const uintptr_t tn = 4, tm = 4, tk = 4;
  VSET(4, e8, m1);

  // A rows: [1..4], [5..8], [9..12], [13..16]
  VLOAD_8(v8,   1,  2,  3,  4,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v10,  5,  6,  7,  8,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v12,  9, 10, 11, 12,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v14, 13, 14, 15, 16,  0,0,0,0,  0,0,0,0,  0,0,0,0);

  // Identity rows
  VLOAD_8(v16,  1,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v18,  0,1,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v20,  0,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v22,  0,0,0,1,  0,0,0,0,  0,0,0,0,  0,0,0,0);

  vme_cfg_e8(tn, tm, tk);
  asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");
  // Zero the accumulator
  asm volatile("vtzero mt0" ::: "memory");
  vme_readback_e32(tn, tm);

  uintptr_t tss;
  tss = TSS_ROW(0, 0);
  asm volatile("vtmv.v.t v1, %[t]" :: [t] "r"(tss) : "memory");

  VSET(4, e32, m1);
  VCMP_I32(1, v1, 0, 0, 0, 0);

  asm volatile("vtdiscard" ::: "memory");
}

// =========================================================
// TC2: SEW=e8/TEW=32, two tiles — vtzero only mt0, mt4 intact
//      mt4 row 0 should remain A*I = [1,2,3,4]
// =========================================================
void TEST_CASE2(void) {
  const uintptr_t tn = 4, tm = 4, tk = 4;
  VSET(4, e8, m1);

  VLOAD_8(v8,   1,  2,  3,  4,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v10,  5,  6,  7,  8,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v12,  9, 10, 11, 12,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v14, 13, 14, 15, 16,  0,0,0,0,  0,0,0,0,  0,0,0,0);

  VLOAD_8(v16,  1,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v18,  0,1,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v20,  0,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v22,  0,0,0,1,  0,0,0,0,  0,0,0,0,  0,0,0,0);

  vme_cfg_e8(tn, tm, tk);
  // Fill both mt0 and mt4 with A * I
  asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");
  asm volatile("vtmmu.tvv mt4, v8, v16" ::: "memory");
  // Zero only mt0; mt4 must remain intact
  asm volatile("vtzero mt0" ::: "memory");
  vme_readback_e32(tn, tm);

  uintptr_t tss;
  // mt0 row 0 → all zeros
  tss = TSS_ROW(0, 0);
  asm volatile("vtmv.v.t v1, %[t]" :: [t] "r"(tss) : "memory");
  // mt4 row 0 → A*I row 0 = [1,2,3,4] (int32)
  tss = TSS_ROW(4, 0);
  asm volatile("vtmv.v.t v2, %[t]" :: [t] "r"(tss) : "memory");

  VSET(4, e32, m1);
  VCMP_I32(1, v1, 0, 0, 0, 0);
  VCMP_I32(2, v2, 1, 5, 9, 13);

  asm volatile("vtdiscard" ::: "memory");
}

// =========================================================
// TC3: SEW=e16/TEW=32, 4×4 — vtmmu then vtzero → all zeros
// =========================================================
void TEST_CASE3(void) {
  const uintptr_t tn = 4, tm = 4, tk = 4;

  // Step 1: pre-fill with vtmmu (SEW=8 is the ONLY valid SEW for vtmmu)
  VSET(4, e8, m1);
  VLOAD_8(v8,  1, 1, 1, 1,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v10, 2, 2, 2, 2,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v12, 3, 3, 3, 3,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v14, 4, 4, 4, 4,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v16, 1,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v18, 0,1,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v20, 0,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v22, 0,0,0,1,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  vme_cfg_e8(tn, tm, tk);
  asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

  // Step 2: reconfigure to SEW=16 and call vtzero
  // (vtzero itself doesn't care about SEW, only uses tm/tn from mtype)
  vme_cfg_e16(tn, tm, tk);
  asm volatile("vtzero mt0" ::: "memory");

  vme_readback_e32(tn, tm);
  uintptr_t tss;
  tss = TSS_ROW(0, 0);
  asm volatile("vtmv.v.t v1, %[t]" :: [t] "r"(tss) : "memory");

  VSET(4, e32, m1);
  VCMP_I32(1, v1, 0, 0, 0, 0);
  asm volatile("vtdiscard" ::: "memory");
}

// =========================================================
// TC4: SEW=e32/TEW=32, no widening (mtwiden=0), 4×4
//      I * I = I → vtzero → all zeros
// =========================================================
void TEST_CASE4(void) {
  const uintptr_t tn = 4, tm = 4, tk = 4;
  VSET(4, e32, m1);

  // A = identity (int32)
  VLOAD_32(v8,   1, 0, 0, 0);
  VLOAD_32(v10,  0, 1, 0, 0);
  VLOAD_32(v12,  0, 0, 1, 0);
  VLOAD_32(v14,  0, 0, 0, 1);

  // B = identity (int32)
  VLOAD_32(v16,  1, 0, 0, 0);
  VLOAD_32(v18,  0, 1, 0, 0);
  VLOAD_32(v20,  0, 0, 1, 0);
  VLOAD_32(v22,  0, 0, 0, 1);

  vme_cfg_e32(tn, tm, tk);
  // I * I = I
  asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");
  // Zero the result
  asm volatile("vtzero mt0" ::: "memory");
  vme_readback_e32(tn, tm);

  uintptr_t tss;
  tss = TSS_ROW(0, 0);
  asm volatile("vtmv.v.t v1, %[t]" :: [t] "r"(tss) : "memory");

  VSET(4, e32, m1);
  VCMP_I32(1, v1, 0, 0, 0, 0);

  asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_vme();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();

  EXIT_CHECK();
}
