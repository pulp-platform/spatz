// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "vector_macros.h"

#define V8_GROUP "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15"
#define REPEAT7(stmt) do { for (int _i = 0; _i < 7; _i++) { stmt; } } while (0)

static inline uint32_t get_vlenb(void) {
  uint32_t vlenb;
  asm volatile("csrr %0, vlenb" : "=r"(vlenb));
  return vlenb;
}

static inline void drain_v8_m8(uint32_t vlenb) {
  uint32_t tmp;
  VSET(8 * vlenb, e8, m8);
  asm volatile("vmv.s.x    v1, zero    \n\t"
               "vredsum.vs v1, v8, v1  \n\t"
               "vmv.x.s    %0, v1      \n\t"
               : "=r"(tmp) :: "v1");
  (void)tmp;
}

//**********Checking functionality of vle8 ********//
void TEST_CASE1(void) {
  VSET(16, e8, m1);
  volatile uint8_t INP1[] = {0x00, 0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb};

  asm volatile("vle8.v v1, (%0)" ::"r"(INP1) : "memory");
  VCMP_U8(1, v1, 0x00, 0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0, 0xf9, 0xaa,
           0x71, 0xf0, 0xc3, 0x94, 0xbb);
}


//*******Checking functionality of vle8 with different values of masking
// register******//
void TEST_CASE2(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xFF, 0xFF);

  VSET(16, e8, m1);
  volatile uint8_t INP1[] = {0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
                              0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89};
  VCLEAR(v1);

  asm volatile("vle8.v v1, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U8(2, v1, 0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88, 0x88, 0xae,
          0x08, 0x91, 0x02, 0x59, 0x11, 0x89);
}

