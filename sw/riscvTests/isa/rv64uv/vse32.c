// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "vector_macros.h"


//**********Checking functionality of vse32********//
void TEST_CASE1(void) {
  volatile uint32_t ALIGNED_I32[16];
  VSET(16, e32, m2);
  VLOAD_32(v2, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  asm volatile("vse32.v v2, (%0)" ::"r"(ALIGNED_I32) : "memory");
  VVCMP_U32(1, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}

//*******Checking functionality of vse32 with different values of masking
// register******//
void TEST_CASE2(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xFF, 0xFF);

  volatile uint32_t ALIGNED_I32[16]={0};
  VSET(16, e32, m2);
  VLOAD_32(v2, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  asm volatile("vse32.v v2, (%0), v0.t" ::"r"(ALIGNED_I32) : "memory");
  VCLEAR(v2);
  VVCMP_U32(2, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}

void TEST_CASE3(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0x00, 0x00);

  volatile uint32_t ALIGNED_I32[16];
  VSET(16, e32, m4);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse32.v v4, (%0)" ::"r"(ALIGNED_I32) : "memory");
  VCLEAR(v4);
  VLOAD_32(v4, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  asm volatile("vse32.v v4, (%0), v0.t" ::"r"(ALIGNED_I32) : "memory");
  VCLEAR(v4);
  VVCMP_U32(3, ALIGNED_I32, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16);
}

void TEST_CASE4(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xAA, 0xAA);

  volatile uint32_t ALIGNED_I32[16];
  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse32.v v8, (%0)" ::"r"(ALIGNED_I32) : "memory");
  VCLEAR(v8);
  VLOAD_32(v8,  0x11111111, 0x22222222, 0x33333333, 0x44444444,
           0x55555555, 0x66666666, 0x77777777, 0x88888888, 0x99999999,
           0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd, 0xeeeeeeee,
           0xffffffff,0x00000000);
  asm volatile("vse32.v v8, (%0), v0.t" ::"r"(ALIGNED_I32) : "memory");
  VCLEAR(v8);
  VVCMP_U32(4, ALIGNED_I32, 1, 0x22222222, 3, 0x44444444, 5, 0x66666666, 7,
            0x88888888, 9, 0xaaaaaaaa, 11, 0xcccccccc, 13, 0xeeeeeeee, 15,
            0x00000000);
}

// change LMUL and EW
void TEST_CASE5(void) {
  volatile uint32_t ALIGNED_I32[16] = {0};
  VSET(16, e32, m2);
  VLOAD_32(v8, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  // VSET(16, e8, m8); // ? uncertain
  asm volatile("vse32.v v8, (%0)" ::"r"(ALIGNED_I32) : "memory");
  VCLEAR(v8);
  VVCMP_U32(5, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}


void TEST_CASE6(void) {
  VSET(2, e32, m1);
  VCLEAR(v0);
  VLOAD_32(v0, 0xAAAAAAAA, 0xAAAAAAAA);

  VSET(64, e32, m8);
  volatile uint32_t ALIGNED_I32[64];

  //init mem
  for (int i = 0; i < 64; i++) ALIGNED_I32[i] = 0xFFFFFFFF;

  VLOAD_32(v8,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
               17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
               33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
               49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);
  asm volatile("vse32.v v8, (%0), v0.t" ::"r"(ALIGNED_I32) : "memory");

  VVCMP_U32(6, ALIGNED_I32,
    0xFFFFFFFF,  2, 0xFFFFFFFF,  4, 0xFFFFFFFF,  6, 0xFFFFFFFF,  8, 0xFFFFFFFF, 10, 0xFFFFFFFF, 12, 0xFFFFFFFF, 14, 0xFFFFFFFF, 16,
    0xFFFFFFFF, 18, 0xFFFFFFFF, 20, 0xFFFFFFFF, 22, 0xFFFFFFFF, 24, 0xFFFFFFFF, 26, 0xFFFFFFFF, 28, 0xFFFFFFFF, 30, 0xFFFFFFFF, 32,
    0xFFFFFFFF, 34, 0xFFFFFFFF, 36, 0xFFFFFFFF, 38, 0xFFFFFFFF, 40, 0xFFFFFFFF, 42, 0xFFFFFFFF, 44, 0xFFFFFFFF, 46, 0xFFFFFFFF, 48,
    0xFFFFFFFF, 50, 0xFFFFFFFF, 52, 0xFFFFFFFF, 54, 0xFFFFFFFF, 56, 0xFFFFFFFF, 58, 0xFFFFFFFF, 60, 0xFFFFFFFF, 62, 0xFFFFFFFF, 64);
}

// Store with a vstart whose byte offset is not aligned to the VRF word
// (ELENB = 8 with RVD), on a vl that spans all FUs: vl=16 e32 = 64 bytes =
// 2 VRF rows of 4 FUs x 8 B. Elements before vstart must not be written.
// vstart=5 -> byte 20, inside FU 2's word: FUs 0 and 1 start one row ahead,
// so a store that reads all FUs from the same VRF row writes row-1 data
// into the row-0 slots of memory.
static void vse32_unaligned_vstart(int casenum, unsigned long vstart) {
  volatile uint32_t SRC[16] __attribute__((aligned(64)));
  volatile uint32_t OUT[16] __attribute__((aligned(64)));
  for (int i = 0; i < 16; i++) { SRC[i] = i + 1; OUT[i] = 0xdeadbeef; }

  // LMUL=2 so VLMAX >= 16 also with VLEN=256 (with m1, vl would be cut to 8)
  VSET(16, e32, m2);
  asm volatile("vle32.v v2, (%0)" ::"r"(SRC) : "memory", "v2", "v3");
  asm volatile(
      "csrw    vstart, %1\n\t"
      "vse32.v v2, (%0)"
      ::"r"(OUT), "r"(vstart) : "memory");
  asm volatile("csrw vstart, 0");

  // Read OUT back through the VLSU (ordered after the store)
  asm volatile("vle32.v v4, (%0)" ::"r"(OUT) : "memory", "v4", "v5");
  if (vstart == 1) {
    VCMP_U32(casenum, v4, 0xdeadbeef, 2, 3, 4, 5, 6, 7, 8,
                          9, 10, 11, 12, 13, 14, 15, 16);
  } else { // vstart == 5
    VCMP_U32(casenum, v4, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
                          0xdeadbeef, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  }
}

// vstart = 5 -> byte 20, middle of FU 2's word (FUs 0 and 1 a row ahead)
void TEST_CASE7(void) { vse32_unaligned_vstart(7, 5); }

// vstart = 1 -> byte 4, middle of FU 0's word (all FUs in the same row)
void TEST_CASE8(void) { vse32_unaligned_vstart(8, 1); }

// Same as above, but vl=32 e32 = 128 B. With the double-bandwidth VLSU the
// vector is split 64 B / 64 B between the two interfaces, so each interface
// spans 2 VRF rows and its FUs below the starting FU are a full row ahead.
// LMUL=4 so VLMAX >= 32 also with VLEN=256.
static void vse32_unaligned_vstart_long(int casenum, unsigned long vstart) {
  volatile uint32_t SRC[32] __attribute__((aligned(64)));
  volatile uint32_t OUT[32] __attribute__((aligned(64)));
  for (int i = 0; i < 32; i++) { SRC[i] = i + 1; OUT[i] = 0xdeadbeef; }

  VSET(32, e32, m4);
  asm volatile("vle32.v v4, (%0)" ::"r"(SRC) : "memory", "v4", "v5", "v6", "v7");
  asm volatile(
      "csrw    vstart, %1\n\t"
      "vse32.v v4, (%0)"
      ::"r"(OUT), "r"(vstart) : "memory");
  asm volatile("csrw vstart, 0");

  for (int i = 0; i < 32; i++) {
    uint32_t exp = (i < (int)vstart) ? 0xdeadbeef : (uint32_t)(i + 1);
    if (OUT[i] != exp) {
      printf("[TC %d] Index %d FAILED. Got \"%u\", expected \"%u\".\n", casenum, i, OUT[i], exp);
      num_failed++;
      return;
    }
  }
  printf("[TC %d] PASSED.\n", casenum);
}

// vstart = 5 -> byte 20: interface 0, middle of FU 2's word, FUs 0-1 a row ahead
void TEST_CASE9(void) { vse32_unaligned_vstart_long(9, 5); }

// vstart = 21 -> byte 84: interface 1 (local byte 20); interface 0 has nothing to do
void TEST_CASE10(void) { vse32_unaligned_vstart_long(10, 21); }

// vstart = 9 -> byte 36: interface 0 row 1 (middle of FU 0's word), so
// interface 0 has 1 row of work and interface 1 has 2
void TEST_CASE11(void) { vse32_unaligned_vstart_long(11, 9); }

// Single-row store with vstart in FU 2: vl=8 e32 = 32 B = one VRF row
// (4 FUs x 8 B). With vstart=5 (byte 20), FUs 0 and 1 have no work at all
// (their counters start at their limit), FU 2 starts mid-word and FU 3 catches
// up. This is the case the vl=16 test hit on VLEN=256 when it used m1.
static void vse32_single_row_vstart(int casenum, unsigned long vstart) {
  volatile uint32_t SRC[8] __attribute__((aligned(64)));
  volatile uint32_t OUT[8] __attribute__((aligned(64)));
  for (int i = 0; i < 8; i++) { SRC[i] = i + 1; OUT[i] = 0xdeadbeef; }

  VSET(8, e32, m1);
  asm volatile("vle32.v v2, (%0)" ::"r"(SRC) : "memory", "v2");
  asm volatile(
      "csrw    vstart, %1\n\t"
      "vse32.v v2, (%0)"
      ::"r"(OUT), "r"(vstart) : "memory");
  asm volatile("csrw vstart, 0");

  for (int i = 0; i < 8; i++) {
    uint32_t exp = (i < (int)vstart) ? 0xdeadbeef : (uint32_t)(i + 1);
    if (OUT[i] != exp) {
      printf("[TC %d] Index %d FAILED. Got \"%u\", expected \"%u\".\n", casenum, i, OUT[i], exp);
      num_failed++;
      return;
    }
  }
  printf("[TC %d] PASSED.\n", casenum);
}

// vstart = 5 -> byte 20: FUs 0-1 idle, FU 2 mid-word, FU 3 catches up
void TEST_CASE12(void) { vse32_single_row_vstart(12, 5); }

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
  TEST_CASE11();
  TEST_CASE12();

  EXIT_CHECK();
}