// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// vlsseg<nf>e<eew>.v: strided segment loads.
// Field f, element i = mem[rs1 + i*rs2 + f*EEW/8], written to vd + f*EMUL.
// INP[k] = k, so every loaded element equals its element offset in INP.
#include "vector_macros.h"

// nf=2, e8, stride not a multiple of the segment size
void TEST_CASE1(void) {
  volatile uint8_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(8, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  asm volatile("vlsseg2e8.v v8, (%0), %1" ::"r"(&INP[0]), "r"(5) : "memory");
  VCMP_U8(1, v8, 0, 5, 10, 15, 20, 25, 30, 35);
  VCMP_U8(2, v9, 1, 6, 11, 16, 21, 26, 31, 36);
}

// nf=4, e16, stride 12 B
void TEST_CASE2(void) {
  volatile uint16_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(6, e16, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VCLEAR(v10);
  VCLEAR(v11);
  asm volatile("vlsseg4e16.v v8, (%0), %1" ::"r"(&INP[0]), "r"(12) : "memory");
  VCMP_U16(3, v8, 0, 6, 12, 18, 24, 30);
  VCMP_U16(4, v9, 1, 7, 13, 19, 25, 31);
  VCMP_U16(5, v10, 2, 8, 14, 20, 26, 32);
  VCMP_U16(6, v11, 3, 9, 15, 21, 27, 33);
}

// nf=3, e32, negative stride
void TEST_CASE3(void) {
  volatile uint32_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(5, e32, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VCLEAR(v10);
  asm volatile("vlsseg3e32.v v8, (%0), %1" ::"r"(&INP[48]), "r"(-16) : "memory");
  VCMP_U32(7, v8, 48, 44, 40, 36, 32);
  VCMP_U32(8, v9, 49, 45, 41, 37, 33);
  VCMP_U32(9, v10, 50, 46, 42, 38, 34);
}

// nf=2, e8, zero stride: every element reads the same segment
void TEST_CASE4(void) {
  volatile uint8_t INP[16] __attribute__((aligned(8)));
  for (int i = 0; i < 16; i++) INP[i] = i;
  VSET(8, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  asm volatile("vlsseg2e8.v v8, (%0), %1" ::"r"(&INP[3]), "r"(0) : "memory");
  VCMP_U8(10, v8, 3, 3, 3, 3, 3, 3, 3, 3);
  VCMP_U8(11, v9, 4, 4, 4, 4, 4, 4, 4, 4);
}

// nf=8 (maximum), e8
void TEST_CASE5(void) {
  volatile uint8_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(4, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VCLEAR(v10);
  VCLEAR(v11);
  VCLEAR(v12);
  VCLEAR(v13);
  VCLEAR(v14);
  VCLEAR(v15);
  asm volatile("vlsseg8e8.v v8, (%0), %1" ::"r"(&INP[0]), "r"(9) : "memory");
  VCMP_U8(12, v8, 0, 9, 18, 27);
  VCMP_U8(13, v9, 1, 10, 19, 28);
  VCMP_U8(14, v10, 2, 11, 20, 29);
  VCMP_U8(15, v11, 3, 12, 21, 30);
  VCMP_U8(16, v12, 4, 13, 22, 31);
  VCMP_U8(17, v13, 5, 14, 23, 32);
  VCMP_U8(18, v14, 6, 15, 24, 33);
  VCMP_U8(19, v15, 7, 16, 25, 34);
}

// nf=2, e64
void TEST_CASE6(void) {
  volatile uint64_t INP[16] __attribute__((aligned(8)));
  for (int i = 0; i < 16; i++) INP[i] = i;
  VSET(3, e64, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  asm volatile("vlsseg2e64.v v8, (%0), %1" ::"r"(&INP[0]), "r"(24) : "memory");
  VCMP_U64(20, v8, 0, 3, 6);
  VCMP_U64(21, v9, 1, 4, 7);
}

// LMUL=2, nf=4 (NFIELDS*EMUL = 8): fields at v8, v10, v12, v14
void TEST_CASE7(void) {
  volatile uint8_t INP[256] __attribute__((aligned(8)));
  for (int i = 0; i < 256; i++) INP[i] = i;
  VSET(32, e8, m2);
  VCLEAR(v8);
  VCLEAR(v10);
  VCLEAR(v12);
  VCLEAR(v14);
  asm volatile("vlsseg4e8.v v8, (%0), %1" ::"r"(&INP[0]), "r"(6) : "memory");
  VCMP_U8(22, v8, 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90, 96, 102, 108, 114, 120, 126, 132, 138, 144, 150, 156, 162, 168, 174, 180, 186);
  VCMP_U8(23, v10, 1, 7, 13, 19, 25, 31, 37, 43, 49, 55, 61, 67, 73, 79, 85, 91, 97, 103, 109, 115, 121, 127, 133, 139, 145, 151, 157, 163, 169, 175, 181, 187);
  VCMP_U8(24, v12, 2, 8, 14, 20, 26, 32, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92, 98, 104, 110, 116, 122, 128, 134, 140, 146, 152, 158, 164, 170, 176, 182, 188);
  VCMP_U8(25, v14, 3, 9, 15, 21, 27, 33, 39, 45, 51, 57, 63, 69, 75, 81, 87, 93, 99, 105, 111, 117, 123, 129, 135, 141, 147, 153, 159, 165, 171, 177, 183, 189);
}

// EEW != SEW: vtype e8,m1 with EEW=32 -> EMUL=4, fields at v8 and v12
void TEST_CASE8(void) {
  volatile uint32_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(8, e8, m1);
  VCLEAR(v8);
  VCLEAR(v12);
  asm volatile("vlsseg2e32.v v8, (%0), %1" ::"r"(&INP[0]), "r"(12) : "memory");
  VCMP_U32(26, v8, 0, 3, 6, 9, 12, 15, 18, 21);
  VCMP_U32(27, v12, 1, 4, 7, 10, 13, 16, 19, 22);
}

// Masked: inactive elements keep their previous value (0)
void TEST_CASE9(void) {
  volatile uint8_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(2, e8, m1);
  VLOAD_8(v0, 0xb5, 0x6c);
  VSET(16, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  asm volatile("vlsseg2e8.v v8, (%0), %1, v0.t" ::"r"(&INP[1]), "r"(3) : "memory");
  VCMP_U8(28, v8, 1, 0, 7, 0, 13, 16, 0, 22, 0, 0, 31, 34, 0, 40, 43, 0);
  VCMP_U8(29, v9, 2, 0, 8, 0, 14, 17, 0, 23, 0, 0, 32, 35, 0, 41, 44, 0);
}

// Hazards on non-base field registers, issued right after the load:
// the scoreboard must cover v8..v11, not only v8
void TEST_CASE10(void) {
  volatile uint8_t INP[256] __attribute__((aligned(8)));
  for (int i = 0; i < 256; i++) INP[i] = i;
  VSET(32, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VCLEAR(v10);
  asm volatile("vmv.v.i v11, -1");
  asm volatile(
      "vlsseg4e8.v v8, (%0), %1\n\t"
      "vmv.v.i     v10, 7\n\t"   // WAW on field 2
      "vadd.vi     v20, v11, 0"   // RAW on field 3
      ::"r"(INP), "r"(7) : "memory");
  VCMP_U8(30, v10, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7);
  VCMP_U8(31, v20, 3, 10, 17, 24, 31, 38, 45, 52, 59, 66, 73, 80, 87, 94, 101, 108, 115, 122, 129, 136, 143, 150, 157, 164, 171, 178, 185, 192, 199, 206, 213, 220);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();
  #if ELEN == 64
    TEST_CASE6();
  #endif
  TEST_CASE7();
  TEST_CASE8();
  TEST_CASE9();
  TEST_CASE10();

  EXIT_CHECK();
}
