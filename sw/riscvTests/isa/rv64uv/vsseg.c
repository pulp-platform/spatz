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
#if ELEN == 64
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
#endif
}

// Segment-3 for 64-bit, vl = 4 --> 96 bytes
void TEST_CASE3_64_vl4(void) {
#if ELEN == 64
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
#endif
}

// Segment-4 for 64-bit, vl = 4 --> 128 bytes
void TEST_CASE4_64_vl4(void) {
#if ELEN == 64
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
#endif
}

// Segment-8 for 64-bit, vl = 2 --> 128 bytes
void TEST_CASE8_64_vl2(void) {
#if ELEN == 64
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
#endif
}

///////////////////////////////////
// Masked segment stores (v0.t) //
//////////////////////////////////

// Segment-2 for 8-bit, vl = 4, mask 0b1010
void TEST_CASE2_8_vl4_m(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[]      = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                                  0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};

  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e8.v v1, (%0), v0.t" ::"r"(BUFFER_O8));

  VVCMP_U8(17, BUFFER_O8, INIT, INIT, 0x19, 0x20, INIT, INIT, 0x05, 0xe0,
                          INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT);
}

// Segment-3 for 8-bit, vl = 4, mask 0b0101
void TEST_CASE3_8_vl4_m(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[]      = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                                  0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};

  VLOAD_8(v0, 0x05, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg3e8.v v1, (%0), v0.t" ::"r"(BUFFER_O8));

  VVCMP_U8(18, BUFFER_O8, 0x9f, 0xe4, 0x19, INIT, INIT, INIT, 0x05, 0xe0,
                          0xf9, INIT, INIT, INIT, INIT, INIT, INIT, INIT);
}

// Segment-4 for 8-bit, vl = 4, mask 0b1001
void TEST_CASE4_8_vl4_m(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[]      = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                                  0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};

  VLOAD_8(v0, 0x09, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e8.v v1, (%0), v0.t" ::"r"(BUFFER_O8));

  VVCMP_U8(19, BUFFER_O8, 0x9f, 0xe4, 0x19, 0x20, INIT, INIT, INIT, INIT,
                          INIT, INIT, INIT, INIT, 0xc3, 0x94, 0xbb, 0xd3);
}

