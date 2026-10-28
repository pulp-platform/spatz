// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Navaneeth Kunhi Purayil <nkunhi@iis.ee.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;

  asm volatile("vl2re8.v v2, (%0)" :: "r"(INP) : "memory", "v2", "v3");

  VCMP_U8(1, v2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(2, v3, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
}

void TEST_CASE2(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[4 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 4 * 64; i++) INP[i] = i;   // wraps at 256, fine for uint8_t

  asm volatile("vl4re32.v v4, (%0)" :: "r"(INP) : "memory", "v4", "v5", "v6", "v7");

  VCMP_U8(3, v4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(4, v5, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  VCMP_U8(5, v6, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143);
  VCMP_U8(6, v7, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207);
}

void TEST_CASE3(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  volatile uint8_t OUT[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) {
    INP[i] = i;
    OUT[i] = 0;
  }

  // Load into v4/v5, store to OUT, load back into v6/v7 to verify store
  asm volatile(
    "vl2re8.v v4, (%0)\n\t"
    "vs2r.v v4, (%1)\n\t"
    "vl2re8.v v6, (%1)"
    :: "r"(INP), "r"(OUT) : "memory", "v4", "v5", "v6", "v7"
  );

  // Check the load worked
  VCMP_U8(7, v4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(8, v5, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  
  // Check the store worked (by verifying the load-back registers)
  VCMP_U8(9, v6, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(10, v7, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
}

void TEST_CASE4(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;

  // e32 instruction should move the exact same 128 bytes into v8 and v9
  asm volatile("vl2re32.v v8, (%0)" :: "r"(INP) : "memory", "v8", "v9");

  VCMP_U8(11, v8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(12, v9, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
}

void TEST_CASE5(void) {
  VSET(1, e32, m8);  // Set a tiny VL and odd vtype
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;

  asm volatile("vl2re8.v v10, (%0)" :: "r"(INP) : "memory", "v10", "v11");

  VSET(16, e8, m1);
  // Even with vl=1, whole register load must move all 64 bytes per register
  VCMP_U8(13, v10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(14, v11, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
}

void TEST_CASE6(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = 255 - i; // reverse to differentiate

  asm volatile("vl2re8.v v30, (%0)" :: "r"(INP) : "memory", "v30", "v31");

  VCMP_U8(15, v30, 255, 254, 253, 252, 251, 250, 249, 248, 247, 246, 245, 244);
  VCMP_U8(16, v31, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 180);
}

void TEST_CASE7(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;
  uint8_t guard_val = 170;

  // Initialize v12 and v15 to 0xAA (170) to ensure vl2r doesn't bleed into them
  asm volatile(
    "vmv.v.x v13, %1\n\t"
    "vmv.v.x v16, %1\n\t"
    "vl2re8.v v14, (%0)" 
    :: "r"(INP), "r"(guard_val) : "memory", "v13", "v14", "v15", "v16"
  );

  // v13 and v14 should contain the loaded data
  VCMP_U8(17, v14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(18, v15, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  
  // Neighbours v12 and v15 must remain untouched
  VCMP_U8(19, v13, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170);
  VCMP_U8(20, v16, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170);
}

void TEST_CASE8(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[4 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 4 * 64; i++) INP[i] = i;

  // WAW: v2,v3 loaded together, then v3 loaded again immediately overriding it
  asm volatile(
    "vl2re8.v v2, (%0)\n\t"
    "vl1re8.v v3, (%1)"
    :: "r"(INP), "r"(INP + 128) : "memory", "v2", "v3"
  );

  // v2 has original 0..63
  VCMP_U8(21, v2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  // v3 has overwritten 128..191 from the second instruction
  VCMP_U8(22, v3, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139);
}

void TEST_CASE9(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64 + 4] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64 + 4; i++) INP[i] = i; // 0..131

  // Load from an unaligned offset (+1 byte)
  asm volatile("vl2re8.v v16, (%0)" :: "r"(INP + 1) : "memory", "v16", "v17");

  // Everything shifts by +1
  VCMP_U8(23, v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  VCMP_U8(24, v17, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80);
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
