// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e8, m2);
  VLOAD_8(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vsub.vv v6, v2, v4");
  VCMP_U8(1, v6, 4, 8, 12, 16, 20, 24, 28, 32, 4, 8, 12, 16, 20, 24, 28, 32);

  VSET(16, e16, m2);
  VLOAD_16(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_16(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vsub.vv v6, v2, v4");
  VCMP_U16(2, v6, 4, 8, 12, 16, 20, 24, 28, 32, 4, 8, 12, 16, 20, 24, 28, 32);

  VSET(16, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vsub.vv v6, v2, v4");
  VCMP_U32(3, v6, 4, 8, 12, 16, 20, 24, 28, 32, 4, 8, 12, 16, 20, 24, 28, 32);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_64(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vsub.vv v6, v2, v4");
  VCMP_U64(4, v6, 4, 8, 12, 16, 20, 24, 28, 32, 4, 8, 12, 16, 20, 24, 28, 32);
#endif
}

void TEST_CASE2(void) {
  VSET(16, e8, m2);
  VLOAD_8(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vsub.vv v6, v2, v4, v0.t");
  VCMP_U8(5, v6, 0, 8, 0, 16, 0, 24, 0, 32, 0, 8, 0, 16, 0, 24, 0, 32);

  VSET(16, e16, m2);
  VLOAD_16(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_16(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vsub.vv v6, v2, v4, v0.t");
  VCMP_U16(6, v6, 0, 8, 0, 16, 0, 24, 0, 32, 0, 8, 0, 16, 0, 24, 0, 32);

  VSET(16, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vsub.vv v6, v2, v4, v0.t");
  VCMP_U32(7, v6, 0, 8, 0, 16, 0, 24, 0, 32, 0, 8, 0, 16, 0, 24, 0, 32);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_64(v4, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vsub.vv v6, v2, v4, v0.t");
  VCMP_U64(8, v6, 0, 8, 0, 16, 0, 24, 0, 32, 0, 8, 0, 16, 0, 24, 0, 32);
#endif
}

void TEST_CASE3(void) {
  const uint64_t scalar = 5;

  VSET(16, e8, m2);
  VLOAD_8(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vsub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VCMP_U8(9, v6, 0, 5, 10, 15, 20, 25, 30, 35, 0, 5, 10, 15, 20, 25, 30, 35);

  VSET(16, e16, m2);
  VLOAD_16(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vsub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VCMP_U16(10, v6, 0, 5, 10, 15, 20, 25, 30, 35, 0, 5, 10, 15, 20, 25, 30, 35);

  VSET(16, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vsub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VCMP_U32(11, v6, 0, 5, 10, 15, 20, 25, 30, 35, 0, 5, 10, 15, 20, 25, 30, 35);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vsub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VCMP_U64(12, v6, 0, 5, 10, 15, 20, 25, 30, 35, 0, 5, 10, 15, 20, 25, 30, 35);
#endif
}

void TEST_CASE4(void) {
  const uint64_t scalar = 5;

  VSET(16, e8, m2);
  VLOAD_8(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vsub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U8(13, v6, 0, 5, 0, 15, 0, 25, 0, 35, 0, 5, 0, 15, 0, 25, 0, 35);

  VSET(16, e16, m2);
  VLOAD_16(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vsub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U16(14, v6, 0, 5, 0, 15, 0, 25, 0, 35, 0, 5, 0, 15, 0, 25, 0, 35);

  VSET(16, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vsub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U32(15, v6, 0, 5, 0, 15, 0, 25, 0, 35, 0, 5, 0, 15, 0, 25, 0, 35);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vsub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U64(16, v6, 0, 5, 0, 15, 0, 25, 0, 35, 0, 5, 0, 15, 0, 25, 0, 35);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  //TEST_CASE2();
  TEST_CASE3();
  //TEST_CASE4();

  EXIT_CHECK();
}
