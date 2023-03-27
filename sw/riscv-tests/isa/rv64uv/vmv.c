// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1() {
  VSET(16, e8, m2);
  VLOAD_8(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.v v6, v2");
  VCMP_U8(1, v6, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(16, e16, m2);
  VLOAD_16(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v4, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1);
  asm volatile("vmv.v.v v6, v2");
  VCMP_U16(2, v6, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(16, e32, m2);
  VLOAD_32(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v4, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1);
  asm volatile("vmv.v.v v6, v2");
  VCMP_U32(3, v6, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v4, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1);
  asm volatile("vmv.v.v v6, v2");
  VCMP_U64(4, v6, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
#endif
}

void TEST_CASE2() {
  const uint32_t scalar = 0xdeadbeef;

  VSET(16, e8, m2);
  VLOAD_8(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.x v6, %[A]" ::[A] "r"(scalar));
  VCMP_U8(5, v6, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef,
          0xef, 0xef, 0xef, 0xef, 0xef, 0xef);

  VSET(16, e16, m2);
  VLOAD_16(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.x v6, %[A]" ::[A] "r"(scalar));
  VCMP_U16(6, v6, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef);

  VSET(16, e32, m2);
  VLOAD_32(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.x v6, %[A]" ::[A] "r"(scalar));
  VCMP_U32(7, v6, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.x v6, %[A]" ::[A] "r"(scalar));
  VCMP_U64(8, v6, 0x00000000deadbeef, 0x00000000deadbeef, 0x00000000deadbeef,
           0x00000000deadbeef, 0x00000000deadbeef, 0x00000000deadbeef,
           0x00000000deadbeef, 0x00000000deadbeef, 0x00000000deadbeef,
           0x00000000deadbeef, 0x00000000deadbeef, 0x00000000deadbeef,
           0x00000000deadbeef, 0x00000000deadbeef, 0x00000000deadbeef,
           0x00000000deadbeef);
#endif
}

void TEST_CASE3() {
  VSET(16, e8, m2);
  VLOAD_8(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.i v6, -9");
  VCMP_U8(9, v6, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9,
          -9);

  VSET(16, e16, m2);
  VLOAD_16(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.i v6, -10");
  VCMP_U16(10, v6, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10,
           -10, -10, -10, -10);

  VSET(16, e32, m2);
  VLOAD_32(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.i v6, -11");
  VCMP_U32(11, v6, -11, -11, -11, -11, -11, -11, -11, -11, -11, -11, -11, -11,
           -11, -11, -11, -11);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vmv.v.i v6, -12");
  VCMP_U64(12, v6, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12,
           -12, -12, -12, -12);
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
