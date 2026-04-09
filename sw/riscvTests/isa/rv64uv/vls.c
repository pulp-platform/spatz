// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Navaneeth Kunhi Purayil <nkunhi@iis.ee.ethz.ch>

#include "vector_macros.h"

int8_t mask[1] = {0xAA};

// Positive-stride tests
void TEST_CASE1(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  uint64_t stride = 3;
  asm volatile("vlse8.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U8(1, v1, 0x9f, 0x20, 0x05, 0xaa);
}

void TEST_CASE2(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0,
                              0xf9aa, 0x71f0, 0xc394, 0xbbd3};
  uint64_t stride = 4;
  asm volatile("vlse16.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U16(2, v1, 0x9fe4, 0x8f2e, 0xf9aa, 0xc394);
}

void TEST_CASE3(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1};
  uint64_t stride = 8;
  asm volatile("vlse32.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U32(3, v1, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348);
}

void TEST_CASE4(void) {
#if ELEN == 64
  VSET(4, e64, m1);
  volatile uint64_t INP1[] = {0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3,
                              0xa11a9384a7163840, 0x99991348a9f38cd1};
  uint64_t stride = 8;
  asm volatile("vlse64.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U64(4, v1, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
           0x99991348a9f38cd1);
#endif
}

void TEST_CASE5(void) {
  VSET(8, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3,
                             0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  uint64_t stride = 3;
  asm volatile("vlse8.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U8(5, v1, 0x9f, 0x20, 0x05, 0xaa, 0xc3, 0xd3, 0x19, 0x2e);
}

void TEST_CASE6(void) {
  VSET(8, e16, m1);
  volatile uint16_t INP1[] = {0x7a3d, 0xc124, 0x5ef9, 0x80b7,
                              0x2dd0, 0x91e2, 0x4b6c, 0xf813,
                              0x36a5, 0xd47f, 0x0c98, 0xb2e1,
                              0x695a, 0xe70d, 0x13c4, 0xacf6};
  uint64_t stride = 4;
  asm volatile("vlse16.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U16(6, v1, 0x7a3d, 0x5ef9, 0x2dd0, 0x4b6c, 0x36a5, 0x0c98, 0x695a, 0x13c4);
}

void TEST_CASE7(void) {
  VSET(8, e32, m1);
  volatile uint32_t INP1[] = {0x8f31c4a7, 0x15be9072, 0xd26a4f1d, 0x7043e8b9,
                              0x2cb59760, 0xfa1843de, 0x61d90a85, 0x9374bc2f,
                              0x4ea12d68, 0xb8f06c13, 0x07cd52f1, 0xe39b847a,
                              0x596ef320, 0xacc1459d, 0x3b7288e4, 0xc6d0175b};
  uint64_t stride = 8;
  asm volatile("vlse32.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U32(7, v1, 0x8f31c4a7, 0xd26a4f1d, 0x2cb59760, 0x61d90a85, 0x4ea12d68, 0x07cd52f1, 0x596ef320, 0x3b7288e4);
}

void TEST_CASE8(void) {
#if ELEN == 64
  VSET(8, e64, m2);
  volatile uint64_t INP1[] = {0x7c31e9a5d0428f16, 0xb57a0c6e913df248,
                              0x18d4fa709c25be63, 0xe2b36d1487f950ac,
                              0x49cf825ab31e670d, 0x96a1780fe4d2cb35,
                              0x2df05c93a86b1741, 0xc8e49b26751adf90};
  uint64_t stride = 8;
  asm volatile("vlse64.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U64(8, v1, 0x7c31e9a5d0428f16, 0xb57a0c6e913df248, 0x18d4fa709c25be63,
           0xe2b36d1487f950ac, 0x49cf825ab31e670d, 0x96a1780fe4d2cb35,
           0x2df05c93a86b1741, 0xc8e49b26751adf90);
#endif
}

// Zero-stride tests
// The implementation must perform all the memory accesses
void TEST_CASE9(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP1[] = {0x9f};
  uint64_t stride = 0;
  asm volatile("vlse8.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U8(9, v1, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
          0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f);
}

// The implementation can also perform fewer accesses
void TEST_CASE10(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP1[] = {0x9f};
  asm volatile("vlse8.v v1, (%0), x0" ::"r"(INP1));
  VCMP_U8(10, v1, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
          0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f);
}

// Different LMUL
void TEST_CASE11(void) {
#if ELEN == 64
  VSET(8, e64, m2);
  volatile uint64_t INP1[] = {0x9fa831c7a11a9384};
  asm volatile("vlse64.v v2, (%0), x0" ::"r"(INP1));
  VCMP_U64(11, v2, 0x9fa831c7a11a9384, 0x9fa831c7a11a9384, 0x9fa831c7a11a9384,
           0x9fa831c7a11a9384, 0x9fa831c7a11a9384, 0x9fa831c7a11a9384,
           0x9fa831c7a11a9384, 0x9fa831c7a11a9384);
#endif
}

// Others
// Negative-stride test
void TEST_CASE12(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0,
                              0xf9aa, 0x71f0, 0xc394, 0xbbd3};
  uint64_t stride = -4;
  asm volatile("vlse16.v v1, (%0), %1" ::"r"(&INP1[7]), "r"(stride));
  VCMP_U16(12, v1, 0xbbd3, 0x71f0, 0x05e0, 0x1920);
}

// Stride > SpatzMemBytes
void TEST_CASE13(void) {
#if ELEN == 64
  VSET(2, e64, m1);
  volatile uint64_t INP1[] = {0x99991348a9f38cd1, 0x9fa831c7a11a9384,
                              0x9fa831c7a11a9384, 0x9fa831c7a11a9384,
                              0x9fa831c7a11a9384, 0x01015ac1309bb678};
  uint64_t stride = 40;
  asm volatile("vlse64.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U64(13, v1, 0x99991348a9f38cd1, 0x01015ac1309bb678);
#endif
}

// Fill Spatz register
void TEST_CASE14(void) {
#if ELEN == 64
  VSET(8, e64, m2);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
      0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548,
      0x1893179501093489, 0x81937598aa819388, 0x1874754791888188,
      0x3eeeeeeee33111ae, 0x9013930148815808, 0xab8b914891484891,
      0x9031850931584902, 0x3189759837598759, 0x8319599991911111,
      0x8913984898951989};
  uint64_t stride = 16;
  asm volatile("vlse64.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U64(14, v1, 0x9fe419208f2e05e0, 0xa11a9384a7163840, 0x9fa831c7a11a9384,
           0x1893179501093489, 0x1874754791888188, 0x9013930148815808,
           0x9031850931584902, 0x8319599991911111);
#endif
}

// Masked stride loads
void TEST_CASE15(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  uint64_t stride = 3;
  VLOAD_8(v0, 0xAA);
  VCLEAR(v1);
  asm volatile("vlse8.v v1, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U8(15, v1, 0x00, 0x20, 0x00, 0xaa);
}

void TEST_CASE16(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0,
                              0xf9aa, 0x71f0, 0xc394, 0xbbd3};
  uint64_t stride = 4;
  VLOAD_8(v0, 0xAA);
  VCLEAR(v1);
  asm volatile("vlse16.v v1, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U16(16, v1, 0, 0x8f2e, 0, 0xc394);
}

void TEST_CASE17(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1};
  uint64_t stride = 8;
  VLOAD_8(v0, 0xAA);
  VCLEAR(v1);
  asm volatile("vlse32.v v1, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U32(17, v1, 0, 0xf9aa71f0, 0, 0x99991348);
}

void TEST_CASE18(void) {
#if ELEN == 64
  VSET(8, e64, m2);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
      0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548,
      0x1893179501093489, 0x81937598aa819388, 0x1874754791888188,
      0x3eeeeeeee33111ae, 0x9013930148815808, 0xab8b914891484891,
      0x9031850931584902, 0x3189759837598759, 0x8319599991911111,
      0x8913984898951989};
  uint64_t stride = 16;
  VLOAD_8(v0, 0xAA);
  VCLEAR(v1);
  asm volatile("vlse64.v v2, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U64(18, v2, 0, 0xa11a9384a7163840, 0, 0x1893179501093489, 0,
           0x9013930148815808, 0, 0x8319599991911111);
#endif
}

// Large vector lengths
void TEST_CASE19(void) {
  VSET(128, e8, m4);
  volatile uint8_t INP1[] = {0x3d, 0x7c, 0xa1, 0x52, 0xf3, 0x0b, 0x86, 0xc9,
                             0x4e, 0x17, 0xda, 0x63, 0x95, 0x2f, 0x71, 0xb8,
                             0x08, 0xe5, 0x3a, 0x6d, 0xc4, 0x90, 0x5b, 0x27,
                             0xfe, 0x44, 0x13, 0x79, 0xab, 0xd6, 0x61, 0x1c,
                             0x82, 0x3f, 0xee, 0x55, 0x09, 0xbc, 0x74, 0x30,
                             0xcb, 0x6a, 0xf7, 0x21, 0x9e, 0x48, 0xd3, 0x8c,
                             0x15, 0x70, 0xa7, 0x4b, 0xe0, 0x2d, 0x93, 0x5f,
                             0xb4, 0x39, 0x67, 0x0c, 0xdf, 0x84, 0x1a, 0x46,
                             0xfe, 0x44, 0x13, 0x79, 0xab, 0xd6, 0x61, 0x1c,
                             0x82, 0x3f, 0xee, 0x55, 0x09, 0xbc, 0x74, 0x30,
                             0xcb, 0x6a, 0xf7, 0x21, 0x9e, 0x48, 0xd3, 0x8c,
                             0x15, 0x70, 0xa7, 0x4b, 0xe0, 0x2d, 0x93, 0x5f,
                             0xb4, 0x39, 0x67, 0x0c, 0xdf, 0x84, 0x1a, 0x46,
                             0xcb, 0x6a, 0xf7, 0x21, 0x9e, 0x48, 0xd3, 0x8c,
                             0x15, 0x70, 0xa7, 0x4b, 0xe0, 0x2d, 0x93, 0x5f,
                             0xb4, 0x39, 0x67, 0x0c, 0xdf, 0x84, 0x1a, 0x46};
  uint64_t stride = 1;
  asm volatile("vlse8.v v4, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U8(19, v4, 0x3d, 0x7c, 0xa1, 0x52, 0xf3, 0x0b, 0x86, 0xc9,
                             0x4e, 0x17, 0xda, 0x63, 0x95, 0x2f, 0x71, 0xb8,
                             0x08, 0xe5, 0x3a, 0x6d, 0xc4, 0x90, 0x5b, 0x27,
                             0xfe, 0x44, 0x13, 0x79, 0xab, 0xd6, 0x61, 0x1c,
                             0x82, 0x3f, 0xee, 0x55, 0x09, 0xbc, 0x74, 0x30,
                             0xcb, 0x6a, 0xf7, 0x21, 0x9e, 0x48, 0xd3, 0x8c,
                             0x15, 0x70, 0xa7, 0x4b, 0xe0, 0x2d, 0x93, 0x5f,
                             0xb4, 0x39, 0x67, 0x0c, 0xdf, 0x84, 0x1a, 0x46,
                             0xfe, 0x44, 0x13, 0x79, 0xab, 0xd6, 0x61, 0x1c,
                             0x82, 0x3f, 0xee, 0x55, 0x09, 0xbc, 0x74, 0x30,
                             0xcb, 0x6a, 0xf7, 0x21, 0x9e, 0x48, 0xd3, 0x8c,
                             0x15, 0x70, 0xa7, 0x4b, 0xe0, 0x2d, 0x93, 0x5f,
                             0xb4, 0x39, 0x67, 0x0c, 0xdf, 0x84, 0x1a, 0x46,
                             0xcb, 0x6a, 0xf7, 0x21, 0x9e, 0x48, 0xd3, 0x8c,
                             0x15, 0x70, 0xa7, 0x4b, 0xe0, 0x2d, 0x93, 0x5f,
                             0xb4, 0x39, 0x67, 0x0c, 0xdf, 0x84, 0x1a, 0x46);
}

void TEST_CASE20(void) {
  VSET(64, e16, m4);
  volatile uint16_t INP1[] = {0x7a3d, 0xc124, 0x5ef9, 0x80b7,
                              0x2dd0, 0x91e2, 0x4b6c, 0xf813,
                              0x36a5, 0xd47f, 0x0c98, 0xb2e1,
                              0x695a, 0xe70d, 0x13c4, 0xacf6,
                              0x41be, 0x9d73, 0x26e8, 0xf05c,
                              0x7b12, 0xc9af, 0x58d4, 0x0e67,
                              0xb84a, 0x3fd1, 0xe295, 0x649c,
                              0x1ab0, 0x95e6, 0x4c3b, 0xda78,
                              0x2f1d, 0x83c6, 0x6e50, 0xb79a,
                              0x145f, 0xe8c3, 0x3974, 0xa2ed,
                              0x5c81, 0xd6b4, 0x087a, 0xf193,
                              0x72ce, 0x2a45, 0x9fb8, 0x4de0,
                              0xc367, 0x1e9a, 0xb50f, 0x6842,
                              0xfad1, 0x305c, 0x87e4, 0x5b29,
                              0xdc70, 0x243f, 0x96a1, 0x4f8c,
                              0xe312, 0x79d5, 0x0b6e, 0xc854,
                            };
  uint64_t stride = 2;
  asm volatile("vlse16.v v4, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U16(20, v4, 0x7a3d, 0xc124, 0x5ef9, 0x80b7,
                   0x2dd0, 0x91e2, 0x4b6c, 0xf813,
                   0x36a5, 0xd47f, 0x0c98, 0xb2e1,
                   0x695a, 0xe70d, 0x13c4, 0xacf6,
                   0x41be, 0x9d73, 0x26e8, 0xf05c,
                   0x7b12, 0xc9af, 0x58d4, 0x0e67,
                   0xb84a, 0x3fd1, 0xe295, 0x649c,
                   0x1ab0, 0x95e6, 0x4c3b, 0xda78,
                   0x2f1d, 0x83c6, 0x6e50, 0xb79a,
                   0x145f, 0xe8c3, 0x3974, 0xa2ed,
                   0x5c81, 0xd6b4, 0x087a, 0xf193,
                   0x72ce, 0x2a45, 0x9fb8, 0x4de0,
                   0xc367, 0x1e9a, 0xb50f, 0x6842,
                   0xfad1, 0x305c, 0x87e4, 0x5b29,
                   0xdc70, 0x243f, 0x96a1, 0x4f8c,
                   0xe312, 0x79d5, 0x0b6e, 0xc854);
}

void TEST_CASE21(void) {
  VSET(32, e32, m4);
  volatile uint32_t INP1[] = {0x8f31c4a7, 0x15be9072, 0xd26a4f1d, 0x7043e8b9,
                              0x2cb59760, 0xfa1843de, 0x61d90a85, 0x9374bc2f,
                              0x4ea12d68, 0xb8f06c13, 0x07cd52f1, 0xe39b847a,
                              0x596ef320, 0xacc1459d, 0x3b7288e4, 0xc6d0175b,
                              0x03430f29, 0x7a1b2c86, 0xf8e5d3c4, 0x9b6a1e72,
                              0x5c8f4a1d, 0xe2b36d14, 0x48f7a9c3, 0x1d0e5b8a,
                              0x6f2c9d57, 0xa7b8c4e1, 0x2d3f6a98, 0x83e1b745,
                              0x4c9a2ddf, 0xf1e8a6b2, 0x9d73c5e8, 0x60a1f394};
  uint64_t stride = 4;
  asm volatile("vlse32.v v4, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U32(21, v4, 0x8f31c4a7, 0x15be9072, 0xd26a4f1d, 0x7043e8b9,
                   0x2cb59760, 0xfa1843de, 0x61d90a85, 0x9374bc2f,
                   0x4ea12d68, 0xb8f06c13, 0x07cd52f1, 0xe39b847a,
                   0x596ef320, 0xacc1459d, 0x3b7288e4, 0xc6d0175b,
                   0x03430f29, 0x7a1b2c86, 0xf8e5d3c4, 0x9b6a1e72,
                   0x5c8f4a1d, 0xe2b36d14, 0x48f7a9c3, 0x1d0e5b8a,
                   0x6f2c9d57, 0xa7b8c4e1, 0x2d3f6a98, 0x83e1b745,
                   0x4c9a2ddf, 0xf1e8a6b2, 0x9d73c5e8, 0x60a1f394);
}

void TEST_CASE22(void) {
#if ELEN == 64
  VSET(16, e64, m4);
  volatile uint64_t INP1[] = {0x7c31e9a5d0428f16, 0xb57a0c6e913df248,
                              0x18d4fa709c25be63, 0xe2b36d1487f950ac,
                              0x49cf825ab31e670d, 0x96a1780fe4d2cb35,
                              0x2df05c93a86b1741, 0xc8e49b26751adf90,
                              0x7c31e9245d0428f1, 0xb57a0c6e913df248,
                              0x18d4fa709c259be6, 0x3db36d1487f950ac,
                              0x49cf825ab31e670d, 0x96a1ffffe4d2cb35,
                              0x2df05c937b6b1741, 0xc8e2cb26751adf90};
  uint64_t stride = 8;
  asm volatile("vlse64.v v4, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U64(22, v4, 0x7c31e9a5d0428f16, 0xb57a0c6e913df248,
                   0x18d4fa709c25be63, 0xe2b36d1487f950ac,
                   0x49cf825ab31e670d, 0x96a1780fe4d2cb35,
                   0x2df05c93a86b1741, 0xc8e49b26751adf90,
                   0x7c31e9245d0428f1, 0xb57a0c6e913df248,
                   0x18d4fa709c259be6, 0x3db36d1487f950ac,
                   0x49cf825ab31e670d, 0x96a1ffffe4d2cb35,
                   0x2df05c937b6b1741, 0xc8e2cb26751adf90);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  // Positive-stride tests
  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();

  TEST_CASE5();
  TEST_CASE6();
  TEST_CASE7();
  TEST_CASE8();

  // Zero-stride tests
  TEST_CASE9();
  TEST_CASE10();
  TEST_CASE11();

  // Negative-stride test
  TEST_CASE12();
  // Stride > SpatzMemBytes (default)
  TEST_CASE13();
  // Fill Spatz register
  TEST_CASE14();

  // TODO: Masked stride loads
  // TEST_CASE15();
  // TEST_CASE16();
  // TEST_CASE17();
  // TEST_CASE18();

  // Large vector lengths
  TEST_CASE19();
  TEST_CASE20();
  TEST_CASE21();
  TEST_CASE22();

  EXIT_CHECK();
}
