// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  uint32_t vxsat;
  VSET(4, e8, m1);
  VLOAD_8(v1, 133, 2, 220, 4);
  VLOAD_8(v2, 133, 2, 50, 4);
  __asm__ volatile("vsaddu.vv v3, v1, v2" ::);
  VCMP_U8(1, v3, 255, 4, 255, 8);
  read_vxsat(vxsat);
  check_vxsat(1, vxsat, 1);
  reset_vxsat;
}

void TEST_CASE2(void) {
  uint32_t vxsat;
  VSET(4, e8, m1);
  VLOAD_8(v1, 1, 2, 3, 154);
  VLOAD_8(v2, 1, 2, 3, 124);
  VLOAD_8(v0, 0xA, 0x0, 0x0, 0x0);
  VCLEAR(v3);
  __asm__ volatile("vsaddu.vv v3, v1, v2, v0.t" ::);
  VCMP_U8(2, v3, 0, 4, 0, 255);
  read_vxsat(vxsat);
  check_vxsat(2, vxsat, 1);
  reset_vxsat;
}

void TEST_CASE3(void) {
  uint32_t vxsat;
  VSET(4, e32, m1);
  VLOAD_32(v1, 1, 0xFFFFFFFB, 3, 4);
  __asm__ volatile("vsaddu.vi v3, v1, 5" ::);
  VCMP_U32(3, v3, 6, 0xFFFFFFFF, 8, 9);
  read_vxsat(vxsat);
  check_vxsat(3, vxsat, 1);
  reset_vxsat;
}

// Dont use VCLEAR here, it results in a glitch where are values are off by 1
void TEST_CASE4(void) {
  uint32_t vxsat;
  VSET(4, e32, m1);
  VLOAD_32(v1, 1, 2, 0xFFFFFFFD, 0xFFFFFFFC);
  VLOAD_32(v0, 0xA, 0x0, 0x0, 0x0);
  VCLEAR(v3);
  __asm__ volatile("vsaddu.vi v3, v1, 5, v0.t" ::);
  VCMP_U32(4, v3, 0, 7, 0, 0xFFFFFFFF);
  read_vxsat(vxsat);
  check_vxsat(4, vxsat, 1);
  reset_vxsat;
}

void TEST_CASE5(void) {
  uint32_t vxsat;
  VSET(4, e32, m1);
  VLOAD_32(v1, 0xFFFFFFFD, 2, 3, 4);
  const uint32_t scalar = 5;
  __asm__ volatile("vsaddu.vx v3, v1, %[A]" ::[A] "r"(scalar));
  VCMP_U32(5, v3, 0xFFFFFFFF, 7, 8, 9);
  read_vxsat(vxsat);
  check_vxsat(5, vxsat, 1);
  reset_vxsat;
}

// Dont use VCLEAR here, it results in a glitch where are values are off by 1
void TEST_CASE6(void) {
  uint32_t vxsat;
  VSET(4, e32, m1);
  VLOAD_32(v1, 1, 0xfffffffC, 3, 4);
  const uint32_t scalar = 5;
  VLOAD_32(v0, 0xA, 0x0, 0x0, 0x0);
  VCLEAR(v3);
  __asm__ volatile("vsaddu.vx v3, v1, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U32(6, v3, 0, 0xFFFFFFFF, 0, 9);
  read_vxsat(vxsat);
  check_vxsat(6, vxsat, 1);
  reset_vxsat;
}

void TEST_CASE7(void) {
  uint32_t vxsat;
  VSET(4, e32, m1);
  VLOAD_32(v1, 1, 0x0000FFFF, 3, 4);
  VLOAD_32(v2, 0xA, 0xFFFF0000, 0x0, 0x0);
  VCLEAR(v3);
  __asm__ volatile("vsaddu.vv v3, v1, v2" ::);
  VCMP_U32(7, v3, 0xB, 0xFFFFFFFF, 3, 4);
  read_vxsat(vxsat);
  check_vxsat(7, vxsat, 0);
  reset_vxsat;
}

void TEST_CASE8(void) {
  uint32_t out_vxsat = 0;   // CSRs are 32-bit on Snitch; a 64-bit variable leaves the high half undefined

  VSET(4, e8, m1);
  VLOAD_8(v1, 255, 255, 255, 255);
  VLOAD_8(v2, 255, 255, 255, 255);

  // Case 30: a read must see an older saturation
  reset_vxsat;
  asm volatile("vsaddu.vv v3, v1, v2" ::: "v3");
  read_vxsat(out_vxsat);
  XCMP(8, out_vxsat, 1);   // bug: reads 0 (read overtakes the saturation)

  // Case 31: a clear must not be undone by an older saturation
  reset_vxsat;
  asm volatile("vsaddu.vv v3, v1, v2" ::: "v3");
  reset_vxsat;
  read_vxsat(out_vxsat);
  XCMP(9, out_vxsat, 0);   // bug: reads 1 (late saturation sets it again)
}

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();
  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();
  TEST_CASE6();
  TEST_CASE7();
  TEST_CASE8();
  EXIT_CHECK();
}