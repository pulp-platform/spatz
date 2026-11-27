// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Generated test for vadc.vim (Vector Add with Carry, immediate format)

#include "vector_macros.h"

// vadc.vim with carry-in = 0 for all elements
void TEST_CASE1(void) {
  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0x00, 0x00); // Carry-in is 0
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U8(1, v24, 6, 7, 8, 9, 10, 11, 12, 13, 6, 7, 8, 9, 10, 11, 12, 13);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0x00, 0x00);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U16(2, v24, 6, 7, 8, 9, 10, 11, 12, 13, 6, 7, 8, 9, 10, 11, 12, 13);

  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0x00, 0x00);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U32(3, v24, 6, 7, 8, 9, 10, 11, 12, 13, 6, 7, 8, 9, 10, 11, 12, 13);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0x00, 0x00);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U64(4, v24, 6, 7, 8, 9, 10, 11, 12, 13, 6, 7, 8, 9, 10, 11, 12, 13);
#endif
}

// vadc.vim with carry-in = 1 for all elements
void TEST_CASE2(void) {
  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xFF, 0xFF); // Carry-in is 1
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U8(5, v24, 7, 8, 9, 10, 11, 12, 13, 14, 7, 8, 9, 10, 11, 12, 13, 14);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xFF, 0xFF);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U16(6, v24, 7, 8, 9, 10, 11, 12, 13, 14, 7, 8, 9, 10, 11, 12, 13, 14);

  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xFF, 0xFF);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U32(7, v24, 7, 8, 9, 10, 11, 12, 13, 14, 7, 8, 9, 10, 11, 12, 13, 14);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xFF, 0xFF);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U64(8, v24, 7, 8, 9, 10, 11, 12, 13, 14, 7, 8, 9, 10, 11, 12, 13, 14);
#endif
}

// This will add (imm=5) + 0 to even indices, and (imm=5) + 1 to odd indices.
void TEST_CASE3(void) {
  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  // Index: 0 (+5), 1 (+6), 2 (+5), 3 (+6)...
  VCMP_U8(9, v24, 6, 8, 8, 10, 10, 12, 12, 14, 6, 8, 8, 10, 10, 12, 12, 14);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U16(10, v24, 6, 8, 8, 10, 10, 12, 12, 14, 6, 8, 8, 10, 10, 12, 12, 14);

  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U32(11, v24, 6, 8, 8, 10, 10, 12, 12, 14, 6, 8, 8, 10, 10, 12, 12, 14);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  VCMP_U64(12, v24, 6, 8, 8, 10, 10, 12, 12, 14, 6, 8, 8, 10, 10, 12, 12, 14);
#endif
}

// Testing immediate values around boundaries, where carry makes it overflow
void TEST_CASE4(void) {
  VSET(16, e8, m8);
  // -2 (0xFE), -1 (0xFF), 0, etc.
  VLOAD_8(v8, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250);
  VLOAD_8(v0, 0xAA, 0xAA); // 10101010
  VCLEAR(v24);
  asm volatile("vadc.vim v24, v8, 5, v0");
  // Even elements: 250 + 5 + 0 = 255
  // Odd elements: 250 + 5 + 1 = 256 (rolls over to 0 for 8-bit)
  VCMP_U8(13, v24, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0);

  // Note: Only included e8 here to keep it concise, but the pattern mimics the other cases.
}

void TEST_CASE5(void) {
  // 1. Carry-in = 0 (v0 = 0x00)
  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v16, 10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50, 60, 70, 80);
  VLOAD_8(v0, 0x00, 0x00);
  VCLEAR(v24);
  asm volatile("vadc.vvm v24, v8, v16, v0");
  VCMP_U8(14, v24, 11, 22, 33, 44, 55, 66, 77, 88, 11, 22, 33, 44, 55, 66, 77, 88);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v16, 10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50, 60, 70, 80);
  VLOAD_8(v0, 0x00, 0x00);
  VCLEAR(v24);
  asm volatile("vadc.vvm v24, v8, v16, v0");
  VCMP_U16(15, v24, 11, 22, 33, 44, 55, 66, 77, 88, 11, 22, 33, 44, 55, 66, 77, 88);

  // 2. Carry-in = 1 (v0 = 0xFF)
  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v16, 10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50, 60, 70, 80);
  VLOAD_8(v0, 0xFF, 0xFF);
  VCLEAR(v24);
  asm volatile("vadc.vvm v24, v8, v16, v0");
  VCMP_U32(16, v24, 12, 23, 34, 45, 56, 67, 78, 89, 12, 23, 34, 45, 56, 67, 78, 89);

#if ELEN == 64
  // 3. Alternating Carry-in (v0 = 0xAA -> Even gets +0, Odd gets +1)
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v16, 10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50, 60, 70, 80);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vadc.vvm v24, v8, v16, v0");
  VCMP_U64(17, v24, 11, 23, 33, 45, 55, 67, 77, 89, 11, 23, 33, 45, 55, 67, 77, 89);
#endif
}

void TEST_CASE6(void) {
  const uint32_t scalar = 5;

  // 1. Carry-in = 0 (v0 = 0x00)
  VSET(16, e8, m8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0x00, 0x00);
  VCLEAR(v24);
  asm volatile("vadc.vxm v24, v8, %[A], v0" ::[A] "r"(scalar));
  VCMP_U8(18, v24, 6, 7, 8, 9, 10, 11, 12, 13, 6, 7, 8, 9, 10, 11, 12, 13);

  VSET(16, e16, m8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0x00, 0x00);
  VCLEAR(v24);
  asm volatile("vadc.vxm v24, v8, %[A], v0" ::[A] "r"(scalar));
  VCMP_U16(19, v24, 6, 7, 8, 9, 10, 11, 12, 13, 6, 7, 8, 9, 10, 11, 12, 13);

  // 2. Carry-in = 1 (v0 = 0xFF)
  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xFF, 0xFF);
  VCLEAR(v24);
  asm volatile("vadc.vxm v24, v8, %[A], v0" ::[A] "r"(scalar));
  VCMP_U32(20, v24, 7, 8, 9, 10, 11, 12, 13, 14, 7, 8, 9, 10, 11, 12, 13, 14);

#if ELEN == 64
  // 3. Alternating Carry-in (v0 = 0xAA -> Even gets +0, Odd gets +1)
  VSET(16, e64, m8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v24);
  asm volatile("vadc.vxm v24, v8, %[A], v0" ::[A] "r"(scalar));
  VCMP_U64(21, v24, 6, 8, 8, 10, 10, 12, 12, 14, 6, 8, 8, 10, 10, 12, 12, 14);
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
  TEST_CASE6();

  EXIT_CHECK();
}