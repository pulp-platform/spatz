// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(4, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20);
  VLOAD_32(v4, -1, 2, -3, 4);
  __asm__ volatile("vasub.vv v6, v2, v4" ::);
  VEC_CMP_32(1, v6, 3, 4, 9, 8);
}

void TEST_CASE2(void) {
  VSET(4, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20);
  VLOAD_32(v4, 1, 2, 3, -4);
  VLOAD_32(v0, 10, 0, 0, 0);
  CLEAR(v6);
  __asm__ volatile("vasub.vv v6, v2, v4, v0.t" ::);
  VEC_CMP_32(2, v6, 0, 4, 0, 12);
}

void TEST_CASE3(void) {
  VSET(4, e32, m2);
  VLOAD_U32(v2, 5, 10, 15, 20);
  const uint64_t scalar = -5;
  __asm__ volatile("vasub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VEC_CMP_U32(3, v6, 5, 8, 10, 13);
}

void TEST_CASE4(void) {
  VSET(4, e32, m2);
  VLOAD_U32(v2, 5, 10, 15, 20);
  const uint64_t scalar = -5;
  VLOAD_U32(v0, 10, 0, 0, 0);
  CLEAR(v6);
  __asm__ volatile("vasub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VEC_CMP_U32(4, v6, 0, 8, 0, 13);
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
