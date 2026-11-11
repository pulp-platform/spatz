// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"
#define REPEAT7(stmt) do { for (int _i = 0; _i < 7; _i++) { stmt; } } while (0)

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

void TEST_CASE5(void) {
  volatile uint8_t buf[64] __attribute__((aligned(64)));   // local: lives in L1 (stack)
  for (int i = 0; i < 64; i++) buf[i] = 0xff;
  VSET(4, e8, m1);
  asm volatile("vmv.v.i v0, -1");
  register uintptr_t base asm("a0") = (uintptr_t)buf;
  REPEAT7(asm volatile(".word 0x00050007" :: "r"(base) : "memory"));  // vle8.v v0, (a0), v0.t
  VCMP_U8(17, v0, 0xff, 0xff, 0xff, 0xff);
  VLOAD_8(v1, 1, 2, 3, 4);
  VCMP_U8(18, v1, 1, 2, 3, 4);
}

void TEST_CASE6(void) {
  volatile uint8_t buf[64] __attribute__((aligned(64)));   // local: lives in L1 (stack)
  for (int i = 0; i < 64; i++) buf[i] = 0xff;
  VSET(4, e8, m1);
  asm volatile("vmv.v.i v0, -1");
  REPEAT7(asm volatile("vle8.v v2, (%0), v0.t" :: "r"(buf) : "memory"));
  VCMP_U8(19, v2, 0xff, 0xff, 0xff, 0xff);
  VLOAD_8(v1, 1, 2, 3, 4);
  VCMP_U8(20, v1, 1, 2, 3, 4);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();
  TEST_CASE6();

  EXIT_CHECK();
}
