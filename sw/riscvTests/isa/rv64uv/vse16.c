// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "vector_macros.h"


void TEST_CASE1(void) {
  VSET(16, e16, m1);
  volatile uint16_t ALIGNED_I16[16];
  VLOAD_16(v0, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  asm volatile("vse16.v v0, (%0)" ::"r"(ALIGNED_I16));
  VVCMP_U16(1, ALIGNED_I16, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548,
            0x3489, 0x9388, 0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759,
            0x1111, 0x1989);
}

void TEST_CASE2(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xFF, 0xFF);

  volatile uint16_t ALIGNED_I16[16]={0};
  VSET(16, e16, m1);
  VLOAD_16(v3, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  asm volatile("vse16.v v3, (%0), v0.t" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VVCMP_U16(2, ALIGNED_I16, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548,
            0x3489, 0x9388, 0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759,
            0x1111, 0x1989);
}

//*******Checking functionality of vse16 with different values of masking
// register******//
void TEST_CASE3(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0x00, 0x00);

  volatile uint16_t ALIGNED_I16[16];
  VSET(16, e16, m1);
  VLOAD_16(v3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse16.v v3, (%0)" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VLOAD_16(v3, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
  asm volatile("vse16.v v3, (%0), v0.t" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VVCMP_U16(3, ALIGNED_I16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16);
}

void TEST_CASE4(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xAA, 0xAA);

  volatile uint16_t ALIGNED_I16[16];
  VSET(16, e16, m1);
  VLOAD_16(v3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse16.v v3, (%0)" ::"r"(ALIGNED_I16));
  VCLEAR(v3);
  VLOAD_16(v3, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
           0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989);
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

void TEST_CASE7(void) {
  VSET(4, e16, m1);
  VCLEAR(v0);
  VLOAD_16(v0, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA);

  VSET(64, e16, m8);
  volatile uint16_t ALIGNED_I16[64];

  //init mem
  for (int i = 0; i < 64; i++) ALIGNED_I16[i] = 0xFFFF;

  VLOAD_16(v8,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
               17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
               33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
               49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);

  asm volatile("vse16.v v8, (%0), v0.t" ::"r"(ALIGNED_I16));

  VVCMP_U16(7, ALIGNED_I16,
    0xFFFF,  2, 0xFFFF,  4, 0xFFFF,  6, 0xFFFF,  8, 0xFFFF, 10, 0xFFFF, 12, 0xFFFF, 14, 0xFFFF, 16,
    0xFFFF, 18, 0xFFFF, 20, 0xFFFF, 22, 0xFFFF, 24, 0xFFFF, 26, 0xFFFF, 28, 0xFFFF, 30, 0xFFFF, 32,
    0xFFFF, 34, 0xFFFF, 36, 0xFFFF, 38, 0xFFFF, 40, 0xFFFF, 42, 0xFFFF, 44, 0xFFFF, 46, 0xFFFF, 48,
    0xFFFF, 50, 0xFFFF, 52, 0xFFFF, 54, 0xFFFF, 56, 0xFFFF, 58, 0xFFFF, 60, 0xFFFF, 62, 0xFFFF, 64);
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
  TEST_CASE7();

  EXIT_CHECK();
}