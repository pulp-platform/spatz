// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e8, m2);
  VLOAD_8(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v4, 8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vsbc.vvm v6, v2, v4, v0");
  VCMP_U8(1, v6, -7, -6, -3, -2, 1, 2, 5, 6, 0, -1, 0, -1, 0, -1, 0, -1);

  VSET(16, e16, m2);
  VLOAD_16(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v4, 8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vsbc.vvm v6, v2, v4, v0");
  VCMP_U16(2, v6, -7, -6, -3, -2, 1, 2, 5, 6, 0, -1, 0, -1, 0, -1, 0, -1);

  VSET(16, e32, m2);
  VLOAD_32(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v4, 8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vsbc.vvm v6, v2, v4, v0");
  VCMP_U32(3, v6, -7, -6, -3, -2, 1, 2, 5, 6, 0, -1, 0, -1, 0, -1, 0, -1);

  VSET(16, e64, m2);
  VLOAD_64(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v4, 8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vsbc.vvm v6, v2, v4, v0");
  VCMP_U64(4, v6, -7, -6, -3, -2, 1, 2, 5, 6, 0, -1, 0, -1, 0, -1, 0, -1);
};

void TEST_CASE2(void) {
  const uint32_t scalar = 5;

  VSET(16, e8, m2);
  VLOAD_8(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VCMP_U8(5, v6, -4, -4, -2, -2, 0, 0, 2, 2, -4, -4, -2, -2, 0, 0, 2, 2);

  VSET(16, e16, m2);
  VLOAD_16(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VCMP_U16(6, v6, -4, -4, -2, -2, 0, 0, 2, 2, -4, -4, -2, -2, 0, 0, 2, 2);

  VSET(16, e32, m2);
  VLOAD_32(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VCMP_U32(7, v6, -4, -4, -2, -2, 0, 0, 2, 2, -4, -4, -2, -2, 0, 0, 2, 2);

  VSET(16, e64, m2);
  VLOAD_64(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VCMP_U64(8, v6, -4, -4, -2, -2, 0, 0, 2, 2, -4, -4, -2, -2, 0, 0, 2, 2);
};

void TEST_CASE3(void) {
  const uint32_t scalar = 5;

  // --- vsbc.vvm (e8) ---
  VSET(16, e8, m8);
  VLOAD_8(v8,  10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50, 60, 70, 80);
  VLOAD_8(v16,  1,  2,  3,  4,  5,  6,  7,  8,  1,  2,  3,  4,  5,  6,  7,  8);
  VLOAD_8(v0, 0xAA, 0xAA); // Borrow-in on odd elements (10101010)
  VCLEAR(v24);
  asm volatile("vsbc.vvm v24, v8, v16, v0");
  // Even: v8 - v16 - 0; Odd: v8 - v16 - 1
  VCMP_U8(9, v24, 9, 17, 27, 35, 45, 53, 63, 71, 9, 17, 27, 35, 45, 53, 63, 71);

  // --- vsbc.vvm (e16) ---
  VSET(16, e16, m8);
  VLOAD_16(v8,  10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50, 60, 70, 80);
  VLOAD_16(v16,  1,  2,  3,  4,  5,  6,  7,  8,  1,  2,  3,  4,  5,  6,  7,  8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vsbc.vvm v24, v8, v16, v0");
  VCMP_U16(10, v24, 9, 17, 27, 35, 45, 53, 63, 71, 9, 17, 27, 35, 45, 53, 63, 71);

  // --- vsbc.vxm (e32) ---
  VSET(16, e32, m8);
  VLOAD_32(v8, 10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50, 60, 70, 80);
  VLOAD_8(v0, 0xFF, 0xFF); // Borrow-in = 1 for all elements
  VCLEAR(v24);
  asm volatile("vsbc.vxm v24, v8, %[A], v0" ::[A] "r"(scalar));
  // v8 - scalar(5) - 1 = v8 - 6
  VCMP_U32(11, v24, 4, 14, 24, 34, 44, 54, 64, 74, 4, 14, 24, 34, 44, 54, 64, 74);

#if ELEN == 64
  // --- vsbc.vvm (e64) ---
  VSET(16, e64, m8);
  VLOAD_64(v8,  10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50, 60, 70, 80);
  VLOAD_64(v16,  1,  2,  3,  4,  5,  6,  7,  8,  1,  2,  3,  4,  5,  6,  7,  8);
  VLOAD_8(v0, 0x00, 0x00); // Borrow-in = 0
  VCLEAR(v24);
  asm volatile("vsbc.vvm v24, v8, v16, v0");
  VCMP_U64(12, v24, 9, 18, 27, 36, 45, 54, 63, 72, 9, 18, 27, 36, 45, 54, 63, 72);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();

  EXIT_CHECK();
}
