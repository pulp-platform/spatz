// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: Riccardo Giunti - Fondazione CHIPS-IT


#include "vector_macros.h"

#define INIT 98

// Segment-2 for 8-bit, vl = 4 --> 8 bytes
void TEST_CASE2_8_vl4(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[]      = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                                  0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e8.v v1, (%0)" ::"r"(BUFFER_O8));

  VVCMP_U8(1, BUFFER_O8, 0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                         INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT);
}

// Segment-3 for 8-bit, vl = 4 --> 12 bytes
void TEST_CASE3_8_vl4(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg3e8.v v1, (%0)" ::"r"(BUFFER_O8));

  VVCMP_U8(2, BUFFER_O8, 0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0, 0xf9,
           0xaa, 0x71, 0xf0, INIT, INIT, INIT, INIT);
}

// Segment-4 for 8-bit, vl = 4 --> 16 bytes
void TEST_CASE4_8_vl4(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e8.v v1, (%0)" ::"r"(BUFFER_O8));

  VVCMP_U8(3, BUFFER_O8, 0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0, 0xf9,
           0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3);
}

// Segment-8 for 8-bit, vl = 2 --> 16 bytes
void TEST_CASE8_8_vl2(void) {
  VSET(2, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

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
  asm volatile("vsseg8e8.v v1, (%0)" ::"r"(BUFFER_O8));

  VVCMP_U8(4, BUFFER_O8, 0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0, 0xf9,
           0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3);
}

// Segment-2 for 16-bit, vl = 4 --> 16 bytes
void TEST_CASE2_16_vl4(void) {
  VSET(4, e16, m1);

  volatile uint16_t BUFFER_O16[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e16.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e16.v v1, (%0)" ::"r"(BUFFER_O16));

  VVCMP_U16(5, BUFFER_O16, 0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0, 0xc394, 0xbbd3,
                           INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT);
}

// Segment-3 for 16-bit, vl = 4 --> 24 bytes
void TEST_CASE3_16_vl4(void) {
  VSET(4, e16, m1);

  volatile uint16_t BUFFER_O16[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e16.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg3e16.v v1, (%0)" ::"r"(BUFFER_O16));

  VVCMP_U16(6, BUFFER_O16, 0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,  0xc394, 0xbbd3,
                           0x1234, 0x5678, 0x9abc, 0xdef0, INIT  , INIT  ,  INIT  , INIT);
}

// Segment-4 for 16-bit, vl = 4 --> 32 bytes
void TEST_CASE4_16_vl4(void) {
  VSET(4, e16, m1);

  volatile uint16_t BUFFER_O16[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};

  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e16.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e16.v v1, (%0)" ::"r"(BUFFER_O16));

  VVCMP_U16(7, BUFFER_O16, 0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0, 0xc394, 0xbbd3,
                           0x1234, 0x5678, 0x9abc, 0xdef0, 0x1357, 0x2468, 0x369b, 0x48ac);
}

// Segment-8 for 16-bit, vl = 2 --> 16 bytes
void TEST_CASE8_16_vl2(void) {
  VSET(2, e16, m1);

  volatile uint16_t BUFFER_O16[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

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
  asm volatile("vsseg8e16.v v1, (%0)" ::"r"(BUFFER_O16));

  VVCMP_U16(8, BUFFER_O16, 0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0, 0xc394, 0xbbd3,
                           0x1234, 0x5678, 0x9abc, 0xdef0, 0x1357, 0x2468, 0x369b, 0x48ac);
}

// Segment-2 for 32-bit, vl = 4 --> 32 bytes
void TEST_CASE2_32_vl4(void) {
  VSET(4, e32, m1);
  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e32.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e32.v v1, (%0)" ::"r"(BUFFER_O32));

  VVCMP_U32(9, BUFFER_O32, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                           0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                           INIT      , INIT      , INIT      , INIT      ,
                           INIT      , INIT      , INIT      , INIT);
}

// Segment-3 for 32-bit, vl = 4 --> 48 bytes
void TEST_CASE3_32_vl4(void) {
  VSET(4, e32, m1);
  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e32.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg3e32.v v1, (%0)" ::"r"(BUFFER_O32));

  VVCMP_U32(10, BUFFER_O32, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                            0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                            0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                            INIT      , INIT      , INIT      , INIT);
}

// Segment-4 for 32-bit, vl = 4 --> 64 bytes
void TEST_CASE4_32_vl4(void) {
  VSET(4, e32, m1);
  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e32.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e32.v v1, (%0)" ::"r"(BUFFER_O32));

  VVCMP_U32(11, BUFFER_O32, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                            0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                            0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                            0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890);
}

// Segment-8 for 32-bit, vl = 2 --> 64 bytes
void TEST_CASE8_32_vl2(void) {
  VSET(2, e32, m1);
  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT}; 

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
  asm volatile("vsseg8e32.v v1, (%0)" ::"r"(BUFFER_O32));

  VVCMP_U32(12, BUFFER_O32, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                            0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                            0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                            0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890);
}

// Segment-2 for 64-bit, vl = 4 --> 64 bytes
void TEST_CASE2_64_vl4(void) {
  VSET(4, e64, m1);
  volatile uint64_t BUFFER_O64[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT}; 
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900,
      0xabcdef1234567890, 0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa,
      0x369b48acdeadbeef, 0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};

  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e64.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e64.v v1, (%0)" ::"r"(BUFFER_O64));

  VVCMP_U64(13, BUFFER_O64, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0, 0x13572468369b48ac,
                            0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900, 0xabcdef1234567890,
                            INIT              , INIT              , INIT              , INIT,
                            INIT              , INIT              , INIT              , INIT);

}

// Segment-3 for 64-bit, vl = 4 --> 96 bytes
void TEST_CASE3_64_vl4(void) {
  VSET(4, e64, m1);
  volatile uint64_t BUFFER_O64[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT}; 
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900,
      0xabcdef1234567890, 0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa,
      0x369b48acdeadbeef, 0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};

  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e64.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg3e64.v v1, (%0)" ::"r"(BUFFER_O64));

  VVCMP_U64(14, BUFFER_O64, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0, 0x13572468369b48ac,
                            0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900, 0xabcdef1234567890,
                            0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
                            INIT              , INIT              , INIT              , INIT);
}

// Segment-4 for 64-bit, vl = 4 --> 128 bytes
void TEST_CASE4_64_vl4(void) {
  VSET(4, e64, m1);
  volatile uint64_t BUFFER_O64[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT}; 
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900,
      0xabcdef1234567890, 0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa,
      0x369b48acdeadbeef, 0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};

  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e64.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e64.v v1, (%0)" ::"r"(BUFFER_O64));

  VVCMP_U64(15, BUFFER_O64, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0, 0x13572468369b48ac,
                            0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900, 0xabcdef1234567890,
                            0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
                            0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1, 0x987654321fedcba0);
}

