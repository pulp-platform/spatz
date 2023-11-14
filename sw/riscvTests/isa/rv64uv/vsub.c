// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e8, m8);
  VLOAD_8(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vsub.vv v24, v8, v16");
  VCMP_U8(1, v24, 4, 8, 12, 16, 20, 24, 28, 32, 4, 8, 12, 16, 20, 24, 28, 32);

  VSET(16, e16, m8);
  VLOAD_16(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vsub.vv v24, v8, v16");
  VCMP_U16(2, v24, 4, 8, 12, 16, 20, 24, 28, 32, 4, 8, 12, 16, 20, 24, 28, 32);

  VSET(16, e32, m8);
  VLOAD_32(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vsub.vv v24, v8, v16");
  VCMP_U32(3, v24, 4, 8, 12, 16, 20, 24, 28, 32, 4, 8, 12, 16, 20, 24, 28, 32);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vsub.vv v24, v8, v16");
  VCMP_U64(4, v24, 4, 8, 12, 16, 20, 24, 28, 32, 4, 8, 12, 16, 20, 24, 28, 32);
#endif
}

void TEST_CASE2(void) {
  VSET(16, e8, m8);
  VLOAD_8(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsub.vv v24, v8, v16, v0.t");
  VCMP_U8(5, v24, 0, 8, 0, 16, 0, 24, 0, 32, 0, 8, 0, 16, 0, 24, 0, 32);

  VSET(16, e16, m8);
  VLOAD_16(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsub.vv v24, v8, v16, v0.t");
  VCMP_U16(6, v24, 0, 8, 0, 16, 0, 24, 0, 32, 0, 8, 0, 16, 0, 24, 0, 32);

  VSET(16, e32, m8);
  VLOAD_32(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsub.vv v24, v8, v16, v0.t");
  VCMP_U32(7, v24, 0, 8, 0, 16, 0, 24, 0, 32, 0, 8, 0, 16, 0, 24, 0, 32);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsub.vv v24, v8, v16, v0.t");
  VCMP_U64(8, v24, 0, 8, 0, 16, 0, 24, 0, 32, 0, 8, 0, 16, 0, 24, 0, 32);
#endif
}

void TEST_CASE3(void) {
  const uint64_t scalar = 5;

  VSET(16, e8, m8);
  VLOAD_8(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vsub.vx v24, v8, %[A]" ::[A] "r"(scalar));
  VCMP_U8(9, v24, 0, 5, 10, 15, 20, 25, 30, 35, 0, 5, 10, 15, 20, 25, 30, 35);

  VSET(16, e16, m8);
  VLOAD_16(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vsub.vx v24, v8, %[A]" ::[A] "r"(scalar));
  VCMP_U16(10, v24, 0, 5, 10, 15, 20, 25, 30, 35, 0, 5, 10, 15, 20, 25, 30, 35);

  VSET(16, e32, m8);
  VLOAD_32(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vsub.vx v24, v8, %[A]" ::[A] "r"(scalar));
  VCMP_U32(11, v24, 0, 5, 10, 15, 20, 25, 30, 35, 0, 5, 10, 15, 20, 25, 30, 35);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  asm volatile("vsub.vx v24, v8, %[A]" ::[A] "r"(scalar));
  VCMP_U64(12, v24, 0, 5, 10, 15, 20, 25, 30, 35, 0, 5, 10, 15, 20, 25, 30, 35);
#endif
}

void TEST_CASE4(void) {
  const uint64_t scalar = 5;

  VSET(16, e8, m8);
  VLOAD_8(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsub.vx v24, v8, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U8(13, v24, 0, 5, 0, 15, 0, 25, 0, 35, 0, 5, 0, 15, 0, 25, 0, 35);

  VSET(16, e16, m8);
  VLOAD_16(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsub.vx v24, v8, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U16(14, v24, 0, 5, 0, 15, 0, 25, 0, 35, 0, 5, 0, 15, 0, 25, 0, 35);

  VSET(16, e32, m8);
  VLOAD_32(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsub.vx v24, v8, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U32(15, v24, 0, 5, 0, 15, 0, 25, 0, 35, 0, 5, 0, 15, 0, 25, 0, 35);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 5, 10, 15, 20, 25, 30, 35, 40, 5, 10, 15, 20, 25, 30, 35, 40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsub.vx v24, v8, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U64(16, v24, 0, 5, 0, 15, 0, 25, 0, 35, 0, 5, 0, 15, 0, 25, 0, 35);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  // TEST_CASE2();
  TEST_CASE3();
  // TEST_CASE4();

  EXIT_CHECK();
}