// Segment-8 for 8-bit, vl = 2, mask 0b01
void TEST_CASE8_8_vl2_m(void) {
  VSET(2, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[]      = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
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
  asm volatile("vlseg8e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg8e8.v v1, (%0), v0.t" ::"r"(BUFFER_O8));

  VVCMP_U8(20, BUFFER_O8, 0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                          INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT);
}

// Segment-2 for 16-bit, vl = 4, mask 0b0110
void TEST_CASE2_16_vl4_m(void) {
  VSET(4, e16, m1);

  volatile uint16_t BUFFER_O16[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};

  VLOAD_8(v0, 0x06, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e16.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e16.v v1, (%0), v0.t" ::"r"(BUFFER_O16));

  VVCMP_U16(21, BUFFER_O16, INIT  , INIT  , 0x8f2e, 0x05e0, 0xf9aa, 0x71f0, INIT  , INIT  ,
                            INIT  , INIT  , INIT  , INIT  , INIT  , INIT  , INIT  , INIT  );
}

// Segment-3 for 16-bit, vl = 4, mask 0b1010
void TEST_CASE3_16_vl4_m(void) {
  VSET(4, e16, m1);

  volatile uint16_t BUFFER_O16[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};

  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e16.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg3e16.v v1, (%0), v0.t" ::"r"(BUFFER_O16));

  VVCMP_U16(22, BUFFER_O16, INIT  , INIT  , INIT  , 0x05e0, 0xf9aa, 0x71f0, INIT  , INIT  ,
                            INIT  , 0x5678, 0x9abc, 0xdef0, INIT  , INIT  , INIT  , INIT  );
}

// Segment-4 for 16-bit, vl = 4, mask 0b0101
void TEST_CASE4_16_vl4_m(void) {
  VSET(4, e16, m1);

  volatile uint16_t BUFFER_O16[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0,
                              0xc394, 0xbbd3, 0x1234, 0x5678, 0x9abc, 0xdef0,
                              0x1357, 0x2468, 0x369b, 0x48ac};

  VLOAD_8(v0, 0x05, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e16.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e16.v v1, (%0), v0.t" ::"r"(BUFFER_O16));

  VVCMP_U16(23, BUFFER_O16, 0x9fe4, 0x1920, 0x8f2e, 0x05e0, INIT  , INIT  , INIT  , INIT  ,
                            0x1234, 0x5678, 0x9abc, 0xdef0, INIT  , INIT  , INIT  , INIT  );
}

// Segment-8 for 16-bit, vl = 2, mask 0b10
void TEST_CASE8_16_vl2_m(void) {
  VSET(2, e16, m1);

  volatile uint16_t BUFFER_O16[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

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
  asm volatile("vlseg8e16.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg8e16.v v1, (%0), v0.t" ::"r"(BUFFER_O16));

  VVCMP_U16(24, BUFFER_O16, INIT  , INIT  , INIT  , INIT  , INIT  , INIT  , INIT  , INIT  ,
                            0x1234, 0x5678, 0x9abc, 0xdef0, 0x1357, 0x2468, 0x369b, 0x48ac);
}

// Segment-2 for 32-bit, vl = 4, mask 0b1100
void TEST_CASE2_32_vl4_m(void) {
  VSET(4, e32, m1);

  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};

  VLOAD_8(v0, 0x0C, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e32.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e32.v v1, (%0), v0.t" ::"r"(BUFFER_O32));

  VVCMP_U32(25, BUFFER_O32, INIT      , INIT      , INIT      , INIT      ,
                            0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                            INIT      , INIT      , INIT      , INIT      ,
                            INIT      , INIT      , INIT      , INIT      );
}

// Segment-3 for 32-bit, vl = 4, mask 0b0011
void TEST_CASE3_32_vl4_m(void) {
  VSET(4, e32, m1);

  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};

  VLOAD_8(v0, 0x03, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e32.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg3e32.v v1, (%0), v0.t" ::"r"(BUFFER_O32));

  VVCMP_U32(26, BUFFER_O32, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                            0x12345678, 0x9abcdef0, INIT      , INIT      ,
                            INIT      , INIT      , INIT      , INIT      ,
                            INIT      , INIT      , INIT      , INIT      );
}

// Segment-4 for 32-bit, vl = 4, mask 0b1010
void TEST_CASE4_32_vl4_m(void) {
  VSET(4, e32, m1);

  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};

  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e32.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e32.v v1, (%0), v0.t" ::"r"(BUFFER_O32));

  VVCMP_U32(27, BUFFER_O32, INIT      , INIT      , INIT      , INIT      ,
                            0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                            INIT      , INIT      , INIT      , INIT      ,
                            0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890);
}

// Segment-8 for 32-bit, vl = 2, mask 0b10
void TEST_CASE8_32_vl2_m(void) {
  VSET(2, e32, m1);

  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

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
  asm volatile("vlseg8e32.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg8e32.v v1, (%0), v0.t" ::"r"(BUFFER_O32));

  VVCMP_U32(28, BUFFER_O32, INIT      , INIT      , INIT      , INIT      ,
                            INIT      , INIT      , INIT      , INIT      ,
                            0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                            0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890);
}

// Segment-2 for 64-bit, vl = 4, mask 0b0101
void TEST_CASE2_64_vl4_m(void) {
#if ELEN == 64
  VSET(4, e64, m1);

  volatile uint64_t BUFFER_O64[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900,
      0xabcdef1234567890, 0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa,
      0x369b48acdeadbeef, 0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};

  VLOAD_8(v0, 0x05, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e64.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e64.v v1, (%0), v0.t" ::"r"(BUFFER_O64));

  VVCMP_U64(29, BUFFER_O64, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, INIT              , INIT              ,
                            0xdeadbeefcafebabe, 0x0123456789abcdef, INIT              , INIT              ,
                            INIT              , INIT              , INIT              , INIT              ,
                            INIT              , INIT              , INIT              , INIT              );
#endif
}

// Segment-3 for 64-bit, vl = 4, mask 0b1010
void TEST_CASE3_64_vl4_m(void) {
#if ELEN == 64
  VSET(4, e64, m1);

  volatile uint64_t BUFFER_O64[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900,
      0xabcdef1234567890, 0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa,
      0x369b48acdeadbeef, 0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};

  VLOAD_8(v0, 0x0A, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  asm volatile("vlseg3e64.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg3e64.v v1, (%0), v0.t" ::"r"(BUFFER_O64));

  VVCMP_U64(30, BUFFER_O64, INIT              , INIT              , INIT              , 0x13572468369b48ac,
                            0xdeadbeefcafebabe, 0x0123456789abcdef, INIT              , INIT              ,
                            INIT              , 0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
                            INIT              , INIT              , INIT              , INIT              );
#endif
}

// Segment-4 for 64-bit, vl = 4, mask 0b0110
void TEST_CASE4_64_vl4_m(void) {
#if ELEN == 64
  VSET(4, e64, m1);

  volatile uint64_t BUFFER_O64[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900,
      0xabcdef1234567890, 0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa,
      0x369b48acdeadbeef, 0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
      0x987654321fedcba0};

  VLOAD_8(v0, 0x06, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e64.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e64.v v1, (%0), v0.t" ::"r"(BUFFER_O64));

  VVCMP_U64(31, BUFFER_O64, INIT              , INIT              , INIT              , INIT              ,
                            0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900, 0xabcdef1234567890,
                            0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa, 0x369b48acdeadbeef,
                            INIT              , INIT              , INIT              , INIT              );
#endif
}

// Segment-8 for 64-bit, vl = 2, mask 0b01
void TEST_CASE8_64_vl2_m(void) {
#if ELEN == 64
  VSET(2, e64, m1);

  volatile uint64_t BUFFER_O64[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0,
      0x13572468369b48ac, 0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900,
      0xabcdef1234567890, 0xfeedfacecafebabe, 0x123456789abcdef0, 0x1357246855aa55aa,
      0x369b48acdeadbeef, 0xcafebabe12345678, 0xabcdef0987654321, 0x012345670abcdef1,
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
  asm volatile("vlseg8e64.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg8e64.v v1, (%0), v0.t" ::"r"(BUFFER_O64));

  VVCMP_U64(32, BUFFER_O64, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0x123456789abcdef0, 0x13572468369b48ac,
                            0xdeadbeefcafebabe, 0x0123456789abcdef, 0x55aa55aa77889900, 0xabcdef1234567890,
                            INIT              , INIT              , INIT              , INIT              ,
                            INIT              , INIT              , INIT              , INIT              );
#endif
}

//////////////////
// Corner cases //
//////////////////


// All-zeros mask: the store must not touch the buffer at all
// Segment-4 for 8-bit, vl = 4, mask 0b0000
void TEST_CASE4_8_vl4_m_none(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[]      = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                                  0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};

  VLOAD_8(v0, 0x00, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e8.v v1, (%0), v0.t" ::"r"(BUFFER_O8));

  VVCMP_U8(33, BUFFER_O8, INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                          INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT);
}

// All-ones mask: must match the unmasked TEST_CASE4_8_vl4 exactly
// Segment-4 for 8-bit, vl = 4, mask 0b1111
void TEST_CASE4_8_vl4_m_all(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[]      = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                                  0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};

  VLOAD_8(v0, 0x0F, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  VCLEAR(v3);
  VCLEAR(v4);
  asm volatile("vlseg4e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg4e8.v v1, (%0), v0.t" ::"r"(BUFFER_O8));

  VVCMP_U8(34, BUFFER_O8, 0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                          0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3);
}

// Single active segment, last one: stresses the strobe offset
// Segment-2 for 8-bit, vl = 4, mask 0b1000
void TEST_CASE2_8_vl4_m_last(void) {
  VSET(4, e8, m1);

  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint8_t INP1[]      = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                                  0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};

  VLOAD_8(v0, 0x08, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e8.v v1, (%0), v0.t" ::"r"(BUFFER_O8));

  VVCMP_U8(35, BUFFER_O8, INIT, INIT, INIT, INIT, INIT, INIT, 0x05, 0xe0,
                          INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT);
}

// Same on e32: only bytes 24..31 of the buffer may change
// Segment-2 for 32-bit, vl = 4, mask 0b1000
void TEST_CASE2_32_vl4_m_last(void) {
  VSET(4, e32, m1);

  volatile uint32_t BUFFER_O32[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                  INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};

  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0x12345678, 0x9abcdef0, 0x13572468, 0x369b48ac,
                              0xdeadbeef, 0xcafebabe, 0x01234567, 0x89abcdef,
                              0x55aa55aa, 0x77889900, 0xabcdef12, 0x34567890};

  VLOAD_8(v0, 0x08, 0x00, 0x00, 0x00);
  VCLEAR(v1);
  VCLEAR(v2);
  asm volatile("vlseg2e32.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e32.v v1, (%0), v0.t" ::"r"(BUFFER_O32));

  VVCMP_U32(36, BUFFER_O32, INIT      , INIT      , INIT      , INIT      ,
                            INIT      , INIT      , 0x13572468, 0x369b48ac,
                            INIT      , INIT      , INIT      , INIT      ,
                            INIT      , INIT      , INIT      , INIT      );
}

// Segment-2 for 8-bit, vl = 32 --> 64 bytes
void TEST_CASE2_8_vl32(void) {
  VSET(32, e8, m1);
 
  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
 
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
  asm volatile("vsseg2e8.v v1, (%0)" ::"r"(BUFFER_O8));
 
  VVCMP_U8(37, BUFFER_O8, 0x31, 0xda, 0x32, 0x6f, 0xb5, 0x1a, 0xe6, 0xc7,
                          0xc1, 0x73, 0x67, 0x1e, 0xd0, 0xab, 0xbb, 0x46,
                          0x74, 0x2b, 0xa7, 0xf6, 0x50, 0xc3, 0xab, 0xee,
                          0x64, 0x73, 0xd7, 0xbe, 0x00, 0x6b, 0xab, 0xc6,
                          0x04, 0x4b, 0xb7, 0x36, 0x40, 0x43, 0x7b, 0x0e,
                          0x74, 0xb3, 0xc7, 0xde, 0xf0, 0x6b, 0x5b, 0xc6,
                          0x54, 0x2b, 0x07, 0xf6, 0x30, 0x03, 0x8b, 0xee,
                          0x04, 0xb3, 0xb7, 0xbe, 0xa0, 0xab, 0x4b, 0x46);
}

// Segment-2 for 8-bit, vl = 32 --> 64 bytes, mask 0xC3A5F00F
void TEST_CASE2_8_vl32_m(void) {
  VSET(32, e8, m1);
 
  volatile uint8_t BUFFER_O8[] = {INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                                 INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
 
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
  asm volatile("vlseg2e8.v v1, (%0)" ::"r"(INP1));
  asm volatile("vsseg2e8.v v1, (%0), v0.t" ::"r"(BUFFER_O8));
 
  VVCMP_U8(38, BUFFER_O8, 0x31, 0xda, 0x32, 0x6f, 0xb5, 0x1a, 0xe6, 0xc7,
                          INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                          INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
                          0x64, 0x73, 0xd7, 0xbe, 0x00, 0x6b, 0xab, 0xc6,
                          0x04, 0x4b, INIT, INIT, 0x40, 0x43, INIT, INIT,
                          INIT, INIT, 0xc7, 0xde, INIT, INIT, 0x5b, 0xc6,
                          0x54, 0x2b, 0x07, 0xf6, INIT, INIT, INIT, INIT,
                          INIT, INIT, INIT, INIT, 0xa0, 0xab, 0x4b, 0x46);
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

  // Masked (v0.t)
  TEST_CASE2_8_vl4_m();
  TEST_CASE3_8_vl4_m();
  TEST_CASE4_8_vl4_m();
  TEST_CASE8_8_vl2_m();

  TEST_CASE2_16_vl4_m();
  TEST_CASE3_16_vl4_m();
  TEST_CASE4_16_vl4_m();
  TEST_CASE8_16_vl2_m();

  TEST_CASE2_32_vl4_m();
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
  TEST_CASE2_8_vl4_m_last();
  TEST_CASE2_32_vl4_m_last();
  TEST_CASE2_8_vl32();
  TEST_CASE2_8_vl32_m();

  EXIT_CHECK();
}