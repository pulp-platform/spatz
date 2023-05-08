// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_16(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  asm volatile("vmin.vv v2, v4, v6");
  VCMP_I16(1, v2, 50, -80, 400, -19900, 50, -80, 400, -19900, 50, -80, 400,
           -19900, 50, -80, 400, -19900);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_32(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  asm volatile("vmin.vv v2, v4, v6");
  VCMP_I32(2, v2, 50, -80, 400, -19900, 50, -80, 400, -19900, 50, -80, 400,
           -19900, 50, -80, 400, -19900);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_64(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  asm volatile("vmin.vv v2, v4, v6");
  VCMP_I64(3, v2, 50, -80, 400, -19900, 50, -80, 400, -19900, 50, -80, 400,
           -19900, 50, -80, 400, -19900);
#endif
};

void TEST_CASE2(void) {
  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_16(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VLOAD_8(v0, 0xCC, 0xCC);
  VLOAD_16(v2, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef);
  asm volatile("vmin.vv v2, v4, v6, v0.t");
  VCMP_I16(4, v2, 0xbeef, 0xbeef, 400, -19900, 0xbeef, 0xbeef, 400, -19900,
           0xbeef, 0xbeef, 400, -19900, 0xbeef, 0xbeef, 400, -19900);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_32(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VLOAD_8(v0, 0xCC, 0xCC);
  VLOAD_32(v2, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef);
  asm volatile("vmin.vv v2, v4, v6, v0.t");
  VCMP_I32(5, v2, 0xdeadbeef, 0xdeadbeef, 400, -19900, 0xdeadbeef, 0xdeadbeef,
           400, -19900, 0xdeadbeef, 0xdeadbeef, 400, -19900, 0xdeadbeef,
           0xdeadbeef, 400, -19900);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, -80, 2560, -19900, 12345, -80, 2560, -19900, 12345, -80,
           2560, -19900, 12345, -80, 2560, -19900);
  VLOAD_64(v6, 50, 7000, 400, 19901, 50, 7000, 400, 19901, 50, 7000, 400, 19901,
           50, 7000, 400, 19901);
  VLOAD_8(v0, 0xCC, 0xCC);
  VLOAD_64(v2, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef);
  asm volatile("vmin.vv v2, v4, v6, v0.t");
  VCMP_I64(6, v2, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 400, -19900,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 400, -19900,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 400, -19900,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 400, -19900);
#endif
};

void TEST_CASE3(void) {
  const uint64_t scalar = 40;

  VSET(16, e8, m2);
  VLOAD_8(v4, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  asm volatile("vmin.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VCMP_I8(7, v2, 40, -8, -25, 40, 40, -8, -25, 40, 40, -8, -25, 40, 40, -8, -25,
          40);

  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  asm volatile("vmin.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VCMP_I16(8, v2, 40, -8, -25, 40, 40, -8, -25, 40, 40, -8, -25, 40, 40, -8,
           -25, 40);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  asm volatile("vmin.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VCMP_I32(9, v2, 40, -8, -25, 40, 40, -8, -25, 40, 40, -8, -25, 40, 40, -8,
           -25, 40);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  asm volatile("vmin.vx v2, v4, %[A]" ::[A] "r"(scalar));
  VCMP_I64(10, v2, 40, -8, -25, 40, 40, -8, -25, 40, 40, -8, -25, 40, 40, -8,
           -25, 40);
#endif
};

void TEST_CASE4(void) {
  const uint64_t scalar = 40;

  VSET(16, e8, m2);
  VLOAD_8(v4, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8, -25, 99, 123, -8,
          -25, 99);
  VLOAD_8(v0, 0xCC, 0xCC);
  VLOAD_8(v2, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef,
          0xef, 0xef, 0xef, 0xef, 0xef);
  asm volatile("vmin.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_I8(11, v2, 0xef, 0xef, -25, 40, 0xef, 0xef, -25, 40, 0xef, 0xef, -25, 40,
          0xef, 0xef, -25, 40);

  VSET(16, e16, m2);
  VLOAD_16(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VLOAD_16(v2, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef);
  asm volatile("vmin.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_I16(12, v2, 0xbeef, 0xbeef, -25, 40, 0xbeef, 0xbeef, -25, 40, 0xbeef,
           0xbeef, -25, 40, 0xbeef, 0xbeef, -25, 40);

  VSET(16, e32, m2);
  VLOAD_32(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VLOAD_32(v2, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef);
  asm volatile("vmin.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_I32(13, v2, 0xdeadbeef, 0xdeadbeef, -25, 40, 0xdeadbeef, 0xdeadbeef, -25,
           40, 0xdeadbeef, 0xdeadbeef, -25, 40, 0xdeadbeef, 0xdeadbeef, -25,
           40);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, 12345, -8, -25, 199, 12345, -8, -25, 199, 12345, -8, -25, 199,
           12345, -8, -25, 199);
  VLOAD_8(v0, 0xCC, 0xCC);
  VLOAD_64(v2, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef);
  asm volatile("vmin.vx v2, v4, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_I64(14, v2, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, -25, 40,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, -25, 40, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, -25, 40, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           -25, 40);
#endif
};

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  // TEST_CASE2();
  TEST_CASE3();
  // TEST_CASE4();

  EXIT_CHECK();
}
