// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// vsoxseg<nf>ei<eew>.v: ordered indexed segment stores.
// mem[rs1 + vs2[i] + f*SEW/8] = (vs3 + f*LMUL)[i]
// Field f holds 10*f + i + 1. OUT is prefilled with 170 to catch stray writes.
// Unless stated otherwise the indices are a permutation of whole segment
// slots, so segments never overlap and the result is order-independent.
#include "vector_macros.h"

// nf=2, ei8, SEW=8
void TEST_CASE1(void) {
  volatile uint8_t OUT[10] __attribute__((aligned(8)));
  for (int i = 0; i < 10; i++) OUT[i] = 170;
  VSET(4, e8, m1);
  VLOAD_8(v24, 0, 6, 4, 2);
  VLOAD_8(v8, 1, 2, 3, 4);
  VLOAD_8(v9, 11, 12, 13, 14);
  asm volatile("vsoxseg2ei8.v v8, (%0), v24" ::"r"(OUT) : "memory");
  VVCMP_U8(1, OUT, 1, 11, 4, 14, 3, 13, 2, 12, 170, 170);
}

// nf=3, ei16 with SEW=8 (index EMUL=2)
void TEST_CASE2(void) {
  volatile uint8_t OUT[14] __attribute__((aligned(8)));
  for (int i = 0; i < 14; i++) OUT[i] = 170;
  VSET(4, e8, m1);
  VLOAD_16(v24, 0, 9, 6, 3);
  VLOAD_8(v8, 1, 2, 3, 4);
  VLOAD_8(v9, 11, 12, 13, 14);
  VLOAD_8(v10, 21, 22, 23, 24);
  asm volatile("vsoxseg3ei16.v v8, (%0), v24" ::"r"(OUT) : "memory");
  VVCMP_U8(2, OUT, 1, 11, 21, 4, 14, 24, 3, 13, 23, 2, 12, 22, 170, 170);
}

// nf=2, ei8 with SEW=32, LMUL=2 (index EMUL=1/2, sources v8-v9 and v10-v11)
void TEST_CASE3(void) {
  volatile uint32_t OUT[10] __attribute__((aligned(8)));
  for (int i = 0; i < 10; i++) OUT[i] = 170;
  VSET(4, e32, m2);
  VLOAD_8(v24, 0, 24, 16, 8);
  VLOAD_32(v8, 1, 2, 3, 4);
  VLOAD_32(v10, 11, 12, 13, 14);
  asm volatile("vsoxseg2ei8.v v8, (%0), v24" ::"r"(OUT) : "memory");
  VVCMP_U32(3, OUT, 1, 11, 4, 14, 3, 13, 2, 12, 170, 170);
}

// Index vector spanning several VRF words (64 B of ei32 indices): every
// field pass must restart reading the index vector from its first word
void TEST_CASE4(void) {
  volatile uint32_t OUT[34] __attribute__((aligned(8)));
  for (int i = 0; i < 34; i++) OUT[i] = 170;
  VSET(16, e32, m2);
  VLOAD_32(v24, 0, 56, 112, 40, 96, 24, 80, 8, 64, 120, 48, 104, 32, 88, 16, 72);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VLOAD_32(v10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26);
  asm volatile("vsoxseg2ei32.v v8, (%0), v24" ::"r"(OUT) : "memory");
  VVCMP_U32(4, OUT, 1, 11, 8, 18, 15, 25, 6, 16, 13, 23, 4, 14, 11, 21, 2, 12, 9, 19, 16, 26, 7, 17, 14, 24, 5, 15, 12, 22, 3, 13, 10, 20, 170, 170);
}

// Same with 64 B of ei16 indices, nf=2, LMUL=2
void TEST_CASE5(void) {
  volatile uint16_t OUT[66] __attribute__((aligned(8)));
  for (int i = 0; i < 66; i++) OUT[i] = 170;
  VSET(32, e16, m2);
  VLOAD_16(v24, 0, 20, 40, 60, 80, 100, 120, 12, 32, 52, 72, 92, 112, 4, 24, 44, 64, 84, 104, 124, 16, 36, 56, 76, 96, 116, 8, 28, 48, 68, 88, 108);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32);
  VLOAD_16(v10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42);
  asm volatile("vsoxseg2ei16.v v8, (%0), v24" ::"r"(OUT) : "memory");
  VVCMP_U16(5, OUT, 1, 11, 14, 24, 27, 37, 8, 18, 21, 31, 2, 12, 15, 25, 28, 38, 9, 19, 22, 32, 3, 13, 16, 26, 29, 39, 10, 20, 23, 33, 4, 14, 17, 27, 30, 40, 11, 21, 24, 34, 5, 15, 18, 28, 31, 41, 12, 22, 25, 35, 6, 16, 19, 29, 32, 42, 13, 23, 26, 36, 7, 17, 20, 30, 170, 170);
}

