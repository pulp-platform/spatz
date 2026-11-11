// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// vsm.v: mask store. Stores evl = ceil(vl/8) bytes of vs3 with EEW=8 and
// EMUL=1, regardless of SEW/LMUL. vstart counts bytes.
// OUT is prefilled with 170 to catch writes past evl.
#include "vector_macros.h"

// vl not a multiple of 8: vl=13 -> 2 bytes
void TEST_CASE1(void) {
  volatile uint8_t OUT[4] __attribute__((aligned(8)));
  for (int i = 0; i < 4; i++) OUT[i] = 170;
  VSET(4, e8, m1);
  VLOAD_8(v8, 0xA5, 0x3C, 0xFF, 0x81);
  VSET(13, e8, m1);
  asm volatile("vsm.v v8, (%0)" ::"r"(OUT) : "memory");
  VVCMP_U8(1, OUT, 0xA5, 0x3C, 170, 170);
}

// SEW/LMUL must not matter: e32,m8, vl=40 -> 5 bytes from v1 (EMUL=1)
void TEST_CASE2(void) {
  volatile uint8_t OUT[8] __attribute__((aligned(8)));
  for (int i = 0; i < 8; i++) OUT[i] = 170;
  VSET(8, e8, m1);
  VLOAD_8(v1, 0xA5, 0x3C, 0xFF, 0x81, 0x5A, 0x99, 0x42, 0x18);
  VSET(40, e32, m8);
  asm volatile("vsm.v v1, (%0)" ::"r"(OUT) : "memory");
  VVCMP_U8(2, OUT, 0xA5, 0x3C, 0xFF, 0x81, 0x5A, 170, 170, 170);
}

// vl=1 -> 1 byte
void TEST_CASE3(void) {
  volatile uint8_t OUT[4] __attribute__((aligned(8)));
  for (int i = 0; i < 4; i++) OUT[i] = 170;
  VSET(4, e8, m1);
  VLOAD_8(v8, 0xA5, 0x3C, 0xFF, 0x81);
  VSET(1, e8, m1);
  asm volatile("vsm.v v8, (%0)" ::"r"(OUT) : "memory");
  VVCMP_U8(3, OUT, 0xA5, 170, 170, 170);
}

// RAW: store the mask produced by the instruction right before it
// v8 = 0..15, v1 = (v8 > 9) -> elements 10..15 -> bytes 0x00, 0xFC
void TEST_CASE4(void) {
  volatile uint8_t OUT[4] __attribute__((aligned(8)));
  for (int i = 0; i < 4; i++) OUT[i] = 170;
  VSET(16, e8, m1);
  VLOAD_8(v8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCLEAR(v1);
  asm volatile(
      "vmsgtu.vi v1, v8, 9\n\t"
      "vsm.v     v1, (%0)"
      ::"r"(OUT) : "memory");
  VVCMP_U8(4, OUT, 0x00, 0xFC, 170, 170);
}

// WAR: vs3 overwritten right after the store is issued
void TEST_CASE5(void) {
  volatile uint8_t OUT[4] __attribute__((aligned(8)));
  for (int i = 0; i < 4; i++) OUT[i] = 170;
  VSET(2, e8, m1);
  VLOAD_8(v1, 0x61, 0x62);
  VSET(16, e8, m1);
  asm volatile(
      "vsm.v   v1, (%0)\n\t"
      "vmv.v.i v1, 0"
      ::"r"(OUT) : "memory");
  VVCMP_U8(5, OUT, 0x61, 0x62, 170, 170);
}

// vstart counts bytes: vstart=1, vl=24 -> byte 0 untouched, bytes 1..2 stored
void TEST_CASE6(void) {
  volatile uint8_t OUT[4] __attribute__((aligned(8)));
  for (int i = 0; i < 4; i++) OUT[i] = 170;
  VSET(4, e8, m1);
  VLOAD_8(v8, 0xA5, 0x3C, 0xFF, 0x81);
  VSET(24, e8, m1);
  asm volatile("csrw vstart, 1");
  asm volatile("vsm.v v8, (%0)" ::"r"(OUT) : "memory");
  asm volatile("csrw vstart, 0");
  VVCMP_U8(6, OUT, 170, 0x3C, 0xFF, 170);
}

// Round trip vlm -> vsm with an odd vl at e16: vl=21 -> 3 bytes
void TEST_CASE7(void) {
  volatile uint8_t INP[4] __attribute__((aligned(8))) = {0xC3, 0x5E, 0x0F, 0x77};
  volatile uint8_t OUT[4] __attribute__((aligned(8)));
  for (int i = 0; i < 4; i++) OUT[i] = 170;
  VSET(21, e16, m1);
  VCLEAR(v8);
  asm volatile(
      "vlm.v v8, (%0)\n\t"
      "vsm.v v8, (%1)"
      ::"r"(INP), "r"(OUT) : "memory");
  VVCMP_U8(7, OUT, 0xC3, 0x5E, 0x0F, 170);
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