// Segment-8 for 64-bit, vl = 2 --> 128 bytes
void TEST_CASE8_64_vl2(void) {
  VSET(2, e64, m1);
  volatile uint64_t BUFFER_O64[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT}; 
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900,
      0xabcdef1234567890, 0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa,
      0x369b48acdeadbeef, 0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
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
  asm volatile("vsseg8e64.v v1, (%0)" ::"r"(BUFFER_O64));

  VVCMP_U64(16, BUFFER_O64, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0, 0x13572468369b48ac,
                            0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900, 0xabcdef1234567890,
                            0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
                            0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1, 0x987654321fedcba0);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE2_8_vl4();
  TEST_CASE3_8_vl4();
  TEST_CASE4_8_vl4();
  TEST_CASE8_8_vl2();

  TEST_CASE2_16_vl4();
  TEST_CASE3_16_vl4();
  TEST_CASE4_16_vl4();
  TEST_CASE8_16_vl2();

  TEST_CASE2_32_vl4();
  TEST_CASE3_32_vl4();
  TEST_CASE4_32_vl4();
  TEST_CASE8_32_vl2();

  TEST_CASE2_64_vl4();
  TEST_CASE3_64_vl4();
  TEST_CASE4_64_vl4();
  TEST_CASE8_64_vl2();

  EXIT_CHECK();
}