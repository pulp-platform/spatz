// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// vlm.v: mask load. Loads evl = ceil(vl/8) bytes into vd with EEW=8 and
// EMUL=1, regardless of SEW/LMUL. vstart counts bytes.
#include "vector_macros.h"

// vl not a multiple of 8: vl=13 -> 2 bytes
void TEST_CASE1(void) {
  volatile uint8_t INP[8] __attribute__((aligned(8))) = {0xA5, 0x3C, 0xFF, 0x81, 0x5A, 0x99, 0x42, 0x18};
  VSET(16, e8, m1);
  VCLEAR(v8);
  VSET(13, e8, m1);
  asm volatile("vlm.v v8, (%0)" ::"r"(INP) : "memory");
  VSET(2, e8, m1);
  VCMP_U8(1, v8, 0xA5, 0x3C);
}

// SEW/LMUL must not matter: e32,m8, vl=40 -> 5 bytes into v1 (EMUL=1).
// v2, which would be inside an LMUL=8 group, must stay untouched
void TEST_CASE2(void) {
  volatile uint8_t INP[8] __attribute__((aligned(8))) = {0xA5, 0x3C, 0xFF, 0x81, 0x5A, 0x99, 0x42, 0x18};
  VSET(16, e8, m1);
  VCLEAR(v1);
  asm volatile("vmv.v.i v2, -1");
  VSET(40, e32, m8);
  asm volatile("vlm.v v1, (%0)" ::"r"(INP) : "memory");
  VSET(5, e8, m1);
  VCMP_U8(2, v1, 0xA5, 0x3C, 0xFF, 0x81, 0x5A);
  VSET(16, e8, m1);
  VCMP_U8(3, v2, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255);
}

// vl=1 -> 1 byte
void TEST_CASE3(void) {
  volatile uint8_t INP[8] __attribute__((aligned(8))) = {0xA5, 0x3C, 0xFF, 0x81, 0x5A, 0x99, 0x42, 0x18};
  VSET(16, e8, m1);
  VCLEAR(v8);
  VSET(1, e8, m1);
  asm volatile("vlm.v v8, (%0)" ::"r"(INP) : "memory");
  VCMP_U8(4, v8, 0xA5);
}

// e16, vl=9 -> 2 bytes (evl depends on vl only, not on SEW)
void TEST_CASE4(void) {
  volatile uint8_t INP[8] __attribute__((aligned(8))) = {0xA5, 0x3C, 0xFF, 0x81, 0x5A, 0x99, 0x42, 0x18};
  VSET(16, e8, m1);
  VCLEAR(v8);
  VSET(9, e16, m1);
  asm volatile("vlm.v v8, (%0)" ::"r"(INP) : "memory");
  VSET(2, e8, m1);
  VCMP_U8(5, v8, 0xA5, 0x3C);
}

// RAW on v0: a masked op issued right after vlm.v v0 must use the new mask
// mask 0xB5, 0x6C -> active elements 0, 2, 4, 5, 7, 10, 11, 13, 14
void TEST_CASE5(void) {
  volatile uint8_t INP[2] __attribute__((aligned(8))) = {0xB5, 0x6C};
  VSET(16, e8, m1);
  VLOAD_8(v16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCLEAR(v0);
  VCLEAR(v8);
  asm volatile(
      "vlm.v   v0, (%0)\n\t"
      "vadd.vi v8, v16, 1, v0.t"
      ::"r"(INP) : "memory");
  VCMP_U8(6, v8, 1, 0, 3, 0, 5, 6, 0, 8, 0, 0, 11, 12, 0, 14, 15, 0);
}

// vstart counts bytes: vstart=1, vl=24 -> byte 0 untouched, bytes 1..2 loaded
void TEST_CASE6(void) {
  volatile uint8_t INP[8] __attribute__((aligned(8))) = {0xA5, 0x3C, 0xFF, 0x81, 0x5A, 0x99, 0x42, 0x18};
  VSET(24, e8, m1);
  asm volatile("vmv.v.i v8, 7");
  asm volatile("csrw vstart, 1");
  asm volatile("vlm.v v8, (%0)" ::"r"(INP) : "memory");
  asm volatile("csrw vstart, 0");
  VSET(3, e8, m1);
  VCMP_U8(7, v8, 7, 0x3C, 0xFF);
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
