// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e8, m2);
  VLOAD_8(v2, 16, 0xef, 10, 0xff, 16, 0xef, 10, 0xff, 16, 0xef, 10, 0xff, 16,
          0xef, 10, 0xff);
  VLOAD_8(v4, 4, 0xef, 12, 0x80, 4, 0xef, 12, 0x80, 4, 0xef, 12, 0x80, 4, 0xef,
          12, 0x80);
  VLOAD_8(v0, 0x99, 0x99);
  asm volatile("vmsbc.vvm v6, v2, v4, v0");
  VSET(2, e8, m2);
  VCMP_U8(1, v6, 0x44, 0x44);

  VSET(16, e16, m2);
  VLOAD_16(v2, 16, 0xbeef, 10, 0xffff, 16, 0xbeef, 10, 0xffff, 16, 0xbeef, 10,
           0xffff, 16, 0xbeef, 10, 0xffff);
  VLOAD_16(v4, 4, 0xbeef, 12, 0x8000, 4, 0xbeef, 12, 0x8000, 4, 0xbeef, 12,
           0x8000, 4, 0xbeef, 12, 0x8000);
  VLOAD_8(v0, 0x99, 0x99);
  asm volatile("vmsbc.vvm v6, v2, v4, v0");
  VSET(2, e8, m2);
  VCMP_U8(2, v6, 0x44, 0x44);

  VSET(16, e32, m2);
  VLOAD_32(v2, 16, 0xdeadbeef, 10, 0xffffffff, 16, 0xdeadbeef, 10, 0xffffffff,
           16, 0xdeadbeef, 10, 0xffffffff, 16, 0xdeadbeef, 10, 0xffffffff);
  VLOAD_32(v4, 4, 0xdeadbeef, 12, 0x80000000, 4, 0xdeadbeef, 12, 0x80000000, 4,
           0xdeadbeef, 12, 0x80000000, 4, 0xdeadbeef, 12, 0x80000000);
  VLOAD_8(v0, 0x99, 0x99);
  asm volatile("vmsbc.vvm v6, v2, v4, v0");
  VSET(2, e8, m2);
  VCMP_U8(3, v6, 0x44, 0x44);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 16, 0xdeadbeefdeadbeef, 10, 0xffffffffffffffff, 16,
           0xdeadbeefdeadbeef, 10, 0xffffffffffffffff, 16, 0xdeadbeefdeadbeef,
           10, 0xffffffffffffffff, 16, 0xdeadbeefdeadbeef, 10,
           0xffffffffffffffff);
  VLOAD_64(v4, 4, 0xdeadbeefdeadbeef, 12, 0x8000000000000000, 4,
           0xdeadbeefdeadbeef, 12, 0x8000000000000000, 4, 0xdeadbeefdeadbeef,
           12, 0x8000000000000000, 4, 0xdeadbeefdeadbeef, 12,
           0x8000000000000000);
  VLOAD_8(v0, 0x99, 0x99);
  asm volatile("vmsbc.vvm v6, v2, v4, v0");
  VSET(2, e8, m2);
  VCMP_U8(4, v6, 0x44, 0x44);
#endif
};

void TEST_CASE2(void) {
  VSET(16, e8, m2);
  VLOAD_8(v2, 16, 0xef, 10, 0xff, 16, 0xef, 10, 0xff, 16, 0xef, 10, 0xff, 16,
          0xef, 10, 0xff);
  VLOAD_8(v4, 4, 0xef, 12, 0x80, 4, 0xef, 12, 0x80, 4, 0xef, 12, 0x80, 4, 0xef,
          12, 0x80);
  asm volatile("vmsbc.vv v6, v2, v4");
  VSET(2, e8, m2);
  VCMP_U8(5, v6, 0x44, 0x44);

  VSET(16, e16, m2);
  VLOAD_16(v2, 16, 0xbeef, 10, 0xffff, 16, 0xbeef, 10, 0xffff, 16, 0xbeef, 10,
           0xffff, 16, 0xbeef, 10, 0xffff);
  VLOAD_16(v4, 4, 0xbeef, 12, 0x8000, 4, 0xbeef, 12, 0x8000, 4, 0xbeef, 12,
           0x8000, 4, 0xbeef, 12, 0x8000);
  asm volatile("vmsbc.vv v6, v2, v4");
  VSET(2, e8, m2);
  VCMP_U8(6, v6, 0x44, 0x44);

  VSET(16, e32, m2);
  VLOAD_32(v2, 16, 0xdeadbeef, 10, 0xffffffff, 16, 0xdeadbeef, 10, 0xffffffff,
           16, 0xdeadbeef, 10, 0xffffffff, 16, 0xdeadbeef, 10, 0xffffffff);
  VLOAD_32(v4, 4, 0xdeadbeef, 12, 0x80000000, 4, 0xdeadbeef, 12, 0x80000000, 4,
           0xdeadbeef, 12, 0x80000000, 4, 0xdeadbeef, 12, 0x80000000);
  asm volatile("vmsbc.vv v6, v2, v4");
  VSET(2, e8, m2);
  VCMP_U8(7, v6, 0x44, 0x44);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 16, 0xdeadbeefdeadbeef, 10, 0xffffffffffffffff, 16,
           0xdeadbeefdeadbeef, 10, 0xffffffffffffffff, 16, 0xdeadbeefdeadbeef,
           10, 0xffffffffffffffff, 16, 0xdeadbeefdeadbeef, 10,
           0xffffffffffffffff);
  VLOAD_64(v4, 4, 0xdeadbeefdeadbeef, 12, 0x8000000000000000, 4,
           0xdeadbeefdeadbeef, 12, 0x8000000000000000, 4, 0xdeadbeefdeadbeef,
           12, 0x8000000000000000, 4, 0xdeadbeefdeadbeef, 12,
           0x8000000000000000);
  asm volatile("vmsbc.vv v6, v2, v4");
  VSET(2, e8, m2);
  VCMP_U8(8, v6, 0x44, 0x44);
