// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: Riccardo Giunti - Fondazione CHIPS-IT

#include "vector_macros.h"

// Segment-2 for 8-bit, vl = 4 --> 8 bytes
void TEST_CASE2_8_vl4(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e8.v v1, (%0)" ::"r"(INP1));
  VCMP_U8(1, v1, 0x9f, 0x19, 0x8f, 0x05);
  VCMP_U8(2, v2, 0xe4, 0x20, 0x2e, 0xe0);
}

// Segment-8 for 8-bit, vl = 2 --> 6 bytes
void TEST_CASE3_8_vl2(void) {
  VSET(2, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e8.v v1, (%0)" ::"r"(INP1));
  VCMP_U8(3, v1, 0x9f, 0x20);
  VCMP_U8(4, v2, 0xe4, 0x8f);
  VCMP_U8(5, v3, 0x19, 0x2e);
}

// Segment-3 for 8-bit, vl = 4 --> 12 bytes
void TEST_CASE3_8_vl4(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e8.v v1, (%0)" ::"r"(INP1));
  VCMP_U8(6, v1, 0x9f, 0x20, 0x05, 0xaa);
  VCMP_U8(7, v2, 0xe4, 0x8f, 0xe0, 0x71);
  VCMP_U8(8, v3, 0x19, 0x2e, 0xf9, 0xf0);
}

// Segment-4 for 8-bit, vl = 4 --> 16 bytes
void TEST_CASE4_8_vl4(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e8.v v1, (%0)" ::"r"(INP1));
  VCMP_U8(9, v1, 0x9f, 0x8f, 0xf9, 0xc3);
  VCMP_U8(10, v2, 0xe4, 0x2e, 0xaa, 0x94);
  VCMP_U8(11, v3, 0x19, 0x05, 0x71, 0xbb);
  VCMP_U8(12, v4, 0x20, 0xe0, 0xf0, 0xd3);
}

// Segment-8 for 8-bit, vl = 2 --> 16 bytes
void TEST_CASE8_8_vl2(void) {
  VSET(2, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  VCLEAR(v5);
  VCLEAR(v6);
  VCLEAR(v7);
  VCLEAR(v8);
  asm volatile("vlseg8e8.v v1, (%0)" ::"r"(INP1));
  VCMP_U8(13, v1, 0x9f, 0xf9);
  VCMP_U8(14, v2, 0xe4, 0xaa);
  VCMP_U8(15, v3, 0x19, 0x71);
  VCMP_U8(16, v4, 0x20, 0xf0);
  VCMP_U8(17, v5, 0x8f, 0xc3);
  VCMP_U8(18, v6, 0x2e, 0x94);
  VCMP_U8(19, v7, 0x05, 0xbb);
  VCMP_U8(20, v8, 0xe0, 0xd3);
}

// Segment-2 for 16-bit, vl = 4 --> 16 bytes
void TEST_CASE2_16_vl4(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e16.v v1, (%0)" ::"r"(INP1));
  VCMP_U16(21, v1, 0x9fe4, 0x8f2e, 0xf9aa, 0xc394);
  VCMP_U16(22, v2, 0x1920, 0x05e0, 0x71f0, 0xbbd3);
}

// Segment-3 for 16-bit, vl = 4 --> 24 bytes
void TEST_CASE3_16_vl4(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e16.v v1, (%0)" ::"r"(INP1));
  VCMP_U16(23, v1, 0x9fe4, 0x05e0, 0xc394, 0x5678);
  VCMP_U16(24, v2, 0x1920, 0xf9aa, 0xbbd3, 0x9abc);
  VCMP_U16(25, v3, 0x8f2e, 0x71f0, 0x1234, 0xdef0);
}

// Segment-4 for 16-bit, vl = 4 --> 32 bytes
void TEST_CASE4_16_vl4(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e16.v v1, (%0)" ::"r"(INP1));
  VCMP_U16(26, v1, 0x9fe4, 0xf9aa, 0x1234, 0x1357);
  VCMP_U16(27, v2, 0x1920, 0x71f0, 0x5678, 0x2468);
  VCMP_U16(28, v3, 0x8f2e, 0xc394, 0x9abc, 0x369b);
  VCMP_U16(29, v4, 0x05e0, 0xbbd3, 0xdef0, 0x48ac);
}

// Segment-8 for 16-bit, vl = 2 --> 16 bytes
void TEST_CASE8_16_vl2(void) {
  VSET(2, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  VCLEAR(v5);
  VCLEAR(v6);
  VCLEAR(v7);
  VCLEAR(v8);
  asm volatile("vlseg8e16.v v1, (%0)" ::"r"(INP1));
  VCMP_U16(30, v1, 0x9fe4, 0x1234);
  VCMP_U16(31, v2, 0x1920, 0x5678);
  VCMP_U16(32, v3, 0x8f2e, 0x9abc);
  VCMP_U16(33, v4, 0x05e0, 0xdef0);
  VCMP_U16(34, v5, 0xf9aa, 0x1357);
  VCMP_U16(35, v6, 0x71f0, 0x2468);
  VCMP_U16(36, v7, 0xc394, 0x369b);
  VCMP_U16(37, v8, 0xbbd3, 0x48ac);
}

// Segment-2 for 32-bit, vl = 4 --> 32 bytes
void TEST_CASE2_32_vl4(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e32.v v1, (%0)" ::"r"(INP1));
  VCMP_U32(38, v1, 0x9fe41920, 0xf9aa71f0, 0x12345678, 0x13572468);
  VCMP_U32(39, v2, 0x8f2e05e0, 0xc394bbd3, 0x9abcdef0, 0x369b48ac);
}

// Segment-3 for 32-bit, vl = 4 --> 48 bytes
void TEST_CASE3_32_vl4(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e32.v v1, (%0)" ::"r"(INP1));
  VCMP_U32(40, v1, 0x9fe41920, 0xc394bbd3, 0x13572468, 0xcafebabe);
  VCMP_U32(41, v2, 0x8f2e05e0, 0x12345678, 0x369b48ac, 0x01234567);
  VCMP_U32(42, v3, 0xf9aa71f0, 0x9abcdef0, 0xdeadbeef, 0x89abcdef);
}

// Segment-3 for 32-bit, vl = 2 --> 24 bytes
void TEST_CASE3_32_vl2(void) {
  VSET(2, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e32.v v1, (%0)" ::"r"(INP1));
  VCMP_U32(43, v1, 0x9fe41920, 0xc394bbd3);
  VCMP_U32(44, v2, 0x8f2e05e0, 0x12345678);
  VCMP_U32(45, v3, 0xf9aa71f0, 0x9abcdef0);
}

// Segment-4 for 32-bit, vl = 4 --> 64 bytes
void TEST_CASE4_32_vl4(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e32.v v1, (%0)" ::"r"(INP1));
  VCMP_U32(46, v1, 0x9fe41920, 0x12345678, 0xdeadbeef, 0x55aa55aa);
  VCMP_U32(47, v2, 0x8f2e05e0, 0x9abcdef0, 0xcafebabe, 0x77889900);
  VCMP_U32(48, v3, 0xf9aa71f0, 0x13572468, 0x01234567, 0xabcdef12);
  VCMP_U32(49, v4, 0xc394bbd3, 0x369b48ac, 0x89abcdef, 0x34567890);
}

// Segment-8 for 32-bit, vl = 2 --> 64 bytes
void TEST_CASE8_32_vl2(void) {
  VSET(2, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  VCLEAR(v5);
  VCLEAR(v6);
  VCLEAR(v7);
  VCLEAR(v8);
  asm volatile("vlseg8e32.v v1, (%0)" ::"r"(INP1));
  VCMP_U32(50, v1, 0x9fe41920, 0xdeadbeef);
  VCMP_U32(51, v2, 0x8f2e05e0, 0xcafebabe);
  VCMP_U32(52, v3, 0xf9aa71f0, 0x01234567);
  VCMP_U32(53, v4, 0xc394bbd3, 0x89abcdef);
  VCMP_U32(54, v5, 0x12345678, 0x55aa55aa);
  VCMP_U32(55, v6, 0x9abcdef0, 0x77889900);
  VCMP_U32(56, v7, 0x13572468, 0xabcdef12);
  VCMP_U32(57, v8, 0x369b48ac, 0x34567890);
}

// Segment-2 for 64-bit, vl = 4 --> 64 bytes
void TEST_CASE2_64_vl4(void) {
  VSET(4, e64, m1);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef,
      0x55aa55aa77889900, 0xabcdef1234567890, 0xfeedfacecafebabe,
      0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
      0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e64.v v1, (%0)" ::"r"(INP1));
  VCMP_U64(58, v1, 0x9fe419208f2e05e0, 0x123456789abcdef0, 0xdeadbeefcafebabe, 0x55aa55aa77889900);
  VCMP_U64(59, v2, 0xf9aa71f0c394bbd3, 0x13572468369b48ac, 0x0123456789abcdef, 0xabcdef1234567890);
}

// Segment-3 for 64-bit, vl = 4 --> 96 bytes
void TEST_CASE3_64_vl4(void) {
  VSET(4, e64, m1);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef,
      0x55aa55aa77889900, 0xabcdef1234567890, 0xfeedfacecafebabe,
      0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
      0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e64.v v1, (%0)" ::"r"(INP1));
  VCMP_U64(60, v1, 0x9fe419208f2e05e0, 0x13572468369b48ac, 0x55aa55aa77889900, 0x123456789abcdef0);
  VCMP_U64(61, v2, 0xf9aa71f0c394bbd3, 0xdeadbeefcafebabe, 0xabcdef1234567890, 0x1357246855aa55aa);
  VCMP_U64(62, v3, 0x123456789abcdef0, 0x0123456789abcdef, 0xfeedfacecafebabe, 0x369b48acdeadbeef);
}

// Segment-4 for 64-bit, vl = 4 --> 128 bytes
void TEST_CASE4_64_vl4(void) {
  VSET(4, e64, m1);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef,
      0x55aa55aa77889900, 0xabcdef1234567890, 0xfeedfacecafebabe,
      0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
      0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e64.v v1, (%0)" ::"r"(INP1));
  VCMP_U64(63, v1, 0x9fe419208f2e05e0, 0xdeadbeefcafebabe, 0xfeedfacecafebabe, 0xcafebabe12345678);
  VCMP_U64(64, v2, 0xf9aa71f0c394bbd3, 0x0123456789abcdef, 0x123456789abcdef0, 0xabcdef0987654321);
  VCMP_U64(65, v3, 0x123456789abcdef0, 0x55aa55aa77889900, 0x1357246855aa55aa, 0x012345670abcdef1);
  VCMP_U64(66, v4, 0x13572468369b48ac, 0xabcdef1234567890, 0x369b48acdeadbeef, 0x987654321fedcba0);
}

// Segment-8 for 64-bit, vl = 2 --> 128 bytes
void TEST_CASE8_64_vl2(void) {
  VSET(2, e64, m1);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef,
      0x55aa55aa77889900, 0xabcdef1234567890, 0xfeedfacecafebabe,
      0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
      0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  VCLEAR(v5);
  VCLEAR(v6);
  VCLEAR(v7);
  VCLEAR(v8);
  asm volatile("vlseg8e64.v v1, (%0)" ::"r"(INP1));
  VCMP_U64(67, v1, 0x9fe419208f2e05e0, 0xfeedfacecafebabe);
  VCMP_U64(68, v2, 0xf9aa71f0c394bbd3, 0x123456789abcdef0);
  VCMP_U64(69, v3, 0x123456789abcdef0, 0x1357246855aa55aa);
  VCMP_U64(70, v4, 0x13572468369b48ac, 0x369b48acdeadbeef);
  VCMP_U64(71, v5, 0xdeadbeefcafebabe, 0xcafebabe12345678);
  VCMP_U64(72, v6, 0x0123456789abcdef, 0xabcdef0987654321);
  VCMP_U64(73, v7, 0x55aa55aa77889900, 0x012345670abcdef1);
  VCMP_U64(74, v8, 0xabcdef1234567890, 0x987654321fedcba0);
}


int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE2_8_vl4();
  TEST_CASE3_8_vl4();
  TEST_CASE3_8_vl2();
  TEST_CASE4_8_vl4();
  TEST_CASE8_8_vl2();

  TEST_CASE2_16_vl4();
  TEST_CASE3_16_vl4();
  TEST_CASE4_16_vl4();
  TEST_CASE8_16_vl2();

  TEST_CASE2_32_vl4();
  TEST_CASE3_32_vl2();
  TEST_CASE3_32_vl4();
  TEST_CASE4_32_vl4();
  TEST_CASE8_32_vl2();

  TEST_CASE2_64_vl4();
  TEST_CASE3_64_vl4();
  TEST_CASE4_64_vl4();
  TEST_CASE8_64_vl2();

  EXIT_CHECK();
}