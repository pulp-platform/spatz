// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1() {
  uint64_t scalar = 99;

  VSET(16, e8, m8);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_8(v8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
  asm volatile("vslide1up.vx v8, v16, %[A]" ::[A] "r"(scalar));
  VCMP_U8(1, v8, 99, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

  VSET(16, e16, m8);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_16(v8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
  asm volatile("vslide1up.vx v8, v16, %[A]" ::[A] "r"(scalar));
  VCMP_U16(2, v8, 99, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

  VSET(16, e32, m8);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_32(v8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
  asm volatile("vslide1up.vx v8, v16, %[A]" ::[A] "r"(scalar));
  VCMP_U32(3, v8, 99, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_64(v8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
  asm volatile("vslide1up.vx v8, v16, %[A]" ::[A] "r"(scalar));
  VCMP_U64(4, v8, 99, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
#endif
}

void TEST_CASE2() {
  uint64_t scalar = 99;

  VSET(16, e8, m8);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_8(v8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vslide1up.vx v8, v16, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U8(5, v8, -1, 1, -1, 3, -1, 5, -1, 7, -1, 9, -1, 11, -1, 13, -1, 15);

  VSET(16, e16, m8);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_16(v8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
  VLOAD_8(v0, 0x55, 0x55);
  asm volatile("vslide1up.vx v8, v16, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U16(6, v8, 99, -1, 2, -1, 4, -1, 6, -1, 8, -1, 10, -1, 12, -1, 14, -1);

  VSET(16, e32, m8);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_32(v8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vslide1up.vx v8, v16, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U32(7, v8, -1, 1, -1, 3, -1, 5, -1, 7, -1, 9, -1, 11, -1, 13, -1, 15);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_64(v8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
  VLOAD_8(v0, 0x55, 0x55);
  asm volatile("vslide1up.vx v8, v16, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U64(8, v8, 99, -1, 2, -1, 4, -1, 6, -1, 8, -1, 10, -1, 12, -1, 14, -1);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  // TEST_CASE2();

  EXIT_CHECK();
}
