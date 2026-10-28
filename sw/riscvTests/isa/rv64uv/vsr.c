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
  volatile uint8_t OUT[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) {
    INP[i] = i;
    OUT[i] = 0;
  }

  // Load INP to v2/v3, store to OUT, load OUT to v4/v5 to verify
  asm volatile(
    "vl2re8.v v2, (%0)\n\t"
    "vs2r.v v2, (%1)\n\t"
    "vl2re8.v v4, (%1)"
    :: "r"(INP), "r"(OUT) : "memory", "v2", "v3", "v4", "v5"
  );

  VCMP_U8(1, v4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(2, v5, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
}

void TEST_CASE2(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  volatile uint8_t OUT[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) { INP[i] = i; OUT[i] = 0; }

  asm volatile("vl2re8.v v6, (%0)" :: "r"(INP) : "memory", "v6", "v7");
  VSET(1, e32, m8);
  asm volatile("vs2r.v v6, (%0)" :: "r"(OUT) : "memory");
  VSET(16, e8, m1);
  asm volatile("vl2re8.v v8, (%0)" :: "r"(OUT) : "memory", "v8", "v9");

  VCMP_U8(3, v8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(4, v9, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
}

void TEST_CASE3(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  // Allocate an extra 64 bytes for guard region
  volatile uint8_t OUT[3 * 64] __attribute__((aligned(64))); 
  
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;
  for (int i = 0; i < 3 * 64; i++) OUT[i] = 170; // 0xAA guard pattern

  asm volatile(
    "vl2re8.v v10, (%0)\n\t"
    "vs2r.v v10, (%1)\n\t"
    // Load the GUARD region (offset +128) into v12 to ensure it wasn't touched
    "vl1re8.v v12, (%2)"
    :: "r"(INP), "r"(OUT), "r"(OUT + 128) : "memory", "v10", "v11", "v12"
  );

  // v12 should be entirely intact guard bytes (170)
  VCMP_U8(5, v12, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170);
}

void TEST_CASE4(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  volatile uint8_t OUT[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) {
    INP[i] = 255 - i;
    OUT[i] = 0;
  }

  // Store from the highest valid register pair (v30, v31)
  asm volatile(
    "vl2re8.v v30, (%0)\n\t"
    "vs2r.v v30, (%1)\n\t"
    "vl2re8.v v14, (%1)"
    :: "r"(INP), "r"(OUT) : "memory", "v14", "v15", "v30", "v31"
  );

  VCMP_U8(6, v14, 255, 254, 253, 252, 251, 250, 249, 248, 247, 246, 245, 244);
  VCMP_U8(7, v15, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 180);
}

void TEST_CASE5(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[4 * 64] __attribute__((aligned(64)));
  volatile uint8_t OUT[4 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 4 * 64; i++) { INP[i] = i; OUT[i] = 0; }

  // Offset 60 means a 128-byte load spans three 64-byte cache lines:
  // Bytes 0-3 in Line 1, Bytes 4-67 in Line 2, Bytes 68-127 in Line 3.
  int off = 60; 

  asm volatile(
    "vl2re8.v v2, (%0)\n\t"
    "vs2r.v v2, (%1)\n\t"
    "vl2re8.v v4, (%1)"
    :: "r"(INP + off), "r"(OUT + off) : "memory", "v2", "v3", "v4", "v5"
  );

  // Assert the first and last few bytes explicitly
  VCMP_U8(8, v4, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75);
  VCMP_U8(9, v5, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139);
}

void TEST_CASE6(void) {
  VSET(16, e8, m1);
  volatile uint8_t MEM[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) MEM[i] = 0;

  // 1. Vector Store followed immediately by a Scalar Load (Read-After-Write in Memory)
  // We initialize v6 to 0xBB, store it to MEM, and try to read MEM[0] into scalar t0
  uint8_t scalar_check = 0;
  asm volatile(
    "vmv.v.i v6, 11\n\t"       // load small immediate (11)
    "vs1r.v v6, (%1)\n\t"      // Store to memory
    "lb %0, 0(%1)"             // Instantly load back via scalar pipeline
    : "=r"(scalar_check) : "r"(MEM) : "memory", "v6"
  );

  const uint8_t expected_scalar[] = { 11 };
  VCMP(uint8_t, "%d", 10, expected_scalar, scalar_check);

  // 2. Scalar Store followed immediately by Vector Load 
  // Write 0x99 to MEM[1], then do a whole register load to see if it picks it up
  MEM[1] = 0x99; 
  asm volatile("vl1re8.v v8, (%0)" :: "r"(MEM) : "memory", "v8");
  
  VCMP_U8(11, v8, 11, 0x99, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11);
}

void TEST_CASE7(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  volatile uint8_t OUT[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) { INP[i] = i; OUT[i] = 0; }

  asm volatile(
    "vmv.v.i v0, 0\n\t"        // Set v0 (the mask register) to all zeros
    "vl2re8.v v10, (%0)\n\t"   // Load should ignore v0
    "vs2r.v v10, (%1)\n\t"     // Store should ignore v0
    "vl2re8.v v12, (%1)"       // Load back to verify
    :: "r"(INP), "r"(OUT) : "memory", "v0", "v10", "v11", "v12", "v13"
  );

  // If the LSU accidentally respected v0, these would be 0.
  VCMP_U8(12, v12, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  VCMP_U8(13, v13, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
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