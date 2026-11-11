// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// vluxseg<nf>ei<eew>.v: unordered indexed segment loads.
// Field f, element i = mem[rs1 + vs2[i] + f*SEW/8], written to vd + f*LMUL.
// Data EEW = SEW, index EEW from the instruction (index EMUL = EEW/SEW*LMUL).
// INP[k] = k, so every loaded element equals its element offset in INP.
#include "vector_macros.h"

// nf=2, ei8, SEW=8, duplicated and non-monotonic indices
void TEST_CASE1(void) {
  volatile uint8_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(8, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VLOAD_8(v24, 7, 0, 30, 3, 3, 12, 50, 1);
  asm volatile("vluxseg2ei8.v v8, (%0), v24" ::"r"(INP) : "memory");
  VCMP_U8(1, v8, 7, 0, 30, 3, 3, 12, 50, 1);
  VCMP_U8(2, v9, 8, 1, 31, 4, 4, 13, 51, 2);
}

// nf=3, ei16 with SEW=8 (index EMUL=2: v24-v25)
void TEST_CASE2(void) {
  volatile uint8_t INP[256] __attribute__((aligned(8)));
  for (int i = 0; i < 256; i++) INP[i] = i;
  VSET(8, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VCLEAR(v10);
  VLOAD_16(v24, 200, 4, 17, 99, 0, 250, 31, 64);
  asm volatile("vluxseg3ei16.v v8, (%0), v24" ::"r"(INP) : "memory");
  VCMP_U8(3, v8, 200, 4, 17, 99, 0, 250, 31, 64);
  VCMP_U8(4, v9, 201, 5, 18, 100, 1, 251, 32, 65);
  VCMP_U8(5, v10, 202, 6, 19, 101, 2, 252, 33, 66);
}

// nf=2, ei8 with SEW=32, LMUL=2 (index EMUL=1/2, fields v8-v9 and v10-v11)
void TEST_CASE3(void) {
  volatile uint32_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(6, e32, m2);
  VCLEAR(v8);
  VCLEAR(v10);
  VLOAD_8(v24, 0, 40, 8, 100, 4, 64);
  asm volatile("vluxseg2ei8.v v8, (%0), v24" ::"r"(INP) : "memory");
  VCMP_U32(6, v8, 0, 10, 2, 25, 1, 16);
  VCMP_U32(7, v10, 1, 11, 3, 26, 2, 17);
}

// Index vector spanning several VRF words (64 B of ei32 indices): every
// field pass must restart reading the index vector from its first word
void TEST_CASE4(void) {
  volatile uint32_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(16, e32, m2);
  VCLEAR(v8);
  VCLEAR(v10);
  VLOAD_32(v24, 0, 56, 112, 168, 224, 32, 88, 144, 200, 8, 64, 120, 176, 232, 40, 96);
  asm volatile("vluxseg2ei32.v v8, (%0), v24" ::"r"(INP) : "memory");
  VCMP_U32(8, v8, 0, 14, 28, 42, 56, 8, 22, 36, 50, 2, 16, 30, 44, 58, 10, 24);
  VCMP_U32(9, v10, 1, 15, 29, 43, 57, 9, 23, 37, 51, 3, 17, 31, 45, 59, 11, 25);
}

// Same with 64 B of ei16 indices, nf=4, LMUL=2 (NFIELDS*EMUL = 8)
void TEST_CASE5(void) {
  volatile uint16_t INP[64] __attribute__((aligned(8)));
  for (int i = 0; i < 64; i++) INP[i] = i;
  VSET(32, e16, m2);
  VCLEAR(v8);
  VCLEAR(v10);
  VCLEAR(v12);
  VCLEAR(v14);
  VLOAD_16(v24, 6, 32, 58, 84, 110, 16, 42, 68, 94, 0, 26, 52, 78, 104, 10, 36, 62, 88, 114, 20, 46, 72, 98, 4, 30, 56, 82, 108, 14, 40, 66, 92);
  asm volatile("vluxseg4ei16.v v8, (%0), v24" ::"r"(INP) : "memory");
  VCMP_U16(10, v8, 3, 16, 29, 42, 55, 8, 21, 34, 47, 0, 13, 26, 39, 52, 5, 18, 31, 44, 57, 10, 23, 36, 49, 2, 15, 28, 41, 54, 7, 20, 33, 46);
  VCMP_U16(11, v10, 4, 17, 30, 43, 56, 9, 22, 35, 48, 1, 14, 27, 40, 53, 6, 19, 32, 45, 58, 11, 24, 37, 50, 3, 16, 29, 42, 55, 8, 21, 34, 47);
  VCMP_U16(12, v12, 5, 18, 31, 44, 57, 10, 23, 36, 49, 2, 15, 28, 41, 54, 7, 20, 33, 46, 59, 12, 25, 38, 51, 4, 17, 30, 43, 56, 9, 22, 35, 48);
  VCMP_U16(13, v14, 6, 19, 32, 45, 58, 11, 24, 37, 50, 3, 16, 29, 42, 55, 8, 21, 34, 47, 60, 13, 26, 39, 52, 5, 18, 31, 44, 57, 10, 23, 36, 49);
}

// nf=8 (maximum)
void TEST_CASE6(void) {
  volatile uint8_t INP[128] __attribute__((aligned(8)));
  for (int i = 0; i < 128; i++) INP[i] = i;
  VSET(4, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VCLEAR(v10);
  VCLEAR(v11);
  VCLEAR(v12);
  VCLEAR(v13);
  VCLEAR(v14);
  VCLEAR(v15);
  VLOAD_8(v24, 90, 0, 17, 45);
  asm volatile("vluxseg8ei8.v v8, (%0), v24" ::"r"(INP) : "memory");
  VCMP_U8(14, v8, 90, 0, 17, 45);
  VCMP_U8(15, v9, 91, 1, 18, 46);
  VCMP_U8(16, v10, 92, 2, 19, 47);
  VCMP_U8(17, v11, 93, 3, 20, 48);
  VCMP_U8(18, v12, 94, 4, 21, 49);
  VCMP_U8(19, v13, 95, 5, 22, 50);
  VCMP_U8(20, v14, 96, 6, 23, 51);
  VCMP_U8(21, v15, 97, 7, 24, 52);
}

// nf=2, SEW=64, ei8
void TEST_CASE7(void) {
  volatile uint64_t INP[32] __attribute__((aligned(8)));
  for (int i = 0; i < 32; i++) INP[i] = i;
  VSET(3, e64, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VLOAD_8(v24, 64, 0, 32);
  asm volatile("vluxseg2ei8.v v8, (%0), v24" ::"r"(INP) : "memory");
  VCMP_U64(22, v8, 8, 0, 4);
  VCMP_U64(23, v9, 9, 1, 5);
}

// Masked: inactive elements keep their previous value (0)
void TEST_CASE8(void) {
  volatile uint8_t INP[256] __attribute__((aligned(8)));
  for (int i = 0; i < 256; i++) INP[i] = i;
  VSET(2, e8, m1);
  VLOAD_8(v0, 0xb5, 0x6c);
  VSET(16, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VLOAD_8(v24, 3, 32, 61, 90, 119, 148, 177, 6, 35, 64, 93, 122, 151, 180, 9, 38);
  asm volatile("vluxseg2ei8.v v8, (%0), v24, v0.t" ::"r"(INP) : "memory");
  VCMP_U8(24, v8, 3, 0, 61, 0, 119, 148, 0, 6, 0, 0, 93, 122, 0, 180, 9, 0);
  VCMP_U8(25, v9, 4, 0, 62, 0, 120, 149, 0, 7, 0, 0, 94, 123, 0, 181, 10, 0);
}

// Hazards on non-base field registers, issued right after the load:
// the scoreboard must cover v8..v11, not only v8
void TEST_CASE9(void) {
  volatile uint8_t INP[256] __attribute__((aligned(8)));
  for (int i = 0; i < 256; i++) INP[i] = i;
  VSET(32, e8, m1);
  VCLEAR(v8);
  VCLEAR(v9);
  VCLEAR(v10);
  asm volatile("vmv.v.i v11, -1");
  VLOAD_8(v24, 7, 48, 89, 130, 171, 212, 13, 54, 95, 136, 177, 218, 19, 60, 101, 142, 183, 224, 25, 66, 107, 148, 189, 230, 31, 72, 113, 154, 195, 236, 37, 78);
  asm volatile(
      "vluxseg4ei8.v v8, (%0), v24\n\t"
      "vmv.v.i v10, 7\n\t"   // WAW on field 2
      "vadd.vi v20, v11, 0"   // RAW on field 3
      ::"r"(INP) : "memory");
  VCMP_U8(26, v10, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7);
  VCMP_U8(27, v20, 10, 51, 92, 133, 174, 215, 16, 57, 98, 139, 180, 221, 22, 63, 104, 145, 186, 227, 28, 69, 110, 151, 192, 233, 34, 75, 116, 157, 198, 239, 40, 81);
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
