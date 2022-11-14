// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(4, e8, m2);
  VLOAD_8(v2, 1, 2, 3, 4);
  VLOAD_8(v4, 1, 2, 3, 4);
  __asm__ volatile("vaaddu.vv v6, v2, v4" ::);
  VEC_CMP_8(1, v6, 1, 2, 3, 4);
}

void TEST_CASE2(void) {
  VSET(4, e8, m2);
  VLOAD_8(v2, 1, 2, 3, 4);
  VLOAD_8(v4, 1, 2, 3, 4);
  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  CLEAR(v6);
  __asm__ volatile("vaaddu.vv v6, v2, v4, v0.t" ::);
  VEC_CMP_8(2, v6, 0, 2, 0, 4);
}

void TEST_CASE3(void) {
  VSET(4, e32, m2);
  VLOAD_U32(v2, 1, 2, 3, 4);
  const uint32_t scalar = 5;
  __asm__ volatile("vaaddu.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VEC_CMP_U32(3, v6, 3, 4, 4, 5);
}

// Dont use VCLEAR here, it results in a glitch where are values are off by 1
void TEST_CASE4(void) {
  VSET(4, e32, m2);
  VLOAD_U32(v2, 1, 2, 3, 4);
  const uint32_t scalar = 5;
  VLOAD_U32(v0, 0xA, 0x0, 0x0, 0x0);
  CLEAR(v6);
  __asm__ volatile("vaaddu.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VEC_CMP_U32(4, v6, 0, 4, 0, 5);
}

int main(void) {
  INIT_CHECK();
  enable_vec();
  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  EXIT_CHECK();
}
