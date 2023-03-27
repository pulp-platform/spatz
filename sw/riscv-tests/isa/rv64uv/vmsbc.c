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

  VSET(16, e64, m2);
  VLOAD_64(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  VLOAD_64(v0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0);
  asm volatile("vmsbc.vxm v6, v2, %[A], v0" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(12, v6, 0x22, 0x22);
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

  VSET(16, e64, m2);
  VLOAD_64(v2, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25, 20, 10, 30, 25);
  asm volatile("vmsbc.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(16, v6, 0x22, 0x22);
};

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();

  EXIT_CHECK();
}
