// Author: CMY

#include "vector_macros.h"

void TEST_CASE0(void) { // test vm signal
  VSET(16, e16, m1);
  VLOAD_16(v1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_8(v0, 0xAA, 0xAA);
  volatile uint16_t INP1[] = {0xaabb,0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567,
                             0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567, 0x0123};
  asm volatile("vle16.v v1, (%0),v0.t" ::"r"(INP1));
  VCMP_U16(0, v1, 0x1,0x0123, 0x3, 0x89ab, 0x5, 0xcdef, 0x7, 0x4567,
                             0x9, 0x4567, 11, 0xcdef, 13, 0x89ab, 15, 0x0123);
}

void TEST_CASE1(void) {
  VSET(16, e16, m1);
  volatile uint16_t INP1[] = {0xaabb,0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567,
                             0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567, 0x0123};
  asm volatile("vle16.v v1, (%0)" ::"r"(INP1));
  VCMP_U16(1, v1, 0xaabb,0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567,
                             0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567, 0x0123);
}

// Positive-stride tests
void TEST_CASE2(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567, 0x0123,
                             0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567, 0x0123};
  uint64_t stride = 6; // stride unit is BYTE
  asm volatile("vlse16.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U16(2, v1, 0x0123, 0xcdef, 0x4567, 0x4567);
}

void TEST_CASE3(void) {
  VSET(16, e16, m1); // SET the VLEN to 16 to use the 4 memory ports
  volatile uint16_t INP1[] = {0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567, 0x0123,
                             0x0123, 0x4567, 0x89ab, 0xcdef, 0xcdef, 0x89ab, 0x4567, 0x0123};
  uint64_t stride = 6; // stride unit is BYTE
  asm volatile("vlse16.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U16(3, v1, 0x0123, 0xcdef, 0x4567, 0x4567,0xcdef,0x0123);
}

void TEST_CASE4(void) {
  VSET(4, e16, m1);
  volatile uint16_t INP1[] = {0x9fe4, 0x1920, 0x8f2e, 0x05e0,
                              0xf9aa, 0x71f0, 0xc394, 0xbbd3};
  uint64_t stride = 4;
  VLOAD_8(v0, 0xAA);
  VCLEAR(v1);
  asm volatile("vlse16.v v1, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U16(4, v1, 0, 0x8f2e, 0, 0xc394);
}


int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE0();
  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();

  EXIT_CHECK();
}