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
#if ELEN == 64
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
#endif
}

// Segment-3 for 64-bit, vl = 4 --> 96 bytes
void TEST_CASE3_64_vl4(void) {
#if ELEN == 64
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
#endif
}

// Segment-4 for 64-bit, vl = 4 --> 128 bytes
void TEST_CASE4_64_vl4(void) {
#if ELEN == 64
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
#endif
}

// Segment-8 for 64-bit, vl = 2 --> 128 bytes
void TEST_CASE8_64_vl2(void) {
#if ELEN == 64
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
#endif
}

/////////////////////////////////
// Masked segment loads (v0.t) //
/////////////////////////////////

// Segment-2 for 8-bit, vl = 4, mask 0b1010
void TEST_CASE2_8_vl4_m(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(75, v1, 0x00, 0x19, 0x00, 0x05);
  VCMP_U8(76, v2, 0x00, 0x20, 0x00, 0xe0);
}

// Segment-3 for 8-bit, vl = 4, mask 0b0101
void TEST_CASE3_8_vl4_m(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VLOAD_8(v0, 0x05, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(77, v1, 0x9f, 0x00, 0x05, 0x00);
  VCMP_U8(78, v2, 0xe4, 0x00, 0xe0, 0x00);
  VCMP_U8(79, v3, 0x19, 0x00, 0xf9, 0x00);
}

// Segment-3 for 8-bit, vl = 2, mask 0b10
void TEST_CASE3_8_vl2_m(void) {
  VSET(2, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VLOAD_8(v0, 0x02, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(80, v1, 0x00, 0x20);
  VCMP_U8(81, v2, 0x00, 0x8f);
  VCMP_U8(82, v3, 0x00, 0x2e);
}

// Segment-4 for 8-bit, vl = 4, mask 0b1001
void TEST_CASE4_8_vl4_m(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VLOAD_8(v0, 0x09, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(83, v1, 0x9f, 0x00, 0x00, 0xc3);
  VCMP_U8(84, v2, 0xe4, 0x00, 0x00, 0x94);
  VCMP_U8(85, v3, 0x19, 0x00, 0x00, 0xbb);
  VCMP_U8(86, v4, 0x20, 0x00, 0x00, 0xd3);
}

// Segment-8 for 8-bit, vl = 2, mask 0b01
void TEST_CASE8_8_vl2_m(void) {
  VSET(2, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VLOAD_8(v0, 0x01, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  VCLEAR(v5);
  VCLEAR(v6);
  VCLEAR(v7);
  VCLEAR(v8);
  asm volatile("vlseg8e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(87, v1, 0x9f, 0x00);
  VCMP_U8(88, v2, 0xe4, 0x00);
  VCMP_U8(89, v3, 0x19, 0x00);
  VCMP_U8(90, v4, 0x20, 0x00);
  VCMP_U8(91, v5, 0x8f, 0x00);
  VCMP_U8(92, v6, 0x2e, 0x00);
  VCMP_U8(93, v7, 0x05, 0x00);
  VCMP_U8(94, v8, 0xe0, 0x00);
}

// Segment-2 for 16-bit, vl = 4, mask 0b0110
void TEST_CASE2_16_vl4_m(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VLOAD_8(v0, 0x06, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e16.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U16(95, v1, 0x0000, 0x8f2e, 0xf9aa, 0x0000);
  VCMP_U16(96, v2, 0x0000, 0x05e0, 0x71f0, 0x0000);
}

// Segment-3 for 16-bit, vl = 4, mask 0b1010
void TEST_CASE3_16_vl4_m(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e16.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U16(97, v1, 0x0000, 0x05e0, 0x0000, 0x5678);
  VCMP_U16(98, v2, 0x0000, 0xf9aa, 0x0000, 0x9abc);
  VCMP_U16(99, v3, 0x0000, 0x71f0, 0x0000, 0xdef0);
}

// Segment-4 for 16-bit, vl = 4, mask 0b0101
void TEST_CASE4_16_vl4_m(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VLOAD_8(v0, 0x05, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e16.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U16(100, v1, 0x9fe4, 0x0000, 0x1234, 0x0000);
  VCMP_U16(101, v2, 0x1920, 0x0000, 0x5678, 0x0000);
  VCMP_U16(102, v3, 0x8f2e, 0x0000, 0x9abc, 0x0000);
  VCMP_U16(103, v4, 0x05e0, 0x0000, 0xdef0, 0x0000);
}

// Segment-8 for 16-bit, vl = 2, mask 0b10
void TEST_CASE8_16_vl2_m(void) {
  VSET(2, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VLOAD_8(v0, 0x02, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  VCLEAR(v5);
  VCLEAR(v6);
  VCLEAR(v7);
  VCLEAR(v8);
  asm volatile("vlseg8e16.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U16(104, v1, 0x0000, 0x1234);
  VCMP_U16(105, v2, 0x0000, 0x5678);
  VCMP_U16(106, v3, 0x0000, 0x9abc);
  VCMP_U16(107, v4, 0x0000, 0xdef0);
  VCMP_U16(108, v5, 0x0000, 0x1357);
  VCMP_U16(109, v6, 0x0000, 0x2468);
  VCMP_U16(110, v7, 0x0000, 0x369b);
  VCMP_U16(111, v8, 0x0000, 0x48ac);
}

// Segment-2 for 32-bit, vl = 4, mask 0b1100
void TEST_CASE2_32_vl4_m(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VLOAD_8(v0, 0x0C, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e32.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U32(112, v1, 0x00000000, 0x00000000, 0x12345678, 0x13572468);
  VCMP_U32(113, v2, 0x00000000, 0x00000000, 0x9abcdef0, 0x369b48ac);
}

// Segment-3 for 32-bit, vl = 4, mask 0b0011
void TEST_CASE3_32_vl4_m(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VLOAD_8(v0, 0x03, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e32.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U32(114, v1, 0x9fe41920, 0xc394bbd3, 0x00000000, 0x00000000);
  VCMP_U32(115, v2, 0x8f2e05e0, 0x12345678, 0x00000000, 0x00000000);
  VCMP_U32(116, v3, 0xf9aa71f0, 0x9abcdef0, 0x00000000, 0x00000000);
}

// Segment-3 for 32-bit, vl = 2, mask 0b01
void TEST_CASE3_32_vl2_m(void) {
  VSET(2, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VLOAD_8(v0, 0x01, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e32.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U32(117, v1, 0x9fe41920, 0x00000000);
  VCMP_U32(118, v2, 0x8f2e05e0, 0x00000000);
  VCMP_U32(119, v3, 0xf9aa71f0, 0x00000000);
}

// Segment-4 for 32-bit, vl = 4, mask 0b1010
void TEST_CASE4_32_vl4_m(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e32.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U32(120, v1, 0x00000000, 0x12345678, 0x00000000, 0x55aa55aa);
  VCMP_U32(121, v2, 0x00000000, 0x9abcdef0, 0x00000000, 0x77889900);
  VCMP_U32(122, v3, 0x00000000, 0x13572468, 0x00000000, 0xabcdef12);
  VCMP_U32(123, v4, 0x00000000, 0x369b48ac, 0x00000000, 0x34567890);
}

// Segment-8 for 32-bit, vl = 2, mask 0b10
void TEST_CASE8_32_vl2_m(void) {
  VSET(2, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VLOAD_8(v0, 0x02, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  VCLEAR(v5);
  VCLEAR(v6);
  VCLEAR(v7);
  VCLEAR(v8);
  asm volatile("vlseg8e32.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U32(124, v1, 0x00000000, 0xdeadbeef);
  VCMP_U32(125, v2, 0x00000000, 0xcafebabe);
  VCMP_U32(126, v3, 0x00000000, 0x01234567);
  VCMP_U32(127, v4, 0x00000000, 0x89abcdef);
  VCMP_U32(128, v5, 0x00000000, 0x55aa55aa);
  VCMP_U32(129, v6, 0x00000000, 0x77889900);
  VCMP_U32(130, v7, 0x00000000, 0xabcdef12);
  VCMP_U32(131, v8, 0x00000000, 0x34567890);
}

// Segment-2 for 64-bit, vl = 4, mask 0b0101
void TEST_CASE2_64_vl4_m(void) {
#if ELEN == 64
  VSET(4, e64, m1);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef,
      0x55aa55aa77889900, 0xabcdef1234567890, 0xfeedfacecafebabe,
      0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
      0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};
  VLOAD_8(v0, 0x05, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e64.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U64(132, v1, 0x9fe419208f2e05e0, 0x0000000000000000, 0xdeadbeefcafebabe, 0x0000000000000000);
  VCMP_U64(133, v2, 0xf9aa71f0c394bbd3, 0x0000000000000000, 0x0123456789abcdef, 0x0000000000000000);
#endif
}

// Segment-3 for 64-bit, vl = 4, mask 0b1010
void TEST_CASE3_64_vl4_m(void) {
#if ELEN == 64
  VSET(4, e64, m1);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef,
      0x55aa55aa77889900, 0xabcdef1234567890, 0xfeedfacecafebabe,
      0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
      0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};
  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e64.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U64(134, v1, 0x0000000000000000, 0x13572468369b48ac, 0x0000000000000000, 0x123456789abcdef0);
  VCMP_U64(135, v2, 0x0000000000000000, 0xdeadbeefcafebabe, 0x0000000000000000, 0x1357246855aa55aa);
  VCMP_U64(136, v3, 0x0000000000000000, 0x0123456789abcdef, 0x0000000000000000, 0x369b48acdeadbeef);
#endif
}

// Segment-4 for 64-bit, vl = 4, mask 0b0110
void TEST_CASE4_64_vl4_m(void) {
#if ELEN == 64
  VSET(4, e64, m1);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef,
      0x55aa55aa77889900, 0xabcdef1234567890, 0xfeedfacecafebabe,
      0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
      0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};
  VLOAD_8(v0, 0x06, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e64.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U64(137, v1, 0x0000000000000000, 0xdeadbeefcafebabe, 0xfeedfacecafebabe, 0x0000000000000000);
  VCMP_U64(138, v2, 0x0000000000000000, 0x0123456789abcdef, 0x123456789abcdef0, 0x0000000000000000);
  VCMP_U64(139, v3, 0x0000000000000000, 0x55aa55aa77889900, 0x1357246855aa55aa, 0x0000000000000000);
  VCMP_U64(140, v4, 0x0000000000000000, 0xabcdef1234567890, 0x369b48acdeadbeef, 0x0000000000000000);
#endif
}

// Segment-8 for 64-bit, vl = 2, mask 0b01
void TEST_CASE8_64_vl2_m(void) {
#if ELEN == 64
  VSET(2, e64, m1);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef,
      0x55aa55aa77889900, 0xabcdef1234567890, 0xfeedfacecafebabe,
      0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
      0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};
  VLOAD_8(v0, 0x01, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  VCLEAR(v5);
  VCLEAR(v6);
  VCLEAR(v7);
  VCLEAR(v8);
  asm volatile("vlseg8e64.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U64(141, v1, 0x9fe419208f2e05e0, 0x0000000000000000);
  VCMP_U64(142, v2, 0xf9aa71f0c394bbd3, 0x0000000000000000);
  VCMP_U64(143, v3, 0x123456789abcdef0, 0x0000000000000000);
  VCMP_U64(144, v4, 0x13572468369b48ac, 0x0000000000000000);
  VCMP_U64(145, v5, 0xdeadbeefcafebabe, 0x0000000000000000);
  VCMP_U64(146, v6, 0x0123456789abcdef, 0x0000000000000000);
  VCMP_U64(147, v7, 0x55aa55aa77889900, 0x0000000000000000);
  VCMP_U64(148, v8, 0xabcdef1234567890, 0x0000000000000000);
#endif
}

//////////////////
// Corner cases //
//////////////////

// All-zeros mask: no segment is loaded at all, destinations stay untouched
void TEST_CASE4_8_vl4_m_none(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VLOAD_8(v0, 0x00, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(149, v1, 0x00, 0x00, 0x00, 0x00);
  VCMP_U8(150, v2, 0x00, 0x00, 0x00, 0x00);
  VCMP_U8(151, v3, 0x00, 0x00, 0x00, 0x00);
  VCMP_U8(152, v4, 0x00, 0x00, 0x00, 0x00);
}

// All-ones mask: must be bit-identical to the unmasked TEST_CASE4_8_vl4
void TEST_CASE4_8_vl4_m_all(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VLOAD_8(v0, 0x0F, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(153, v1, 0x9f, 0x8f, 0xf9, 0xc3);
  VCMP_U8(154, v2, 0xe4, 0x2e, 0xaa, 0x94);
  VCMP_U8(155, v3, 0x19, 0x05, 0x71, 0xbb);
  VCMP_U8(156, v4, 0x20, 0xe0, 0xf0, 0xd3);
}

// Non-zero background: masked-off elements must keep their previous value
// (mask-undisturbed), not be zeroed or filled with ones
void TEST_CASE2_8_vl4_m_undist(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VLOAD_8(v1, 0xa1, 0xa1, 0xa1, 0xa1);
  VLOAD_8(v2, 0xb2, 0xb2, 0xb2, 0xb2);
  asm volatile("vlseg2e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(157, v1, 0xa1, 0x19, 0xa1, 0x05);
  VCMP_U8(158, v2, 0xb2, 0x20, 0xb2, 0xe0);
}

// Same, on a wider SEW
void TEST_CASE2_32_vl4_m_undist(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VLOAD_8(v0, 0x06, 0x00, 0x00, 0x00);
  VLOAD_32(v1, 0xa1a1a1a1, 0xa1a1a1a1, 0xa1a1a1a1, 0xa1a1a1a1);
  VLOAD_32(v2, 0xb2b2b2b2, 0xb2b2b2b2, 0xb2b2b2b2, 0xb2b2b2b2);
  asm volatile("vlseg2e32.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U32(159, v1, 0xa1a1a1a1, 0xf9aa71f0, 0x12345678, 0xa1a1a1a1);
  VCMP_U32(160, v2, 0xb2b2b2b2, 0xc394bbd3, 0x9abcdef0, 0xb2b2b2b2);
}

// Segment-2 for 8-bit, vl = 32 --> 64 bytes
void TEST_CASE2_8_vl32(void) {
  VSET(32, e8, m1);
  volatile uint8_t INP1[] = {0x31, 0xda, 0x32, 0x6f, 0xb5, 0x1a, 0xe6, 0xc7,
                             0xc1, 0x73, 0x67, 0x1e, 0xd0, 0xab, 0xbb, 0x46,
                             0x74, 0x2b, 0xa7, 0xf6, 0x50, 0xc3, 0xab, 0xee,
                             0x64, 0x73, 0xd7, 0xbe, 0x00, 0x6b, 0xab, 0xc6,
                             0x04, 0x4b, 0xb7, 0x36, 0x40, 0x43, 0x7b, 0x0e,
                             0x74, 0xb3, 0xc7, 0xde, 0xf0, 0x6b, 0x5b, 0xc6,
                             0x54, 0x2b, 0x07, 0xf6, 0x30, 0x03, 0x8b, 0xee,
                             0x04, 0xb3, 0xb7, 0xbe, 0xa0, 0xab, 0x4b, 0x46};
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e8.v v1, (%0)" ::"r"(INP1));
  VCMP_U8(161, v1, 0x31, 0x32, 0xb5, 0xe6, 0xc1, 0x67, 0xd0, 0xbb,
                   0x74, 0xa7, 0x50, 0xab, 0x64, 0xd7, 0x00, 0xab,
                   0x04, 0xb7, 0x40, 0x7b, 0x74, 0xc7, 0xf0, 0x5b,
                   0x54, 0x07, 0x30, 0x8b, 0x04, 0xb7, 0xa0, 0x4b);
  VCMP_U8(162, v2, 0xda, 0x6f, 0x1a, 0xc7, 0x73, 0x1e, 0xab, 0x46,
                   0x2b, 0xf6, 0xc3, 0xee, 0x73, 0xbe, 0x6b, 0xc6,
                   0x4b, 0x36, 0x43, 0x0e, 0xb3, 0xde, 0x6b, 0xc6,
                   0x2b, 0xf6, 0x03, 0xee, 0xb3, 0xbe, 0xab, 0x46);
}

// Segment-2 for 8-bit, vl = 32 --> 64 bytes, mask 0xC3A5F00F
void TEST_CASE2_8_vl32_m(void) {
  VSET(32, e8, m1);
  volatile uint8_t INP1[] = {0x31, 0xda, 0x32, 0x6f, 0xb5, 0x1a, 0xe6, 0xc7,
                             0xc1, 0x73, 0x67, 0x1e, 0xd0, 0xab, 0xbb, 0x46,
                             0x74, 0x2b, 0xa7, 0xf6, 0x50, 0xc3, 0xab, 0xee,
                             0x64, 0x73, 0xd7, 0xbe, 0x00, 0x6b, 0xab, 0xc6,
                             0x04, 0x4b, 0xb7, 0x36, 0x40, 0x43, 0x7b, 0x0e,
                             0x74, 0xb3, 0xc7, 0xde, 0xf0, 0x6b, 0x5b, 0xc6,
                             0x54, 0x2b, 0x07, 0xf6, 0x30, 0x03, 0x8b, 0xee,
                             0x04, 0xb3, 0xb7, 0xbe, 0xa0, 0xab, 0x4b, 0x46};
  VLOAD_8(v0, 0x0f, 0xf0, 0xa5, 0xc3, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e8.v v1, (%0), v0.t" ::"r"(INP1));
  VCMP_U8(163, v1, 0x31, 0x32, 0xb5, 0xe6, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x64, 0xd7, 0x00, 0xab,
                   0x04, 0x00, 0x40, 0x00, 0x00, 0xc7, 0x00, 0x5b,
                   0x54, 0x07, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x4b);
  VCMP_U8(164, v2, 0xda, 0x6f, 0x1a, 0xc7, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x73, 0xbe, 0x6b, 0xc6,
                   0x4b, 0x00, 0x43, 0x00, 0x00, 0xde, 0x00, 0xc6,
                   0x2b, 0xf6, 0x00, 0x00, 0x00, 0x00, 0xab, 0x46);
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

  // Masked (v0.t)
  TEST_CASE2_8_vl4_m();
  TEST_CASE3_8_vl4_m();
  TEST_CASE3_8_vl2_m();
  TEST_CASE4_8_vl4_m();
  TEST_CASE8_8_vl2_m();

  TEST_CASE2_16_vl4_m();
  TEST_CASE3_16_vl4_m();
  TEST_CASE4_16_vl4_m();
  TEST_CASE8_16_vl2_m();

  TEST_CASE2_32_vl4_m();
  TEST_CASE3_32_vl2_m();
  TEST_CASE3_32_vl4_m();
  TEST_CASE4_32_vl4_m();
  TEST_CASE8_32_vl2_m();

  TEST_CASE2_64_vl4_m();
  TEST_CASE3_64_vl4_m();
  TEST_CASE4_64_vl4_m();
  TEST_CASE8_64_vl2_m();

  // Corner cases
  TEST_CASE4_8_vl4_m_none();
  TEST_CASE4_8_vl4_m_all();
  TEST_CASE2_8_vl4_m_undist();
  TEST_CASE2_32_vl4_m_undist();
  TEST_CASE2_8_vl32();
  TEST_CASE2_8_vl32_m();

  EXIT_CHECK();
}