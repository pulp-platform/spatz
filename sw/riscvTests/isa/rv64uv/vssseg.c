// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// vssseg<nf>e<eew>.v: strided segment stores.
// mem[rs1 + i*rs2 + f*EEW/8] = (vs3 + f*EMUL)[i]
// Field f holds 10*f + i + 1. OUT is prefilled with 170 to catch stray writes.
#include "vector_macros.h"

// nf=2, e8
void TEST_CASE1(void) {
  volatile uint8_t OUT[20] __attribute__((aligned(8)));
  for (int i = 0; i < 20; i++) OUT[i] = 170;
  VSET(4, e8, m1);
  VLOAD_8(v8, 1, 2, 3, 4);
  VLOAD_8(v9, 11, 12, 13, 14);
  asm volatile("vssseg2e8.v v8, (%0), %1" ::"r"(&OUT[0]), "r"(5) : "memory");
  VVCMP_U8(1, OUT, 1, 11, 170, 170, 170, 2, 12, 170, 170, 170, 3, 13, 170, 170, 170, 4, 14, 170, 170, 170);
}

// nf=3, e16, stride 8 B
void TEST_CASE2(void) {
  volatile uint16_t OUT[12] __attribute__((aligned(8)));
  for (int i = 0; i < 12; i++) OUT[i] = 170;
  VSET(3, e16, m1);
  VLOAD_16(v8, 1, 2, 3);
  VLOAD_16(v9, 11, 12, 13);
  VLOAD_16(v10, 21, 22, 23);
  asm volatile("vssseg3e16.v v8, (%0), %1" ::"r"(&OUT[0]), "r"(8) : "memory");
  VVCMP_U16(2, OUT, 1, 11, 21, 170, 2, 12, 22, 170, 3, 13, 23, 170);
}

// nf=2, e32, negative stride
void TEST_CASE3(void) {
  volatile uint32_t OUT[8] __attribute__((aligned(8)));
  for (int i = 0; i < 8; i++) OUT[i] = 170;
  VSET(3, e32, m1);
  VLOAD_32(v8, 1, 2, 3);
  VLOAD_32(v9, 11, 12, 13);
  asm volatile("vssseg2e32.v v8, (%0), %1" ::"r"(&OUT[6]), "r"(-12) : "memory");
  VVCMP_U32(3, OUT, 3, 13, 170, 2, 12, 170, 1, 11);
}

// nf=8 (maximum), e8
void TEST_CASE4(void) {
  volatile uint8_t OUT[20] __attribute__((aligned(8)));
  for (int i = 0; i < 20; i++) OUT[i] = 170;
  VSET(2, e8, m1);
  VLOAD_8(v8, 1, 2);
  VLOAD_8(v9, 11, 12);
  VLOAD_8(v10, 21, 22);
  VLOAD_8(v11, 31, 32);
  VLOAD_8(v12, 41, 42);
  VLOAD_8(v13, 51, 52);
  VLOAD_8(v14, 61, 62);
  VLOAD_8(v15, 71, 72);
  asm volatile("vssseg8e8.v v8, (%0), %1" ::"r"(&OUT[0]), "r"(10) : "memory");
  VVCMP_U8(4, OUT, 1, 11, 21, 31, 41, 51, 61, 71, 170, 170, 2, 12, 22, 32, 42, 52, 62, 72, 170, 170);
}

// nf=2, e64
void TEST_CASE5(void) {
  volatile uint64_t OUT[6] __attribute__((aligned(8)));
  for (int i = 0; i < 6; i++) OUT[i] = 170;
  VSET(2, e64, m1);
  VLOAD_64(v8, 1, 2);
  VLOAD_64(v9, 11, 12);
  asm volatile("vssseg2e64.v v8, (%0), %1" ::"r"(&OUT[0]), "r"(24) : "memory");
  VVCMP_U64(5, OUT, 1, 11, 170, 2, 12, 170);
}

// LMUL=2, nf=2: sources at v8 and v10
void TEST_CASE6(void) {
  volatile uint8_t OUT[14] __attribute__((aligned(8)));
  for (int i = 0; i < 14; i++) OUT[i] = 170;
  VSET(4, e8, m2);
  VLOAD_8(v8, 1, 2, 3, 4);
  VLOAD_8(v10, 11, 12, 13, 14);
  asm volatile("vssseg2e8.v v8, (%0), %1" ::"r"(&OUT[0]), "r"(3) : "memory");
  VVCMP_U8(6, OUT, 1, 11, 170, 2, 12, 170, 3, 13, 170, 4, 14, 170, 170, 170);
}

// EEW != SEW: vtype e8,m1 with EEW=32 -> EMUL=4, sources at v8 and v12
void TEST_CASE7(void) {
  volatile uint32_t OUT[8] __attribute__((aligned(8)));
  for (int i = 0; i < 8; i++) OUT[i] = 170;
  VSET(4, e32, m4);
  VLOAD_32(v8, 1, 2, 3, 4);
  VLOAD_32(v12, 11, 12, 13, 14);
  VSET(4, e8, m1);
  asm volatile("vssseg2e32.v v8, (%0), %1" ::"r"(&OUT[0]), "r"(8) : "memory");
  VVCMP_U32(7, OUT, 1, 11, 2, 12, 3, 13, 4, 14);
}

// Masked: inactive segments are not written
void TEST_CASE8(void) {
  volatile uint8_t OUT[24] __attribute__((aligned(8)));
  for (int i = 0; i < 24; i++) OUT[i] = 170;
  VSET(1, e8, m1);
  VLOAD_8(v0, 0xb5);
  VSET(8, e8, m1);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v9, 11, 12, 13, 14, 15, 16, 17, 18);
  asm volatile("vssseg2e8.v v8, (%0), %1, v0.t" ::"r"(&OUT[0]), "r"(3) : "memory");
  VVCMP_U8(8, OUT, 1, 11, 170, 170, 170, 170, 3, 13, 170, 170, 170, 170, 5, 15, 170, 6, 16, 170, 170, 170, 170, 8, 18, 170);
}

// WAR on non-base source registers: v9 and v11 are overwritten right after
// the store is issued, the store must still write the old values
void TEST_CASE9(void) {
  volatile uint8_t OUT[40] __attribute__((aligned(8)));
  for (int i = 0; i < 40; i++) OUT[i] = 170;
  VSET(8, e8, m1);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v9, 11, 12, 13, 14, 15, 16, 17, 18);
  VLOAD_8(v10, 21, 22, 23, 24, 25, 26, 27, 28);
  VLOAD_8(v11, 31, 32, 33, 34, 35, 36, 37, 38);
  asm volatile(
      "vssseg4e8.v v8, (%0), %1\n\t"
      "vmv.v.i v11, 0\n\t"
      "vmv.v.i v9, 0"
      ::"r"(&OUT[0]), "r"(5) : "memory");
  VVCMP_U8(9, OUT, 1, 11, 21, 31, 170, 2, 12, 22, 32, 170, 3, 13, 23, 33, 170, 4, 14, 24, 34, 170, 5, 15, 25, 35, 170, 6, 16, 26, 36, 170, 7, 17, 27, 37, 170, 8, 18, 28, 38, 170);
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

  EXIT_CHECK();
}
