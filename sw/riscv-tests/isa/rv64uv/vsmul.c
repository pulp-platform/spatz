// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1() {
  VSET(3, e8, m2);
  VLOAD_8(v4, 127, 127, -50);
  VLOAD_8(v6, 127, 10, 127);
  __asm__ volatile("vsmul.vv v2, v4, v6");
  VEC_CMP_8(1, v2, 126, 10, -50);
}

void TEST_CASE2() {
  VSET(3, e8, m2);
  VLOAD_8(v4, 127, 127, -50);
  VLOAD_8(v6, 127, 10, 127);
  VLOAD_8(v0, 5, 0, 0);
  CLEAR(v2);
  __asm__ volatile("vsmul.vv v2, v4, v6, v0.t");
  VEC_CMP_8(2, v2, 126, 0, -50);
}

void TEST_CASE3() {
  VSET(3, e8, m2);
  VLOAD_8(v4, 127, 63, -50);
  int8_t scalar = 55;
  __asm__ volatile("vsmul.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VEC_CMP_8(3, v2, 55, 27, -21);
}

void TEST_CASE4() {
  VSET(3, e8, m2);
  VLOAD_8(v4, 127, 127, -50);
  int8_t scalar = 55;
  VLOAD_8(v0, 5, 0, 0);
  CLEAR(v2);
  __asm__ volatile("vsmul.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VEC_CMP_8(4, v2, 55, 0, -21);
}

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();
  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  EXIT_CHECK();
}
