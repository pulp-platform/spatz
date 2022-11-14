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
  asm volatile("vrsub.vi v6, v2, 10");
  VCMP_U8(1, v6, 5, 0, -5, -10, -15, -20, -25, -30, 5, 0, -5, -10, -15, -20,
          -25, -30);

  VSET(16, e16, m2);
  VLOAD_16(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vrsub.vi v6, v2, 10");
  VCMP_U16(2, v6, 5, 0, -5, -10, -15, -20, -25, -30, 5, 0, -5, -10, -15, -20,
           -25, -30);

  VSET(16, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vrsub.vi v6, v2, 10");
  VCMP_U32(3, v6, 5, 0, -5, -10, -15, -20, -25, -30, 5, 0, -5, -10, -15, -20,
           -25, -30);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vrsub.vi v6, v2, 10");
  VCMP_U64(4, v6, 5, 0, -5, -10, -15, -20, -25, -30, 5, 0, -5, -10, -15, -20,
           -25, -30);
#endif
}

void TEST_CASE2(void) {
  VSET(16, e8, m2);
  VLOAD_8(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0x33, 0x33);
  VCLEAR(v6);
  asm volatile("vrsub.vi v6, v2, 10, v0.t");
  VCMP_U8(5, v6, 5, 0, 0, 0, -15, -20, 0, 0, 5, 0, 0, 0, -15, -20, 0, 0);

  VSET(16, e16, m2);
  VLOAD_16(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0x33, 0x33);
  VCLEAR(v6);
  asm volatile("vrsub.vi v6, v2, 10, v0.t");
  VCMP_U16(6, v6, 5, 0, 0, 0, -15, -20, 0, 0, 5, 0, 0, 0, -15, -20, 0, 0);

  VSET(16, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0x33, 0x33);
  VCLEAR(v6);
  asm volatile("vrsub.vi v6, v2, 10, v0.t");
  VCMP_U32(7, v6, 5, 0, 0, 0, -15, -20, 0, 0, 5, 0, 0, 0, -15, -20, 0, 0);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0x33, 0x33);
  VCLEAR(v6);
  asm volatile("vrsub.vi v6, v2, 10, v0.t");
  VCMP_U64(8, v6, 5, 0, 0, 0, -15, -20, 0, 0, 5, 0, 0, 0, -15, -20, 0, 0);
#endif
}

void TEST_CASE3(void) {
  const uint64_t scalar = 25;

  VSET(16, e8, m2);
  VLOAD_8(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vrsub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VCMP_U8(9, v6, 20, 15, 10, 5, 0, -5, -10, -15, 20, 15, 10, 5, 0, -5, -10,
          -15);

  VSET(16, e16, m2);
  VLOAD_16(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vrsub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VCMP_U16(10, v6, 20, 15, 10, 5, 0, -5, -10, -15, 20, 15, 10, 5, 0, -5, -10,
           -15);

  VSET(16, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vrsub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VCMP_U32(11, v6, 20, 15, 10, 5, 0, -5, -10, -15, 20, 15, 10, 5, 0, -5, -10,
           -15);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vrsub.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VCMP_U64(12, v6, 20, 15, 10, 5, 0, -5, -10, -15, 20, 15, 10, 5, 0, -5, -10,
           -15);
#endif
}

void TEST_CASE4(void) {
  const uint64_t scalar = 25;

  VSET(16, e8, m2);
  VLOAD_8(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0x33, 0x33);
  VCLEAR(v6);
  asm volatile("vrsub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U8(13, v6, 20, 15, 0, 0, 0, -5, 0, 0, 20, 15, 0, 0, 0, -5, 0, 0);

  VSET(16, e16, m2);
  VLOAD_16(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0x33, 0x33);
  VCLEAR(v6);
  asm volatile("vrsub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U16(14, v6, 20, 15, 0, 0, 0, -5, 0, 0, 20, 15, 0, 0, 0, -5, 0, 0);

  VSET(16, e32, m2);
  VLOAD_32(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0x33, 0x33);
  VCLEAR(v6);
  asm volatile("vrsub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U32(15, v6, 20, 15, 0, 0, 0, -5, 0, 0, 20, 15, 0, 0, 0, -5, 0, 0);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0x33, 0x33);
  VCLEAR(v6);
  asm volatile("vrsub.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U64(16, v6, 20, 15, 0, 0, 0, -5, 0, 0, 20, 15, 0, 0, 0, -5, 0, 0);
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
