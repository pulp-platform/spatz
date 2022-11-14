// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e16, m2);
  VLOAD_16(v4, 50, 80, 2560, 19900, 12345, 80, 2560, 19900, 12345, 80, 2560,
           19900, 12345, 80, 2560, 19900);
  VLOAD_16(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VCLEAR(v2);
  asm volatile("vmsleu.vv v2, v4, v6");
  VSET(2, e8, m2);
  VCMP_U8(1, v2, 0xAB, 0xAA);

  VSET(16, e32, m2);
  VLOAD_32(v4, 50, 80, 2560, 19900, 12345, 80, 2560, 19900, 12345, 80, 2560,
           19900, 12345, 80, 2560, 19900);
  VLOAD_32(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VCLEAR(v2);
  asm volatile("vmsleu.vv v2, v4, v6");
  VSET(2, e8, m2);
  VCMP_U8(2, v2, 0xAB, 0xAA);

  VSET(16, e64, m2);
  VLOAD_64(v4, 50, 80, 2560, 19900, 12345, 80, 2560, 19900, 12345, 80, 2560,
           19900, 12345, 80, 2560, 19900);
  VLOAD_64(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VCLEAR(v2);
  asm volatile("vmsleu.vv v2, v4, v6");
  VSET(2, e8, m2);
  VCMP_U8(3, v2, 0xAB, 0xAA);
};

void TEST_CASE2(void) {
  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, 80, 2560, 19900, 12345, 80, 2560, 19900, 12345, 80, 2560,
           19900, 12345, 80, 2560, 19900);
  VLOAD_16(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vv v2, v4, v6, v0.t");
  VSET(2, e8, m2);
  VCMP_U8(4, v2, 0x88, 0x88);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, 80, 2560, 19900, 12345, 80, 2560, 19900, 12345, 80, 2560,
           19900, 12345, 80, 2560, 19900);
  VLOAD_32(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vv v2, v4, v6, v0.t");
  VSET(2, e8, m2);
  VCMP_U8(5, v2, 0x88, 0x88);

  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, 80, 2560, 19900, 12345, 80, 2560, 19900, 12345, 80, 2560,
           19900, 12345, 80, 2560, 19900);
  VLOAD_64(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vv v2, v4, v6, v0.t");
  VSET(2, e8, m2);
  VCMP_U8(6, v2, 0x88, 0x88);
};

void TEST_CASE3(void) {
  const uint64_t scalar = 40;

  VSET(16, e8, m2);
  VLOAD_8(v4, 123, 8, 25, 199, 123, 8, 25, 199, 123, 8, 25, 199, 123, 8, 25,
          199);
  VCLEAR(v2);
  asm volatile("vmsleu.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(7, v2, 0x66, 0x66);

  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VCLEAR(v2);
  asm volatile("vmsleu.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(8, v2, 0x66, 0x66);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VCLEAR(v2);
  asm volatile("vmsleu.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(9, v2, 0x66, 0x66);

  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VCLEAR(v2);
  asm volatile("vmsleu.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(10, v2, 0x66, 0x66);
};

void TEST_CASE4(void) {
  const uint64_t scalar = 40;

  VSET(16, e8, m2);
  VLOAD_8(v4, 123, 8, 25, 199, 123, 8, 25, 199, 123, 8, 25, 199, 123, 8, 25,
          199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(11, v2, 0x44, 0x44);

  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(12, v2, 0x44, 0x44);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(13, v2, 0x44, 0x44);

  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VSET(2, e8, m2);
  VCMP_U8(14, v2, 0x44, 0x44);
};

void TEST_CASE5(void) {
  VSET(16, e8, m2);
  VLOAD_8(v4, 123, 8, 25, 199, 123, 8, 25, 199, 123, 8, 25, 199, 123, 8, 25,
          199);
  VCLEAR(v2);
  asm volatile("vmsleu.vi v2, v4, 15");
  VSET(2, e8, m2);
  VCMP_U8(15, v2, 0x22, 0x22);

  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VCLEAR(v2);
  asm volatile("vmsleu.vi v2, v4, 15");
  VSET(2, e8, m2);
  VCMP_U8(16, v2, 0x22, 0x22);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VCLEAR(v2);
  asm volatile("vmsleu.vi v2, v4, 15");
  VSET(2, e8, m2);
  VCMP_U8(17, v2, 0x22, 0x22);

  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VCLEAR(v2);
  asm volatile("vmsleu.vi v2, v4, 15");
  VSET(2, e8, m2);
  VCMP_U8(18, v2, 0x22, 0x22);
};

void TEST_CASE6(void) {
  VSET(16, e8, m2);
  VLOAD_8(v4, 123, 8, 25, 199, 123, 8, 25, 199, 123, 8, 25, 199, 123, 8, 25,
          199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vi v2, v4, 15, v0.t");
  VSET(2, e8, m2);
  VCMP_U8(19, v2, 0x00, 0x00);

  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vi v2, v4, 15, v0.t");
  VSET(2, e8, m2);
  VCMP_U8(20, v2, 0x00, 0x00);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vi v2, v4, 15, v0.t");
  VSET(2, e8, m2);
  VCMP_U8(21, v2, 0x00, 0x00);

  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345, 8, 25, 199, 12345,
           8, 25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VCLEAR(v2);
  asm volatile("vmsleu.vi v2, v4, 15, v0.t");
  VSET(2, e8, m2);
  VCMP_U8(22, v2, 0x00, 0x00);
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
