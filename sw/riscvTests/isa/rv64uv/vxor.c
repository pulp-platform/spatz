// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1() {
  VSET(12, e8, m8);
  VLOAD_8(v16, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01,
          0xf0);
  VLOAD_8(v24, 0xf0, 0x03, 0xf0, 0xf0, 0x03, 0xf0, 0xf0, 0x03, 0xf0, 0xf0, 0x03,
          0xf0);
  asm volatile("vxor.vv v8, v16, v24");
  VCMP_U8(1, v8, 0x0f, 0x02, 0x00, 0x0f, 0x02, 0x00, 0x0f, 0x02, 0x00, 0x0f,
          0x02, 0x00);

  VSET(12, e16, m8);
  VLOAD_16(v16, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001,
           0xf0f0, 0xffff, 0x0001, 0xf0f0);
  VLOAD_16(v24, 0xff00, 0x0003, 0xf0f0, 0xff00, 0x0003, 0xf0f0, 0xff00, 0x0003,
           0xf0f0, 0xff00, 0x0003, 0xf0f0);
  asm volatile("vxor.vv v8, v16, v24");
  VCMP_U16(2, v8, 0x00ff, 0x0002, 0x0000, 0x00ff, 0x0002, 0x0000, 0x00ff,
           0x0002, 0x0000, 0x00ff, 0x0002, 0x0000);

  VSET(12, e32, m8);
  VLOAD_32(v16, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff, 0x00000001,
           0xf0f0f0f0, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff,
           0x00000001, 0xf0f0f0f0);
  VLOAD_32(v24, 0xffff0000, 0x00000003, 0xf0f0f0f0, 0xffff0000, 0x00000003,
           0xf0f0f0f0, 0xffff0000, 0x00000003, 0xf0f0f0f0, 0xffff0000,
           0x00000003, 0xf0f0f0f0);
  asm volatile("vxor.vv v8, v16, v24");
  VCMP_U32(3, v8, 0x0000ffff, 0x00000002, 0x00000000, 0x0000ffff, 0x00000002,
           0x00000000, 0x0000ffff, 0x00000002, 0x00000000, 0x0000ffff,
           0x00000002, 0x00000000);

#if ELEN == 64
  VSET(12, e64, m8);
  VLOAD_64(v16, 0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0);
  VLOAD_64(v24, 0xffffffff00000000, 0x0000000000000003, 0xf0f0f0f0f0f0f0f0,
           0xffffffff00000000, 0x0000000000000003, 0xf0f0f0f0f0f0f0f0,
           0xffffffff00000000, 0x0000000000000003, 0xf0f0f0f0f0f0f0f0,
           0xffffffff00000000, 0x0000000000000003, 0xf0f0f0f0f0f0f0f0);
  asm volatile("vxor.vv v8, v16, v24");
  VCMP_U64(4, v8, 0x00000000ffffffff, 0x0000000000000002, 0x0000000000000000,
           0x00000000ffffffff, 0x0000000000000002, 0x0000000000000000,
           0x00000000ffffffff, 0x0000000000000002, 0x0000000000000000,
           0x00000000ffffffff, 0x0000000000000002, 0x0000000000000000);
#endif
}