// nf=8 (maximum)
void TEST_CASE6(void) {
  volatile uint8_t OUT[18] __attribute__((aligned(8)));
  for (int i = 0; i < 18; i++) OUT[i] = 170;
  VSET(2, e8, m1);
  VLOAD_8(v24, 8, 0);
  VLOAD_8(v8, 1, 2);
  VLOAD_8(v9, 11, 12);
  VLOAD_8(v10, 21, 22);
  VLOAD_8(v11, 31, 32);
  VLOAD_8(v12, 41, 42);
  VLOAD_8(v13, 51, 52);
  VLOAD_8(v14, 61, 62);
  VLOAD_8(v15, 71, 72);
  asm volatile("vsoxseg8ei8.v v8, (%0), v24" ::"r"(OUT) : "memory");
  VVCMP_U8(6, OUT, 2, 12, 22, 32, 42, 52, 62, 72, 1, 11, 21, 31, 41, 51, 61, 71, 170, 170);
}

// nf=2, SEW=64, ei8
void TEST_CASE7(void) {
  volatile uint64_t OUT[5] __attribute__((aligned(8)));
  for (int i = 0; i < 5; i++) OUT[i] = 170;
  VSET(2, e64, m1);
  VLOAD_8(v24, 16, 0);
  VLOAD_64(v8, 1, 2);
  VLOAD_64(v9, 11, 12);
  asm volatile("vsoxseg2ei8.v v8, (%0), v24" ::"r"(OUT) : "memory");
  VVCMP_U64(7, OUT, 2, 12, 1, 11, 170);
}

// Masked: inactive segments are not written
void TEST_CASE8(void) {
  volatile uint8_t OUT[16] __attribute__((aligned(8)));
  for (int i = 0; i < 16; i++) OUT[i] = 170;
  VSET(1, e8, m1);
  VLOAD_8(v0, 0xb5);
  VSET(8, e8, m1);
  VLOAD_8(v24, 0, 6, 12, 2, 8, 14, 4, 10);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v9, 11, 12, 13, 14, 15, 16, 17, 18);
  asm volatile("vsoxseg2ei8.v v8, (%0), v24, v0.t" ::"r"(OUT) : "memory");
  VVCMP_U8(8, OUT, 1, 11, 170, 170, 170, 170, 170, 170, 5, 15, 8, 18, 3, 13, 6, 16);
}

// WAR on non-base source registers: v9 and v11 are overwritten right after
// the store is issued, the store must still write the old values
void TEST_CASE9(void) {
  volatile uint8_t OUT[32] __attribute__((aligned(8)));
  for (int i = 0; i < 32; i++) OUT[i] = 170;
  VSET(8, e8, m1);
  VLOAD_8(v24, 0, 12, 24, 4, 16, 28, 8, 20);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v9, 11, 12, 13, 14, 15, 16, 17, 18);
  VLOAD_8(v10, 21, 22, 23, 24, 25, 26, 27, 28);
  VLOAD_8(v11, 31, 32, 33, 34, 35, 36, 37, 38);
  asm volatile(
      "vsoxseg4ei8.v v8, (%0), v24\n\t"
      "vmv.v.i v11, 0\n\t"
      "vmv.v.i v9, 0"
      ::"r"(OUT) : "memory");
  VVCMP_U8(9, OUT, 1, 11, 21, 31, 4, 14, 24, 34, 7, 17, 27, 37, 2, 12, 22, 32, 5, 15, 25, 35, 8, 18, 28, 38, 3, 13, 23, 33, 6, 16, 26, 36);
}

// Ordered: segments with the same index are written in element order,
// so the last element wins
void TEST_CASE10(void) {
  volatile uint8_t OUT[12] __attribute__((aligned(8)));
  for (int i = 0; i < 12; i++) OUT[i] = 170;
  VSET(5, e8, m1);
  VLOAD_8(v24, 0, 8, 0, 8, 0);
  VLOAD_8(v8, 1, 2, 3, 4, 5);
  VLOAD_8(v9, 11, 12, 13, 14, 15);
  asm volatile("vsoxseg2ei8.v v8, (%0), v24" ::"r"(OUT) : "memory");
  VVCMP_U8(10, OUT, 5, 15, 170, 170, 170, 170, 170, 170, 4, 14, 170, 170);
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
  TEST_CASE8();
  TEST_CASE9();
  TEST_CASE10();

  EXIT_CHECK();
}