void TEST_CASE3(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0x00, 0x00);

  VSET(16, e8, m1);
  volatile uint8_t INP1[] = {0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
                              0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89};
  VLOAD_8(v1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

  asm volatile("vle8.v v1, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U8(3, v1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
}

void TEST_CASE4(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xAA, 0xAA);

  VSET(16, e8, m1);
  volatile uint8_t INP1[] = {0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
                              0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89};
  VCLEAR(v1);
  VLOAD_8(v1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

  asm volatile("vle8.v v1, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U8(4, v1, 1, 0xd3, 3, 0xd1, 5, 0x48, 7, 0x88, 9, 0xae, 11, 0x91, 13,
          0x59, 15, 0x89);
}

void TEST_CASE5(void) {
  VSET(2, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xAA, 0xAA);

  VSET(16, e8, m8);
  volatile uint8_t INP1[] = {0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
                              0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89};
  VCLEAR(v8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

  asm volatile("vle8.v v8, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U8(5, v8, 1, 0xd3, 3, 0xd1, 5, 0x48, 7, 0x88, 9, 0xae, 11, 0x91, 13,
          0x59, 15, 0x89);
}

void TEST_CASE6(void) {
  VSET(8, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);

  VSET(64, e8, m8);
  volatile uint8_t INP1[] = {0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
                              0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89,
                              0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
                              0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89,
                              0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
                              0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89,
                              0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
                              0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89};
  VCLEAR(v8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
              17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
              33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
              49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);

  asm volatile("vle8.v v8, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U8(6, v8, 1, 0xd3, 3, 0xd1, 5, 0x48, 7, 0x88, 9, 0xae, 11, 0x91, 13, 0x59, 15, 0x89,
                 17, 0xd3, 19, 0xd1, 21, 0x48, 23, 0x88, 25, 0xae, 27, 0x91, 29, 0x59, 31, 0x89,
                 33, 0xd3, 35, 0xd1, 37, 0x48, 39, 0x88, 41, 0xae, 43, 0x91, 45, 0x59, 47, 0x89,
                 49, 0xd3, 51, 0xd1, 53, 0x48, 55, 0x88, 57, 0xae, 59, 0x91, 61, 0x59, 63, 0x89);
}

void TEST_CASE7(void) {
  uint32_t vlenb;
  asm volatile("csrr %0, vlenb" : "=r"(vlenb));

  // m8 group v8..v15; only v15 (the last register) gets 5,6,7,8,...
  volatile uint8_t buf[8 * vlenb] __attribute__((aligned(8)));
  for (uint32_t i = 0; i < 8 * vlenb; i++)
    buf[i] = (i < 7 * vlenb) ? 0x11 : (uint8_t)(i - 7 * vlenb + 5);

  // Poison v15 so a premature read is visible as 0xFF
  VSET(vlenb, e8, m1);
  asm volatile("vmv.v.i v15, -1" ::: "v15");

  // Long producer: 8 * VLENB bytes, v15 is written at the very end
  VSET(8 * vlenb, e8, m8);
  asm volatile("vle8.v v8, (%0)" :: "r"(buf)
               : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory");

  // Consumer reads the last register of the group at LMUL=1
  VSET(4, e8, m1);
  asm volatile("vadd.vi v4, v15, 0" ::: "v4");

  VCMP_U8(7, v4, 5, 6, 7, 8);
}

void TEST_CASE8(void) {
  uint32_t vlenb = get_vlenb();
 
  volatile uint8_t buf[8 * vlenb] __attribute__((aligned(8)));
  for (uint32_t i = 0; i < 8 * vlenb; i++)
    buf[i] = 0x11;
 
  VSET(8 * vlenb, e8, m8);
  asm volatile("vle8.v v8, (%0)" :: "r"(buf) : V8_GROUP, "memory");
 
  VSET(vlenb, e8, m1);
  asm volatile("vmv.v.i v15, 3" ::: "v15");
 
  drain_v8_m8(vlenb);              // check the FINAL value, after both writers are done
 
  VSET(4, e8, m1);
  VCMP_U8(8, v15, 3, 3, 3, 3);
}

void TEST_CASE9(void) {
  uint32_t vlenb = get_vlenb();
  volatile uint8_t src = 0x11;              // one byte, read 8 * vlenb times

  // Long producer: every element of v8..v15 = 0x11, v15 written at the end
  VSET(8 * vlenb, e8, m8);
  asm volatile("vlse8.v v8, (%0), zero" :: "r"(&src)
               : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory");

  // Short write to the last group member, issued right behind the load (no drain)
  VSET(vlenb, e8, m1);
  asm volatile("vmv.v.i v15, 3" ::: "v15");

  VSET(4, e8, m1);
  VCMP_U8(9, v15, 3, 3, 3, 3);             // 0x11 if the load's write landed last
}

void TEST_CASE10(void) {
  uint32_t vlenb = get_vlenb();
  volatile uint8_t src  = 0x11;   // old contents of the whole group
  volatile uint8_t out  = 0;      // receives v15's elements last
  volatile uint8_t sync = 0;
  uint32_t result;

  // Fill v8..v15 with 0x11
  VSET(8 * vlenb, e8, m8);
  asm volatile("vlse8.v v8, (%0), zero" :: "r"(&src) : V8_GROUP, "memory");

  // Wait until the load has written v15 (replaces drain_v8_m8, no vmv.x.s):
  // the 1-element store reads v15, and the scalar load waits for the store
  VSET(1, e8, m1);
  asm volatile("vse8.v v15, (%0)" :: "r"(&sync) : "memory");
  (void)sync;

  // Long store reading v8..v15; v15 is read at the end
  VSET(8 * vlenb, e8, m8);
  asm volatile("vsse8.v v8, (%0), zero" :: "r"(&out) : "memory");

  // Must wait until the store has read v15
  VSET(vlenb, e8, m1);
  asm volatile("vmv.v.i v15, 3" ::: "v15");

  // The scalar load waits for the vector store to finish
  result = out;
  XCMP(10, result, 0x11);         // 3 if vmv.v.i overtook the store's read of v15
}

void TEST_CASE11(void) {
  uint32_t vlenb = get_vlenb();

  // m2 group v8..v9: 2 * vlenb bytes (128 B at VLEN = 512), stored back in place
  volatile uint8_t buf[2 * vlenb] __attribute__((aligned(8)));
  for (uint32_t i = 0; i < 2 * vlenb; i++)
    buf[i] = (i < vlenb) ? 0x11 : (uint8_t)(i - vlenb + 5);

  VSET(2 * vlenb, e8, m2);
  asm volatile("vle8.v v8, (%0)" :: "r"(buf) : "v8", "v9", "memory");
  // The VLSU executes in order, so the store below starts only after this load is done

  VSET(2 * vlenb, e8, m2);
  asm volatile("vse8.v v8, (%0)" :: "r"(buf) : "memory");

  VSET(vlenb, e8, m1);
  asm volatile("vmv.v.i v9, 3" ::: "v9");    // must wait until the store has read v9

  // Read back the part of buf that came from v9
  VSET(4, e8, m1);
  asm volatile("vle8.v v4, (%0)" :: "r"((volatile uint8_t *)buf + vlenb) : "v4", "memory");

  VCMP_U8(11, v4, 5, 6, 7, 8);       // 3, 3, 3, 3 if vmv.v.i overtook the store
}

void TEST_CASE12(void) {
  uint32_t vlenb;
  asm volatile("csrr %0, vlenb" : "=r"(vlenb));

  // m8 group v8..v15; only v15 (the last register) gets 5,6,7,8,...
  volatile uint8_t buf[8 * vlenb] __attribute__((aligned(8)));
  for (uint32_t i = 0; i < 8 * vlenb; i++)
    buf[i] = (i < 7 * vlenb) ? 0x11 : (uint8_t)(i - 7 * vlenb + 5);

  // Poison v15 so a premature read is visible as 0xFF
  VSET(vlenb, e8, m1);
  asm volatile("vmv.v.i v15, -1" ::: "v15");

  // Long producer: 8 * VLENB bytes, v15 is written at the very end
  VSET(8 * vlenb, e8, m8);
  asm volatile("vle8.v v8, (%0)" :: "r"(buf)
               : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory");

  // Consumer reads the last register of the group at LMUL=1
  VSET(4, e8, m1);
  asm volatile("vadd.vi v4, v15, 0" ::: "v4");

  VCMP_U8(12, v4, 5, 6, 7, 8);
}

void TEST_CASE13(void) {
  volatile uint8_t buf[64] __attribute__((aligned(64)));   // local: lives in L1 (stack)
  for (int i = 0; i < 64; i++) buf[i] = 0xff;

  VSET(4, e8, m1);
  REPEAT7(asm volatile("vle8.v v2, (%0)" :: "r"(buf) : "memory"));
  VCMP_U8(13, v2, 0xff, 0xff, 0xff, 0xff);
  VLOAD_8(v1, 1, 2, 3, 4);
  VCMP_U8(14, v1, 1, 2, 3, 4);
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
  TEST_CASE11();
  TEST_CASE12();
  TEST_CASE13();

  EXIT_CHECK();
}