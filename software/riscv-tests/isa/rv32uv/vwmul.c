// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0xee, 0xfe, 0xbd, 0xc2, 0x02, 0xa4, 0x34, 0x33, 0x2b, 0x35, 0x16,
          0x9b, 0x3b, 0x5f, 0xfc, 0x8b);
  VLOAD_8(v4, 0xcb, 0x24, 0xe8, 0xb2, 0xeb, 0x24, 0x80, 0x67, 0x43, 0x11, 0x7c,
          0x94, 0x22, 0x71, 0xca, 0x80);
  asm volatile("vwmul.vv v6, v2, v4");
  VSET(16, e16, m1);
  VCMP_I16(1, v6, 0x03ba, 0xffb8, 0x0648, 0x12e4, 0xffd6, 0xf310, 0xe600,
           0x1485, 0x0b41, 0x0385, 0x0aa8, 0x2a9c, 0x07d6, 0x29ef, 0x00d8,
           0x3a80);

  VSET(16, e16, m1);
  VLOAD_16(v2, 0x8aed, 0x2153, 0x5377, 0xc19c, 0x1051, 0x1b75, 0xbafd, 0xb200,
           0xb209, 0xa9a2, 0xbdc4, 0x1653, 0x5965, 0x145e, 0xb626, 0xd79c);
  VLOAD_16(v4, 0x778d, 0xc104, 0x6eac, 0x78e8, 0xacd2, 0x698b, 0xc7d3, 0x1e29,
           0x0a58, 0x58b5, 0x29f9, 0x2fb0, 0x2166, 0x0ac4, 0x44e5, 0xbc40);
  asm volatile("vwmul.vv v6, v2, v4");
  VSET(16, e32, m1);
  VCMP_I32(2, v6, 0xc953af89, 0xf7cd184c, 0x241535f4, 0xe2889560, 0xfab2ce72,
           0x0b51e587, 0x0f24c987, 0xf6cf8200, 0xfcd98d18, 0xe2129f8a,
           0xf523f7a4, 0x04289610, 0x0ba9a33e, 0x00db43f8, 0xec2007fe,
           0x0ab07700);
};

void TEST_CASE2(void) {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0xee, 0xfe, 0xbd, 0xc2, 0x02, 0xa4, 0x34, 0x33, 0x2b, 0x35, 0x16,
          0x9b, 0x3b, 0x5f, 0xfc, 0x8b);
  VLOAD_8(v4, 0xcb, 0x24, 0xe8, 0xb2, 0xeb, 0x24, 0x80, 0x67, 0x43, 0x11, 0x7c,
          0x94, 0x22, 0x71, 0xca, 0x80);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwmul.vv v6, v2, v4, v0.t");
  VSET(16, e16, m1);
  VCMP_I16(4, v6, 0, 0xffb8, 0, 0x12e4, 0, 0xf310, 0, 0x1485, 0, 0x0385, 0,
           0x2a9c, 0, 0x29ef, 0, 0x3a80);

  VSET(16, e16, m1);
  VLOAD_16(v2, 0x8aed, 0x2153, 0x5377, 0xc19c, 0x1051, 0x1b75, 0xbafd, 0xb200,
           0xb209, 0xa9a2, 0xbdc4, 0x1653, 0x5965, 0x145e, 0xb626, 0xd79c);
  VLOAD_16(v4, 0x778d, 0xc104, 0x6eac, 0x78e8, 0xacd2, 0x698b, 0xc7d3, 0x1e29,
           0x0a58, 0x58b5, 0x29f9, 0x2fb0, 0x2166, 0x0ac4, 0x44e5, 0xbc40);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwmul.vv v6, v2, v4, v0.t");
  VSET(16, e32, m1);
  VCMP_I32(5, v6, 0, 0xf7cd184c, 0, 0xe2889560, 0, 0x0b51e587, 0, 0xf6cf8200, 0,
           0xe2129f8a, 0, 0x04289610, 0, 0x00db43f8, 0, 0x0ab07700);
};

void TEST_CASE3(void) {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0x86, 0x79, 0xa0, 0x8a, 0x3e, 0xc3, 0x3e, 0x0c, 0x1b, 0xca, 0x80,
          0x41, 0x0e, 0xee, 0x94, 0xdf);
  int64_t scalar = 5;
  asm volatile("vwmul.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VSET(16, e16, m1);
  VCMP_I16(7, v6, 0xfd9e, 0x025d, 0xfe20, 0xfdb2, 0x0136, 0xfecf, 0x0136,
           0x003c, 0x0087, 0xfef2, 0xfd80, 0x0145, 0x0046, 0xffa6, 0xfde4,
           0xff5b);

  VSET(16, e16, m1);
  VLOAD_16(v2, 0xb0ab, 0xcccb, 0x5fad, 0x9e24, 0x1496, 0xd4a0, 0x2552, 0xcef6,
           0x34b8, 0xef22, 0x69c3, 0xbb05, 0xbe72, 0x315b, 0x3f03, 0xf58b);
  scalar = -5383;
  asm volatile("vwmul.vx v6, v2, %[A]" ::[A] "r"(scalar));
  VSET(16, e32, m1);
  VCMP_I32(8, v6, 0x06842453, 0x0434bf73, 0xf8243145, 0x0809b904, 0xfe4f21e6,
           0x03900fa0, 0xfcef40c2, 0x04072946, 0xfbab76f8, 0x0162ac12,
           0xf7501cab, 0x05aa79dd, 0x056270e2, 0xfbf22f83, 0xfad307eb,
           0x00dbe233);
};

void TEST_CASE4(void) {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0x86, 0x79, 0xa0, 0x8a, 0x3e, 0xc3, 0x3e, 0x0c, 0x1b, 0xca, 0x80,
          0x41, 0x0e, 0xee, 0x94, 0xdf);
  int64_t scalar = 5;
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwmul.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VSET(16, e16, m1);
  VCMP_I16(10, v6, 0, 0x025d, 0, 0xfdb2, 0, 0xfecf, 0, 0x003c, 0, 0xfef2, 0,
           0x0145, 0, 0xffa6, 0, 0xff5b);

  VSET(16, e16, m1);
  VLOAD_16(v2, 0xb0ab, 0xcccb, 0x5fad, 0x9e24, 0x1496, 0xd4a0, 0x2552, 0xcef6,
           0x34b8, 0xef22, 0x69c3, 0xbb05, 0xbe72, 0x315b, 0x3f03, 0xf58b);
  scalar = -5383;
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v6);
  asm volatile("vwmul.vx v6, v2, %[A], v0.t" ::[A] "r"(scalar));
  VSET(16, e32, m1);
  VCMP_I32(11, v6, 0, 0x0434bf73, 0, 0x0809b904, 0, 0x03900fa0, 0, 0x04072946,
           0, 0x0162ac12, 0, 0x05aa79dd, 0, 0xfbf22f83, 0, 0x00dbe233);
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