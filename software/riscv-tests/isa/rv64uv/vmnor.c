// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1() {
  VSET(16, e8, m2);
  VLOAD_8(v4, 0xCD, 0xEF);
  VLOAD_8(v6, 0x84, 0x21);
  asm volatile("vmnor.mm v2, v4, v6");
  VSET(2, e8, m2);
  VCMP_U8(1, v2, 0x32, 0x10);
}

void TEST_CASE2() {
  VSET(16, e8, m2);
  VLOAD_8(v4, 0xCD, 0xEF);
  VLOAD_8(v6, 0xFF, 0xFF);
  asm volatile("vmnor.mm v2, v4, v6");
  VSET(2, e8, m2);
  VCMP_U8(2, v2, 0x00, 0x00);
}

void TEST_CASE3() {
  VSET(16, e8, m2);
  VLOAD_8(v4, 0xCD, 0xEF);
  VLOAD_8(v6, 0x00, 0x00);
  asm volatile("vmnor.mm v2, v4, v6");
  VSET(2, e8, m2);
  VCMP_U8(3, v2, 0x32, 0x10);
}

void TEST_CASE4() {
  VSET(16, e8, m2);
  VLOAD_8(v4, 0xCD, 0xEF);
  VLOAD_8(v6, 0x0F, 0xF0);
  asm volatile("vmnor.mm v2, v4, v6");
  VSET(2, e8, m2);
  VCMP_U8(4, v2, 0x30, 0x00);
}

void TEST_CASE5() {
  VSET(16, e8, m2);
  VLOAD_8(v2, 0xFF, 0xFF);
  VLOAD_8(v4, 0xCD, 0xEF);
  VLOAD_8(v6, 0x84, 0x21);
  VSET(13, e8, m2);
  asm volatile("vmnor.mm v2, v4, v6");
  VSET(2, e8, m2);
  VCMP_U8(5, v2, 0x32, 0xF0);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();

  EXIT_CHECK();
}
