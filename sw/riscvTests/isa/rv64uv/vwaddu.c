// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e8, m2);
  VLOAD_8(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vwaddu.vv v6, v4, v8");
  VSET(16, e16, m2);
  VCMP_U16(1, v6, 2, 4, 6, 8, 10, 12, 14, 16, 2, 4, 6, 8, 10, 12, 14, 16);

  VSET(16, e16, m2);
  VLOAD_16(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vwaddu.vv v6, v4, v8");
  VSET(16, e32, m2);
  VCMP_U32(2, v6, 2, 4, 6, 8, 10, 12, 14, 16, 2, 4, 6, 8, 10, 12, 14, 16);

  VSET(16, e32, m2);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vwaddu.vv v6, v4, v8");
  VSET(16, e64, m2);
  VCMP_U64(3, v6, 2, 4, 6, 8, 10, 12, 14, 16, 2, 4, 6, 8, 10, 12, 14, 16);
}

void TEST_CASE2(void) {
  VSET(16, e8, m2);
  VLOAD_8(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.vv v6, v4, v8, v0.t");
  VSET(16, e16, m2);
  VCMP_U16(4, v6, 0, 4, 0, 8, 0, 12, 0, 16, 0, 4, 0, 8, 0, 12, 0, 16);

  VSET(16, e16, m2);
  VLOAD_16(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.vv v6, v4, v8, v0.t");
  VSET(16, e32, m2);
  VCMP_U32(5, v6, 0, 4, 0, 8, 0, 12, 0, 16, 0, 4, 0, 8, 0, 12, 0, 16);

  VSET(16, e32, m2);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.vv v6, v4, v8, v0.t");
  VSET(16, e64, m2);
  VCMP_U64(6, v6, 0, 4, 0, 8, 0, 12, 0, 16, 0, 4, 0, 8, 0, 12, 0, 16);
}

void TEST_CASE3(void) {
  const uint32_t scalar = 5;

  VSET(16, e8, m2);
  VLOAD_8(v4, 1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16);
  asm volatile("vwaddu.vx v6, v4, %[A]" ::[A] "r"(scalar));
  VSET(16, e16, m2);
  VCMP_U16(7, v6, 6, 259, 8, 257, 10, 255, 12, 253, 14, 251, 16, 249, 18, 247,
           20, 245);

  VSET(16, e16, m2);
  VLOAD_16(v4, 1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16);
  asm volatile("vwaddu.vx v6, v4, %[A]" ::[A] "r"(scalar));
  VSET(16, e32, m2);
  VCMP_U32(8, v6, 6, 65539, 8, 65537, 10, 65535, 12, 65533, 14, 65531, 16,
           65529, 18, 65527, 20, 65525);

  VSET(16, e32, m2);
  VLOAD_32(v4, 1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16);
  asm volatile("vwaddu.vx v6, v4, %[A]" ::[A] "r"(scalar));
  VSET(16, e64, m2);
  VCMP_U64(9, v6, 6, 4294967299, 8, 4294967297, 10, 4294967295, 12, 4294967293,
           14, 4294967291, 16, 4294967289, 18, 4294967287, 20, 4294967285);
}

void TEST_CASE4(void) {
  const uint32_t scalar = 5;

  VSET(16, e8, m2);
  VLOAD_8(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.vx v6, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(16, e16, m2);
  VCMP_U16(10, v6, 0, 7, 0, 9, 0, 11, 0, 13, 0, 7, 0, 9, 0, 11, 0, 13);

  VSET(16, e16, m2);
  VLOAD_16(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.vx v6, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(16, e32, m2);
  VCMP_U32(11, v6, 0, 7, 0, 9, 0, 11, 0, 13, 0, 7, 0, 9, 0, 11, 0, 13);

  VSET(16, e32, m2);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.vx v6, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(16, e64, m2);
  VCMP_U64(12, v6, 0, 7, 0, 9, 0, 11, 0, 13, 0, 7, 0, 9, 0, 11, 0, 13);
}

void TEST_CASE5(void) {
  VSET(16, e8, m2);
  VLOAD_16(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vwaddu.wv v6, v4, v8");
  VSET(16, e16, m2);
  VCMP_U16(13, v6, 2, 4, 6, 8, 10, 12, 14, 16, 2, 4, 6, 8, 10, 12, 14, 16);

  VSET(16, e16, m2);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vwaddu.wv v6, v4, v8");
  VSET(16, e32, m2);
  VCMP_U32(14, v6, 2, 4, 6, 8, 10, 12, 14, 16, 2, 4, 6, 8, 10, 12, 14, 16);

  VSET(16, e32, m2);
  VLOAD_64(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vwaddu.wv v6, v4, v8");
  VSET(16, e64, m2);
  VCMP_U64(15, v6, 2, 4, 6, 8, 10, 12, 14, 16, 2, 4, 6, 8, 10, 12, 14, 16);
}

void TEST_CASE6(void) {
  VSET(16, e8, m2);
  VLOAD_16(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.wv v6, v4, v8, v0.t");
  VSET(16, e16, m2);
  VCMP_U16(16, v6, 0, 4, 0, 8, 0, 12, 0, 16, 0, 4, 0, 8, 0, 12, 0, 16);

  VSET(16, e16, m2);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.wv v6, v4, v8, v0.t");
  VSET(16, e32, m2);
  VCMP_U32(17, v6, 0, 4, 0, 8, 0, 12, 0, 16, 0, 4, 0, 8, 0, 12, 0, 16);

  VSET(16, e32, m2);
  VLOAD_64(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.wv v6, v4, v8, v0.t");
  VSET(16, e64, m2);
  VCMP_U64(18, v6, 0, 4, 0, 8, 0, 12, 0, 16, 0, 4, 0, 8, 0, 12, 0, 16);
}

void TEST_CASE7(void) {
  const uint32_t scalar = 5;

  VSET(16, e8, m2);
  VLOAD_16(v4, 1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16);
  asm volatile("vwaddu.wx v6, v4, %[A]" ::[A] "r"(scalar));
  VSET(16, e16, m2);
  VCMP_U16(19, v6, 6, 3, 8, 1, 10, -1, 12, -3, 14, -5, 16, -7, 18, -9, 20, -11);

  VSET(16, e16, m2);
  VLOAD_32(v4, 1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16);
  asm volatile("vwaddu.wx v6, v4, %[A]" ::[A] "r"(scalar));
  VSET(16, e32, m2);
  VCMP_U32(20, v6, 6, 3, 8, 1, 10, -1, 12, -3, 14, -5, 16, -7, 18, -9, 20, -11);

  VSET(16, e32, m2);
  VLOAD_64(v4, 1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16);
  asm volatile("vwaddu.wx v6, v4, %[A]" ::[A] "r"(scalar));
  VSET(16, e64, m2);
  VCMP_U64(21, v6, 6, 3, 8, 1, 10, -1, 12, -3, 14, -5, 16, -7, 18, -9, 20, -11);
}

void TEST_CASE8(void) {
  const uint32_t scalar = 5;

  VSET(16, e8, m2);
  VLOAD_16(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.wx v6, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(16, e16, m2);
  VCMP_U16(22, v6, 0, 7, 0, 9, 0, 11, 0, 13, 0, 7, 0, 9, 0, 11, 0, 13);

  VSET(16, e16, m2);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.wx v6, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(16, e32, m2);
  VCMP_U32(23, v6, 0, 7, 0, 9, 0, 11, 0, 13, 0, 7, 0, 9, 0, 11, 0, 13);

  VSET(16, e32, m2);
  VLOAD_64(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwaddu.wx v6, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(16, e64, m2);
  VCMP_U64(24, v6, 0, 7, 0, 9, 0, 11, 0, 13, 0, 7, 0, 9, 0, 11, 0, 13);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  // SKIP 2,4: masking not supported
  TEST_CASE1();
  // TEST_CASE2();
  TEST_CASE3();
  // TEST_CASE4();
  // SKIP 5-8: vwaddu.wv and vwaddu.wx not supported
  // TEST_CASE5();
  // TEST_CASE6();
  // TEST_CASE7();
  // TEST_CASE8();

  EXIT_CHECK();
}