#endif
};

void TEST_CASE3(void) {
  const uint64_t scalar = 20;

  VSET(16, e8, m2);
  VLOAD_8(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  VLOAD_8(v0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0);
  asm volatile("vmsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(9, v6, 0x22, 0x22);

  VSET(16, e16, m2);
  VLOAD_16(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  VLOAD_16(v0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0);
  asm volatile("vmsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(10, v6, 0x22, 0x22);

  VSET(16, e32, m2);
  VLOAD_32(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  VLOAD_32(v0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0);
  asm volatile("vmsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(11, v6, 0x22, 0x22);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  VLOAD_64(v0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0);
  asm volatile("vmsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(12, v6, 0x22, 0x22);
#endif
};

void TEST_CASE4(void) {
  const uint64_t scalar = 20;

  VSET(16, e8, m2);
  VLOAD_8(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  asm volatile("vmsbc.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(13, v6, 0x22, 0x22);

  VSET(16, e16, m2);
  VLOAD_16(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  asm volatile("vmsbc.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(14, v6, 0x22, 0x22);

  VSET(16, e32, m2);
  VLOAD_32(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  asm volatile("vmsbc.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(15, v6, 0x22, 0x22);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  asm volatile("vmsbc.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(16, v6, 0x22, 0x22);
#endif
};

void TEST_CASE5(void) {
  const uint32_t scalar = 5;

  // --- vmsbc.vv (no borrow-in) ---
  VSET(16, e8, m8);
  // vs2 < vs1 causes borrow: (0 < 1) -> 1, (10 < 5) -> 0
  VLOAD_8(v8,  0, 10, 0, 10, 0, 10, 0, 10, 0, 10, 0, 10, 0, 10, 0, 10);
  VLOAD_8(v16, 1,  5, 1,  5, 1,  5, 1,  5, 1,  5, 1,  5, 1,  5, 1,  5);
  VCLEAR(v24);
  asm volatile("vmsbc.vv v24, v8, v16");
  // Even elements borrow (1), odd do not (0) -> 0x55
  VCMP_U8(17, v24, 0x55, 0x55, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  // --- vmsbc.vvm (with borrow-in) ---
  VSET(16, e8, m8);
  // vs2 == vs1. Borrow occurs only if borrow-in is 1.
  VLOAD_8(v8,  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5);
  VLOAD_8(v16, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5);
  VLOAD_8(v0, 0xAA, 0xAA); // Borrow-in on odd elements
  VCLEAR(v24);
  asm volatile("vmsbc.vvm v24, v8, v16, v0");
  // 5 < 5 + 0 (false), 5 < 5 + 1 (true -> borrow) -> 0xAA
  VCMP_U8(18, v24, 0xAA, 0xAA, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  // --- vmsbc.vx (no borrow-in) ---
  VSET(16, e16, m8);
  // vs2 < scalar(5): (2 < 5) -> borrow, (10 < 5) -> no borrow
  VLOAD_16(v8, 2, 10, 2, 10, 2, 10, 2, 10, 2, 10, 2, 10, 2, 10, 2, 10);
  VCLEAR(v24);
  asm volatile("vmsbc.vx v24, v8, %[A]" ::[A] "r"(scalar));
  VCMP_U8(19, v24, 0x55, 0x55, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  // --- vmsbc.vxm (with borrow-in) ---
  VSET(16, e32, m8);
  // vs2 == scalar(5). Borrow occurs on odd elements where borrow-in = 1.
  VLOAD_32(v8, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vmsbc.vxm v24, v8, %[A], v0" ::[A] "r"(scalar));
  VCMP_U8(20, v24, 0xAA, 0xAA, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

#if ELEN == 64
  // --- vmsbc.vvm (e64) ---
  VSET(16, e64, m8);
  VLOAD_64(v8,  0, 10, 0, 10, 0, 10, 0, 10, 0, 10, 0, 10, 0, 10, 0, 10);
  VLOAD_64(v16, 1,  5, 1,  5, 1,  5, 1,  5, 1,  5, 1,  5, 1,  5, 1,  5);
  VLOAD_8(v0, 0x00, 0x00);
  VCLEAR(v24);
  asm volatile("vmsbc.vvm v24, v8, v16, v0");
  VCMP_U8(21, v24, 0x55, 0x55, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();

  EXIT_CHECK();
}
