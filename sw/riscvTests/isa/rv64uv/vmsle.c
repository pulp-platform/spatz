// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e16, m1);
  VLOAD_16(v2, 50, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_16(v3, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VCLEAR(v1);
  asm volatile("vmsle.vv v1, v2, v3");
  VSET(2, e8, m1);
  VCMP_U8(1, v1, 0xAB, 0xAA);

  VSET(16, e32, m2);
  VLOAD_32(v4, 50, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_32(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VCLEAR(v2);
  asm volatile("vmsle.vv v2, v4, v6");
  VSET(2, e8, m1);
  VCMP_U8(2, v2, 0xAB, 0xAA);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 50, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
             2560, -19900, 12345, -80, 2560, -19900);
    VLOAD_64(v12, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
             50, 7000, 400, 19901);
    VCLEAR(v8);
    asm volatile("vmsle.vv v8, v4, v12");
    VSET(2, e8, m1);
    VCMP_U8(3, v8, 0xAB, 0xAA);
  #endif
};

void TEST_CASE2(void) {
  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_16(v3, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v1);
  asm volatile("vmsle.vv v1, v2, v3, v0.t");
  VSET(2, e8, m1);
  VCMP_U8(4, v1, 0xBB, 0xBB);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_32(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsle.vv v2, v4, v6, v0.t");
  VSET(2, e8, m1);
  VCMP_U8(5, v2, 0xBB, 0xBB);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
             2560, -19900, 12345, -80, 2560, -19900);
    VLOAD_64(v12, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
             50, 7000, 400, 19901);
    VLOAD_8(v0, 0xCC, 0xCC);
    VCLEAR(v8);
    asm volatile("vmsle.vv v8, v4, v12, v0.t");
    VSET(2, e8, m1);
    VCMP_U8(6, v8, 0xBB, 0xBB);
  #endif
};

void TEST_CASE3(void) {
  const uint64_t scalar = 40;

  VSET(16, e8, m1);
  VLOAD_8(v2, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VCLEAR(v1);
  asm volatile("vmsle.vx v1, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(7, v1, 0x66, 0x66);

  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VCLEAR(v1);
  asm volatile("vmsle.vx v1, v2, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(8, v1, 0x66, 0x66);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VCLEAR(v2);
  asm volatile("vmsle.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(9, v2, 0x66, 0x66);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
             12345, -8, -25, 199);
    VCLEAR(v8);
    asm volatile("vmsle.vx v8, v4, %[A]" ::[A] "r"(scalar));
    VSET(2, e8, m1);
    VCMP_U8(10, v8, 0x66, 0x66);
  #endif
};

void TEST_CASE4(void) {
  const uint64_t scalar = 40;

  VSET(16, e8, m1);
  VLOAD_8(v2, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v1);
  asm volatile("vmsle.vx v1, v2, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(11, v1, 0x77, 0x77);

  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v1);
  asm volatile("vmsle.vx v1, v2, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(12, v1, 0x77, 0x77);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsle.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m1);
  VCMP_U8(13, v2, 0x77, 0x77);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
             12345, -8, -25, 199);
    VLOAD_8(v0, 0xCC, 0xCC);
    VCLEAR(v8);
    asm volatile("vmsle.vx v8, v4, %[A], v0.t" ::[A] "r"(scalar));
    VSET(2, e8, m1);
    VCMP_U8(14, v8, 0x77, 0x77);
  #endif
};

void TEST_CASE5(void) {
  VSET(16, e8, m1);
  VLOAD_8(v2, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VCLEAR(v1);
  asm volatile("vmsle.vi v1, v2, 15");
  VSET(2, e8, m1);
  VCMP_U8(15, v1, 0x66, 0x66);

  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VCLEAR(v1);
  asm volatile("vmsle.vi v1, v2, 15");
  VSET(2, e8, m1);
  VCMP_U8(16, v1, 0x66, 0x66);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VCLEAR(v2);
  asm volatile("vmsle.vi v2, v4, 15");
  VSET(2, e8, m1);
  VCMP_U8(17, v2, 0x66, 0x66);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
             12345, -8, -25, 199);
    VCLEAR(v8);
    asm volatile("vmsle.vi v8, v4, 15");
    VSET(2, e8, m1);
    VCMP_U8(18, v8, 0x66, 0x66);
  #endif
};

void TEST_CASE6(void) {
  VSET(16, e8, m1);
  VLOAD_8(v2, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v1);
  asm volatile("vmsle.vi v1, v2, 15, v0.t");
  VSET(2, e8, m1);
  VCMP_U8(19, v1, 0x77, 0x77);

  VSET(16, e16, m1);
  VLOAD_16(v2, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v1);
  asm volatile("vmsle.vi v1, v2, 15, v0.t");
  VSET(2, e8, m1);
  VCMP_U8(20, v1, 0x77, 0x77);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsle.vi v2, v4, 15, v0.t");
  VSET(2, e8, m1);
  VCMP_U8(21, v2, 0x77, 0x77);

  #if ELEN == 64
    VSET(16, e64, m4);
    VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
             12345, -8, -25, 199);
    VLOAD_8(v0, 0xCC, 0xCC);
    VCLEAR(v8);
    asm volatile("vmsle.vi v8, v4, 15, v0.t");
    VSET(2, e8, m1);
    VCMP_U8(22, v8, 0x77, 0x77);
  #endif
};

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