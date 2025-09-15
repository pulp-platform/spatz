// Author: CMY

#include "vector_macros.h"


void TEST_CASE1(void) {
  VSET(16, e32, m1);
  volatile uint32_t ALIGNED_I32[16] = {
        0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598,
        0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee, 0x90139301, 0xab8b9148,
        0x90318509, 0x31897598, 0x83195999, 0x89139848};
  asm volatile("vle32.v v0, (%0)" ::"r"(ALIGNED_I32));
  VCMP_U32(1, v0, 0x9fe41920,0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598,
           0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee, 0x90139301,
           0xab8b9148, 0x90318509, 0x31897598, 0x83195999, 0x89139848);
}

// Positive-stride tests
void TEST_CASE2(void) {
  VSET(32, e32, m8);
  volatile uint32_t INP1[] = {1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        0xa, 0xb, 0xc,
        0xd, 0xe, 0xf,
        0x10,
    0x10,0xf,0xe,0xd,0xc,0xb,0xa,9,8,7,6,5,4,3,2,1};
  VCLEAR(v2);
  asm volatile("vle32.v v2, (%0)" ::"r"(INP1));
  VCMP_U32(32, v2, 1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        0xa, 0xb, 0xc,
        0xd, 0xe, 0xf,
        0x10,
    0x10,0xf,0xe,0xd,0xc,0xb,0xa,9,8,7,6,5,4,3,2,1);
}

void TEST_CASE3(void) {
  VSET(4, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1};
  uint64_t stride = 8;
  VLOAD_8(v0, 0xAA);
  VCLEAR(v1);
  asm volatile("vlse32.v v1, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U32(3, v1, 0, 0xf9aa71f0, 0, 0x99991348);
}

void TEST_CASE10(void) {
  VSET(8, e32, m1);
  volatile uint32_t INP1[] = {0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1};
  VCLEAR(v1);
  asm volatile("vle32.v v1, (%0)" ::"r"(INP1));
  VCMP_U32(10, v1, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                              0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1);
}

void TEST_CASE11(void) {
  VSET(16, e32, m2);
  volatile uint32_t INP1[] = {0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598,
        0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee, 0x90139301, 0xab8b9148,
        0x90318509, 0x31897598, 0x83195999, 0x89139848};
  VCLEAR(v1);
  asm volatile("vle32.v v1, (%0)" ::"r"(INP1));
  VCMP_U32(11, v1, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598,
        0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee, 0x90139301, 0xab8b9148,
        0x90318509, 0x31897598, 0x83195999, 0x89139848);
}

void TEST_CASE12(void) {
  VSET(16, e64, m8);
  volatile uint64_t INP1[] = {0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
        0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548,
        0x1893179501093489, 0x81937598aa819388, 0x1874754791888188,
        0x3eeeeeeee33111ae, 0x9013930148815808, 0xab8b914891484891,
        0x9031850931584902, 0x3189759837598759, 0x8319599991911111,
        0x8913984898951989};
  // VCLEAR(v8);
  VLOAD_8(v0, 0xFF, 0xFF);
  VCLEAR(v8);
  asm volatile("vle64.v v8, (%0), v0.t" ::"r"(INP1));
  VCMP_U64(12, v8, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
           0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548,
           0x1893179501093489, 0x81937598aa819388, 0x1874754791888188,
           0x3eeeeeeee33111ae, 0x9013930148815808, 0xab8b914891484891,
           0x9031850931584902, 0x3189759837598759, 0x8319599991911111,
           0x8913984898951989);
}

void TEST_CASE4(void) {
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
  VCLEAR(v8);
  asm volatile("vlse64.v v8, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U64(4, v8, 0x9fe419208f2e05e0, 0xa11a9384a7163840, 0x9fa831c7a11a9384, 0x1893179501093489, 0x1874754791888188,
           0x9013930148815808, 0x9031850931584902, 0x8319599991911111);
}

void TEST_CASE5(void) {
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
  VCLEAR(v8);
  asm volatile("vlse64.v v8, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U64(5, v8, 0, 0xa11a9384a7163840, 0, 0x1893179501093489, 0,
           0x9013930148815808, 0,0x8319599991911111);
}

void TEST_CASE6(void) {
  VSET(4, e64, m1);
  volatile uint64_t INP1[] = {0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3,
                              0xa11a9384a7163840, 0x99991348a9f38cd1};
  uint64_t stride = 8;
  asm volatile("vlse64.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U64(6, v1, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
           0x99991348a9f38cd1);
}

void TEST_CASE7(void) {
  VSET(2, e64, m1);
  volatile uint64_t INP1[] = {0x99991348a9f38cd1, 0x9fa831c7a11a9384,
                              0x9fa831c7a11a9384, 0x9fa831c7a11a9384,
                              0x9fa831c7a11a9384, 0x01015ac1309bb678};
  uint64_t stride = 40;
  asm volatile("vlse64.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U64(9, v1, 0x99991348a9f38cd1, 0x01015ac1309bb678);
}

void TEST_CASE8(void) {
  VSET(8, e64, m2);
  volatile uint64_t INP1[] = {
      0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
      0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548,
      0x1893179501093489, 0x81937598aa819388, 0x1874754791888188,
      0x3eeeeeeee33111ae, 0x9013930148815808, 0xab8b914891484891,
      0x9031850931584902, 0x3189759837598759, 0x8319599991911111,
      0x8913984898951989};
  uint64_t stride = 16;
  VLOAD_8(v0, 0xAB);
  VCLEAR(v8);
  asm volatile("vlse64.v v8, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U64(8, v8, 0x9fe419208f2e05e0, 0xa11a9384a7163840, 0, 0x1893179501093489, 0,
           0x9013930148815808, 0,0x8319599991911111);
}


int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
//   TEST_CASE10();
//   TEST_CASE11();
//   TEST_CASE12();
//   // TEST_CASE4();
//   TEST_CASE5();
//   // TEST_CASE6();
//   // TEST_CASE7();
//   TEST_CASE8();

  EXIT_CHECK();
}