// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1() {
  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.v v16, v8");
  VCMP_U8(1, v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v4, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1);
  asm volatile("vmv.v.v v16, v8");
  VCMP_U16(2, v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v4, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1);
  asm volatile("vmv.v.v v16, v8");
  VCMP_U32(3, v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v4, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1);
  asm volatile("vmv.v.v v16, v8");
  VCMP_U64(4, v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
#endif
}

void TEST_CASE2() {
  const uint32_t scalar = 0xdeadbeef;

  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.x v16, %[A]" ::[A] "r"(scalar));
  VCMP_U8(5, v16, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef,
          0xef, 0xef, 0xef, 0xef, 0xef, 0xef);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.x v16, %[A]" ::[A] "r"(scalar));
  VCMP_U16(6, v16, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef);

  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.x v16, %[A]" ::[A] "r"(scalar));
  VCMP_U32(7, v16, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.x v16, %[A]" ::[A] "r"(scalar));
  VCMP_U64(8, v16, 0xffffffffdeadbeef, 0xffffffffdeadbeef, 0xffffffffdeadbeef,
           0xffffffffdeadbeef, 0xffffffffdeadbeef, 0xffffffffdeadbeef,
           0xffffffffdeadbeef, 0xffffffffdeadbeef, 0xffffffffdeadbeef,
           0xffffffffdeadbeef, 0xffffffffdeadbeef, 0xffffffffdeadbeef,
           0xffffffffdeadbeef, 0xffffffffdeadbeef, 0xffffffffdeadbeef,
           0xffffffffdeadbeef);
#endif
}

void TEST_CASE3() {
  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.i v16, -9");
  VCMP_U8(9, v16, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9,
          -9);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.i v16, -10");
  VCMP_U16(10, v16, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10,
           -10, -10, -10, -10);

  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.i v16, -11");
  VCMP_U32(11, v16, -11, -11, -11, -11, -11, -11, -11, -11, -11, -11, -11, -11,
           -11, -11, -11, -11);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.i v16, -12");
  VCMP_U64(12, v16, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12,
           -12, -12, -12, -12);
#endif
}

void TEST_CASE4 () {
  // Integer scalar move instructions
  uint32_t scalar = 0xdeadbeef;

  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.s.x v8, %[a]" :: [a] "r"(scalar));
  VCMP_U8(13, v8, 0x000000ef, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.s.x v8, %[a]" :: [a] "r"(scalar));
  VCMP_U16(14, v8, 0x0000beef, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.s.x v8, %[a]" :: [a] "r"(scalar));
  VCMP_U32(15, v8, 0xdeadbeef, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.s.x v8, %[a]" :: [a] "r"(scalar));
  VCMP_U64(16, v8, 0xffffffffdeadbeef, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
#endif
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
