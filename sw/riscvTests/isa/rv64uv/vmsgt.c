// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  const uint64_t scalar = 40;

  VSET(16, e8, m1);
  VLOAD_8(v2, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VCLEAR(v1);
  asm volatile("vmsgt.vx v1, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(1, v1, 0x99, 0x99);

  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VCLEAR(v1);
  asm volatile("vmsgt.vx v1, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(2, v1, 0x99, 0x99);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VCLEAR(v2);
  asm volatile("vmsgt.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(3, v2, 0x99, 0x99);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
             12345, -8, -25, 199);
    VCLEAR(v8);
    asm volatile("vmsgt.vx v8, v4, %[A]" ::[A] "r"(scalar));
    VSET(2, e8, m1);
    VCMP_U8(4, v8, 0x99, 0x99);
  #endif
};

void TEST_CASE2(void) {
  const uint64_t scalar = 40;

  VSET(16, e8, m1);
  VLOAD_8(v2, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v1);
  asm volatile("vmsgt.vx v1, v2, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(5, v1, 0xbb, 0xbb);

  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v1);
  asm volatile("vmsgt.vx v1, v2, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(6, v1, 0xbb, 0xbb);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsgt.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(7, v2, 0xbb, 0xbb);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
             12345, -8, -25, 199);
    VLOAD_8(v0, 0xCC, 0xCC);
    VCLEAR(v8);
    asm volatile("vmsgt.vx v8, v4, %[A], v0.t" ::[A] "r"(scalar));
    VSET(2, e8, m1);
    VCMP_U8(8, v8, 0xbb, 0xbb);
  #endif
};

void TEST_CASE3(void) {
  VSET(16, e8, m1);
  VLOAD_8(v2, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VCLEAR(v1);
  asm volatile("vmsgt.vi v1, v2, 15");
  VSET(2, e8, m1);
  VCMP_U8(9, v1, 0x99, 0x99);

  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VCLEAR(v1);
  asm volatile("vmsgt.vi v1, v2, 15");
  VSET(2, e8, m1);
  VCMP_U8(10, v1, 0x99, 0x99);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VCLEAR(v2);
  asm volatile("vmsgt.vi v2, v4, 15");
  VSET(2, e8, m1);
  VCMP_U8(11, v2, 0x99, 0x99);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
             12345, -8, -25, 199);
    VCLEAR(v8);
    asm volatile("vmsgt.vi v8, v4, 15");
    VSET(2, e8, m1);
    VCMP_U8(12, v8, 0x99, 0x99);
  #endif
};

void TEST_CASE4(void) {
  VSET(16, e8, m1);
  VLOAD_8(v2, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VLOAD_8(v0, 0x88, 0x88);
  VCLEAR(v4);
  asm volatile("vmsgt.vi v4, v2, 15, v0.t");
  VSET(2, e8, m1);
  VCMP_U8(13, v4, 0xff, 0xff);

  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0x88, 0x88);
  VCLEAR(v4);
  asm volatile("vmsgt.vi v4, v2, 15, v0.t");
  VSET(2, e8, m1);
  VCMP_U8(14, v4, 0xff, 0xff);

  VSET(16, e32, m2);
  VLOAD_32(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0x88, 0x88);
  VCLEAR(v4);
  asm volatile("vmsgt.vi v4, v2, 15, v0.t");
  VSET(2, e8, m1);
  VCMP_U8(15, v4, 0xff, 0xff);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
             12345, -8, -25, 199);
    VLOAD_8(v0, 0x88, 0x88);
    VCLEAR(v8);
    asm volatile("vmsgt.vi v8, v4, 15, v0.t");
    VSET(2, e8, m1);
    VCMP_U8(16, v8, 0xff, 0xff);
  #endif
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