void TEST_CASE2() {
  VSET(12, e8, m8);
  VLOAD_8(v16, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01,
          0xf0);
  VLOAD_8(v24, 0xf0, 0x03, 0xf0, 0xf0, 0x03, 0xf0, 0xf0, 0x03, 0xf0, 0xf0, 0x03,
          0xf0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_8(v8, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef,
          0xef);
  asm volatile("vxor.vv v8, v16, v24, v0.t");
  VCMP_U8(5, v8, 0x0f, 0xef, 0x00, 0x0f, 0xef, 0x00, 0x0f, 0xef, 0x00, 0x0f,
          0xef, 0x00);

  VSET(12, e16, m8);
  VLOAD_16(v16, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001,
           0xf0f0, 0xffff, 0x0001, 0xf0f0);
  VLOAD_16(v24, 0xff00, 0x0003, 0xf0f0, 0xff00, 0x0003, 0xf0f0, 0xff00, 0x0003,
           0xf0f0, 0xff00, 0x0003, 0xf0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_16(v8, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef, 0xbeef, 0xbeef, 0xbeef);
  asm volatile("vxor.vv v8, v16, v24, v0.t");
  VCMP_U16(6, v8, 0x00ff, 0xbeef, 0x0000, 0x00ff, 0xbeef, 0x0000, 0x00ff,
           0xbeef, 0x0000, 0x00ff, 0xbeef, 0x0000);

  VSET(12, e32, m8);
  VLOAD_32(v16, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff, 0x00000001,
           0xf0f0f0f0, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff,
           0x00000001, 0xf0f0f0f0);
  VLOAD_32(v24, 0xffff0000, 0x00000003, 0xf0f0f0f0, 0xffff0000, 0x00000003,
           0xf0f0f0f0, 0xffff0000, 0x00000003, 0xf0f0f0f0, 0xffff0000,
           0x00000003, 0xf0f0f0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_32(v8, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef);
  asm volatile("vxor.vv v8, v16, v24, v0.t");
  VCMP_U32(7, v8, 0x0000ffff, 0xdeadbeef, 0x00000000, 0x0000ffff, 0xdeadbeef,
           0x00000000, 0x0000ffff, 0xdeadbeef, 0x00000000, 0x0000ffff,
           0xdeadbeef, 0x00000000);

#if ELEN == 64
  VSET(12, e64, m8);
  VLOAD_64(v16, 0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0);
  VLOAD_64(v24, 0xffffffff00000000, 0x0000000000000003, 0xf0f0f0f0f0f0f0f0,
           0xffffffff00000000, 0x0000000000000003, 0xf0f0f0f0f0f0f0f0,
           0xffffffff00000000, 0x0000000000000003, 0xf0f0f0f0f0f0f0f0,
           0xffffffff00000000, 0x0000000000000003, 0xf0f0f0f0f0f0f0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_64(v8, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef);
  asm volatile("vxor.vv v8, v16, v24, v0.t");
  VCMP_U64(8, v8, 0x00000000ffffffff, 0xdeadbeefdeadbeef, 0x0000000000000000,
           0x00000000ffffffff, 0xdeadbeefdeadbeef, 0x0000000000000000,
           0x00000000ffffffff, 0xdeadbeefdeadbeef, 0x0000000000000000,
           0x00000000ffffffff, 0xdeadbeefdeadbeef, 0x0000000000000000);
#endif
}

void TEST_CASE3() {
  const uint32_t scalar = 0x0ff00ff0;

  VSET(12, e8, m8);
  VLOAD_8(v16, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01,
          0xf0);
  asm volatile("vxor.vx v8, v16, %[A]" ::[A] "r"(scalar));
  VCMP_U8(9, v8, 0x0f, 0xf1, 0x00, 0x0f, 0xf1, 0x00, 0x0f, 0xf1, 0x00, 0x0f,
          0xf1, 0x00);

  VSET(12, e16, m8);
  VLOAD_16(v16, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001,
           0xf0f0, 0xffff, 0x0001, 0xf0f0);
  asm volatile("vxor.vx v8, v16, %[A]" ::[A] "r"(scalar));
  VCMP_U16(10, v8, 0xf00f, 0x0ff1, 0xff00, 0xf00f, 0x0ff1, 0xff00, 0xf00f,
           0x0ff1, 0xff00, 0xf00f, 0x0ff1, 0xff00);

  VSET(12, e32, m8);
  VLOAD_32(v16, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff, 0x00000001,
           0xf0f0f0f0, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff,
           0x00000001, 0xf0f0f0f0);
  asm volatile("vxor.vx v8, v16, %[A]" ::[A] "r"(scalar));
  VCMP_U32(11, v8, 0xf00ff00f, 0x0ff00ff1, 0xff00ff00, 0xf00ff00f, 0x0ff00ff1,
           0xff00ff00, 0xf00ff00f, 0x0ff00ff1, 0xff00ff00, 0xf00ff00f,
           0x0ff00ff1, 0xff00ff00);

#if ELEN == 64
  VSET(12, e64, m8);
  VLOAD_64(v16, 0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0);
  asm volatile("vxor.vx v8, v16, %[A]" ::[A] "r"(scalar));
  VCMP_U64(12, v8, 0xfffffffff00ff00f, 0x000000000ff00ff1, 0xf0f0f0f0ff00ff00,
           0xfffffffff00ff00f, 0x000000000ff00ff1, 0xf0f0f0f0ff00ff00,
           0xfffffffff00ff00f, 0x000000000ff00ff1, 0xf0f0f0f0ff00ff00,
           0xfffffffff00ff00f, 0x000000000ff00ff1, 0xf0f0f0f0ff00ff00);
#endif
}

void TEST_CASE4() {
  const uint32_t scalar = 0x0ff00ff0;

  VSET(12, e8, m8);
  VLOAD_8(v16, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01,
          0xf0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_8(v8, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef,
          0xef);
  asm volatile("vxor.vx v8, v16, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U8(13, v8, 0x0f, 0xef, 0x00, 0x0f, 0xef, 0x00, 0x0f, 0xef, 0x00, 0x0f,
          0xef, 0x00);

  VSET(12, e16, m8);
  VLOAD_16(v16, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001,
           0xf0f0, 0xffff, 0x0001, 0xf0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_16(v8, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef, 0xbeef, 0xbeef, 0xbeef);
  asm volatile("vxor.vx v8, v16, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U16(14, v8, 0xf00f, 0xbeef, 0xff00, 0xf00f, 0xbeef, 0xff00, 0xf00f,
           0xbeef, 0xff00, 0xf00f, 0xbeef, 0xff00);

  VSET(12, e32, m8);
  VLOAD_32(v16, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff, 0x00000001,
           0xf0f0f0f0, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff,
           0x00000001, 0xf0f0f0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_32(v8, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef);
  asm volatile("vxor.vx v8, v16, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U32(15, v8, 0xf00ff00f, 0xdeadbeef, 0xff00ff00, 0xf00ff00f, 0xdeadbeef,
           0xff00ff00, 0xf00ff00f, 0xdeadbeef, 0xff00ff00, 0xf00ff00f,
           0xdeadbeef, 0xff00ff00);

#if ELEN == 64
  VSET(12, e64, m8);
  VLOAD_64(v16, 0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_64(v8, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef);
  asm volatile("vxor.vx v8, v16, %[A], v0.t" ::[A] "r"(scalar));
  VCMP_U64(16, v8, 0xfffffffff00ff00f, 0xdeadbeefdeadbeef, 0xf0f0f0f0ff00ff00,
           0xfffffffff00ff00f, 0xdeadbeefdeadbeef, 0xf0f0f0f0ff00ff00,
           0xfffffffff00ff00f, 0xdeadbeefdeadbeef, 0xf0f0f0f0ff00ff00,
           0xfffffffff00ff00f, 0xdeadbeefdeadbeef, 0xf0f0f0f0ff00ff00);
#endif
}

void TEST_CASE5() {
  VSET(12, e8, m8);
  VLOAD_8(v16, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01,
          0xf0);
  asm volatile("vxor.vi v8, v16, 15");
  VCMP_U8(17, v8, 0xf0, 0x0e, 0xff, 0xf0, 0x0e, 0xff, 0xf0, 0x0e, 0xff, 0xf0,
          0x0e, 0xff);

  VSET(12, e16, m8);
  VLOAD_16(v16, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001,
           0xf0f0, 0xffff, 0x0001, 0xf0f0);
  asm volatile("vxor.vi v8, v16, 15");
  VCMP_U16(18, v8, 0xfff0, 0x000e, 0xf0ff, 0xfff0, 0x000e, 0xf0ff, 0xfff0,
           0x000e, 0xf0ff, 0xfff0, 0x000e, 0xf0ff);

  VSET(12, e32, m8);
  VLOAD_32(v16, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff, 0x00000001,
           0xf0f0f0f0, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff,
           0x00000001, 0xf0f0f0f0);
  asm volatile("vxor.vi v8, v16, 15");
  VCMP_U32(19, v8, 0xfffffff0, 0x0000000e, 0xf0f0f0ff, 0xfffffff0, 0x0000000e,
           0xf0f0f0ff, 0xfffffff0, 0x0000000e, 0xf0f0f0ff, 0xfffffff0,
           0x0000000e, 0xf0f0f0ff);

#if ELEN == 64
  VSET(12, e64, m8);
  VLOAD_64(v16, 0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0);
  asm volatile("vxor.vi v8, v16, 15");
  VCMP_U64(20, v8, 0xfffffffffffffff0, 0x000000000000000e, 0xf0f0f0f0f0f0f0ff,
           0xfffffffffffffff0, 0x000000000000000e, 0xf0f0f0f0f0f0f0ff,
           0xfffffffffffffff0, 0x000000000000000e, 0xf0f0f0f0f0f0f0ff,
           0xfffffffffffffff0, 0x000000000000000e, 0xf0f0f0f0f0f0f0ff);
#endif
}

void TEST_CASE6() {
  VSET(12, e8, m8);
  VLOAD_8(v16, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01, 0xf0, 0xff, 0x01,
          0xf0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_8(v8, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef,
          0xef);
  asm volatile("vxor.vi v8, v16, 15, v0.t");
  VCMP_U8(21, v8, 0xf0, 0xef, 0xff, 0xf0, 0xef, 0xff, 0xf0, 0xef, 0xff, 0xf0,
          0xef, 0xff);

  VSET(12, e16, m8);
  VLOAD_16(v16, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001, 0xf0f0, 0xffff, 0x0001,
           0xf0f0, 0xffff, 0x0001, 0xf0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_16(v8, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef, 0xbeef,
           0xbeef, 0xbeef, 0xbeef, 0xbeef);
  asm volatile("vxor.vi v8, v16, 15, v0.t");
  VCMP_U16(22, v8, 0xfff0, 0xbeef, 0xf0ff, 0xfff0, 0xbeef, 0xf0ff, 0xfff0,
           0xbeef, 0xf0ff, 0xfff0, 0xbeef, 0xf0ff);

  VSET(12, e32, m8);
  VLOAD_32(v16, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff, 0x00000001,
           0xf0f0f0f0, 0xffffffff, 0x00000001, 0xf0f0f0f0, 0xffffffff,
           0x00000001, 0xf0f0f0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_32(v8, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
           0xdeadbeef, 0xdeadbeef);
  asm volatile("vxor.vi v8, v16, 15, v0.t");
  VCMP_U32(23, v8, 0xfffffff0, 0xdeadbeef, 0xf0f0f0ff, 0xfffffff0, 0xdeadbeef,
           0xf0f0f0ff, 0xfffffff0, 0xdeadbeef, 0xf0f0f0ff, 0xfffffff0,
           0xdeadbeef, 0xf0f0f0ff);

#if ELEN == 64
  VSET(12, e64, m8);
  VLOAD_64(v16, 0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0,
           0xffffffffffffffff, 0x0000000000000001, 0xf0f0f0f0f0f0f0f0);
  VLOAD_8(v0, 0x6D, 0x0B);
  VLOAD_64(v8, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef,
           0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef, 0xdeadbeefdeadbeef);
  asm volatile("vxor.vi v8, v16, 15, v0.t");
  VCMP_U64(40, v8, 0xfffffffffffffff0, 0xdeadbeefdeadbeef, 0xf0f0f0f0f0f0f0ff,
           0xfffffffffffffff0, 0xdeadbeefdeadbeef, 0xf0f0f0f0f0f0f0ff,
           0xfffffffffffffff0, 0xdeadbeefdeadbeef, 0xf0f0f0f0f0f0f0ff,
           0xfffffffffffffff0, 0xdeadbeefdeadbeef, 0xf0f0f0f0f0f0f0ff);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  // TEST_CASE2();
  TEST_CASE3();
  // TEST_CASE4();
  TEST_CASE5();
  // TEST_CASE6();

  EXIT_CHECK();
}
