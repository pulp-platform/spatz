#include "vector_macros.h"


void TEST_CASE1(void) {
  VSET(16, e8, m1);
  volatile uint16_t ALIGNED_I16[1024];
  VLOAD_16(v0, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  asm volatile("vse16.v v0, (%0)" ::"r"(ALIGNED_I16));
  VVCMP_U16(1, ALIGNED_I16, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548,
            0x3489, 0x9388, 0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759,
            0x1111, 0x1989);
}

void TEST_CASE2(void) {
  volatile uint16_t ALIGNED_I16[16]={0};
  VSET(16, e16, m1);
  VLOAD_16(v3, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  VLOAD_8(v0, 0xFF, 0xFF);
  asm volatile("vse16.v v3, (%0), v0.t" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VVCMP_U16(2, ALIGNED_I16, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548,
            0x3489, 0x9388, 0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759,
            0x1111, 0x1989);
}

//*******Checking functionality of vse16 with different values of masking
// register******//
void TEST_CASE3(void) {
  volatile uint16_t ALIGNED_I16[16];
  VSET(16, e16, m1);
  VLOAD_16(v3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse16.v v3, (%0)" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VLOAD_16(v3, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  VLOAD_8(v0, 0x00, 0x00);
  asm volatile("vse16.v v3, (%0), v0.t" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VVCMP_U16(3, ALIGNED_I16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16);
}

void TEST_CASE4(void) {
  volatile uint16_t ALIGNED_I16[16];
  VSET(16, e16, m1);
  VLOAD_16(v3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse16.v v3, (%0)" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VLOAD_16(v3, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vse16.v v3, (%0), v0.t" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VVCMP_U16(4, ALIGNED_I16, 1, 0xbbd3, 3, 0x8cd1, 5, 0x7548, 7, 0x9388, 9,
            0x11ae, 11, 0x4891, 13, 0x8759, 15, 0x1989);
}

void TEST_CASE5(void) {
  volatile uint16_t ALIGNED_I16[16] = {0};
  VSET(16, e16, m1);
  VLOAD_16(v8, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  VSET(16, e8, m4);
  asm volatile("vse16.v v8, (%0)" ::"r"(ALIGNED_I16));
  VCLEAR(v8);
  VVCMP_U16(5, ALIGNED_I16, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548,
            0x3489, 0x9388, 0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759,
            0x1111, 0x1989);
}

void TEST_CASE6(void) {
  volatile uint16_t ALIGNED_I16[16] = {0};
  VSET(16, e16, m1);
  VLOAD_16(v6, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  asm volatile("vse16.v v6, (%0)" ::"r"(ALIGNED_I16));
  VCLEAR(v6);
  VVCMP_U16(6, ALIGNED_I16, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548,
            0x3489, 0x9388, 0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759,
            0x1111, 0x1989);
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