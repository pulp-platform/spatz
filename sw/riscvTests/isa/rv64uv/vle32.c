// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "vector_macros.h"

//**********Checking functionality of vle32 ********//
void TEST_CASE1(void) {
  VSET(8, e32, m1);
  volatile uint32_t INP1[] = {0xDEADBE00, 0xDEADBE9f, 0xDEADBEe4, 0xDEADBE19,
                              0xDEADBE20, 0xDEADBE8f, 0xDEADBE2e, 0xDEADBE05};

  asm volatile("vle32.v v1, (%0)" ::"r"(INP1) : "memory");
  VCMP_U32(1, v1, 0xDEADBE00, 0xDEADBE9f, 0xDEADBEe4, 0xDEADBE19,
                  0xDEADBE20, 0xDEADBE8f, 0xDEADBE2e, 0xDEADBE05);
}

//*******Checking functionality of vle32 with different values of masking register******//
void TEST_CASE2(void) {
  volatile uint32_t INP1[] = {0xDEADBE00, 0xDEADBE9f, 0xDEADBEe4, 0xDEADBE19,
                              0xDEADBE20, 0xDEADBE8f, 0xDEADBE2e, 0xDEADBE05};
  VSET(1, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xFF);

  VSET(8, e32, m1);
  VCLEAR(v1);
  asm volatile("vle32.v v1, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U32(2, v1, 0xDEADBE00, 0xDEADBE9f, 0xDEADBEe4, 0xDEADBE19,
                  0xDEADBE20, 0xDEADBE8f, 0xDEADBE2e, 0xDEADBE05);
}

void TEST_CASE3(void) {
  volatile uint32_t INP1[] = {0xDEADBEe0, 0xDEADBEd3, 0xDEADBE40, 0xDEADBEd1,
                              0xDEADBE84, 0xDEADBE48, 0xDEADBE89, 0xDEADBE88};
  VSET(1, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0x00);

  VSET(8, e32, m1);
  VCLEAR(v1);
  VLOAD_32(v1, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vle32.v v1, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U32(3, v1, 1, 2, 3, 4, 5, 6, 7, 8);
}

void TEST_CASE4(void) {
  volatile uint32_t INP1[] = {0xDEADBEe0, 0xDEADBEd3, 0xDEADBE40, 0xDEADBEd1,
                              0xDEADBE84, 0xDEADBE48, 0xDEADBE89, 0xDEADBE88};
  VSET(1, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xAA);

  VSET(8, e32, m1);
  VCLEAR(v1);
  VLOAD_32(v1, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vle32.v v1, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U32(4, v1, 1, 0xDEADBEd3, 3, 0xDEADBEd1, 5, 0xDEADBE48, 7, 0xDEADBE88);
}

void TEST_CASE5(void) {
  volatile uint32_t INP1[] = {0xDEADBEe0, 0xDEADBEd3, 0xDEADBE40, 0xDEADBEd1,
                              0xDEADBE84, 0xDEADBE48, 0xDEADBE89, 0xDEADBE88};
  VSET(1, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xAA);

  VSET(8, e32, m8);
  VCLEAR(v8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vle32.v v8, (%0), v0.t" ::"r"(INP1) : "memory");
  VCMP_U32(5, v8, 1, 0xDEADBEd3, 3, 0xDEADBEd1, 5, 0xDEADBE48, 7, 0xDEADBE88);
}

//*******Checking functionality of vle32 with large number of elements******//
void TEST_CASE6(void) {
  VSET(32, e32, m8);
  volatile uint32_t INP1[] = {
      0x00000000, 0x00000001, 0x00000002, 0x00000003,
      0x00000004, 0x00000005, 0x00000006, 0x00000007,
      0x00000008, 0x00000009, 0x0000000A, 0x0000000B,
      0x0000000C, 0x0000000D, 0x0000000E, 0x0000000F,

      0x00000010, 0x00000011, 0x00000012, 0x00000013,
      0x00000014, 0x00000015, 0x00000016, 0x00000017,
      0x00000018, 0x00000019, 0x0000001A, 0x0000001B,
      0x0000001C, 0x0000001D, 0x0000001E, 0x0000001F
  };

  asm volatile("vle32.v v8, (%0)" ::"r"(INP1) : "memory");

  VCMP_U32(6, v8,
              0x00000000, 0x00000001, 0x00000002, 0x00000003,
              0x00000004, 0x00000005, 0x00000006, 0x00000007,
              0x00000008, 0x00000009, 0x0000000A, 0x0000000B,
              0x0000000C, 0x0000000D, 0x0000000E, 0x0000000F,

              0x00000010, 0x00000011, 0x00000012, 0x00000013,
              0x00000014, 0x00000015, 0x00000016, 0x00000017,
              0x00000018, 0x00000019, 0x0000001A, 0x0000001B,
              0x0000001C, 0x0000001D, 0x0000001E, 0x0000001F);
}

void TEST_CASE7(void) {
  VSET(64, e32, m8);
  volatile uint32_t INP1[] = {
      0x00000000, 0x00000001, 0x00000002, 0x00000003,
      0x00000004, 0x00000005, 0x00000006, 0x00000007,
      0x00000008, 0x00000009, 0x0000000A, 0x0000000B,
      0x0000000C, 0x0000000D, 0x0000000E, 0x0000000F,

      0x00000010, 0x00000011, 0x00000012, 0x00000013,
      0x00000014, 0x00000015, 0x00000016, 0x00000017,
      0x00000018, 0x00000019, 0x0000001A, 0x0000001B,
      0x0000001C, 0x0000001D, 0x0000001E, 0x0000001F,

      0x00000020, 0x00000021, 0x00000022, 0x00000023,
      0x00000024, 0x00000025, 0x00000026, 0x00000027,
      0x00000028, 0x00000029, 0x0000002A, 0x0000002B,
      0x0000002C, 0x0000002D, 0x0000002E, 0x0000002F,

      0x00000030, 0x00000031, 0x00000032, 0x00000033,
      0x00000034, 0x00000035, 0x00000036, 0x00000037,
      0x00000038, 0x00000039, 0x0000003A, 0x0000003B,
      0x0000003C, 0x0000003D, 0x0000003E, 0x0000003F
  };

  asm volatile("vle32.v v8, (%0)" ::"r"(INP1) : "memory");

  VCMP_U32(7, v8,
    0x00000000, 0x00000001, 0x00000002, 0x00000003,
    0x00000004, 0x00000005, 0x00000006, 0x00000007,
    0x00000008, 0x00000009, 0x0000000A, 0x0000000B,
    0x0000000C, 0x0000000D, 0x0000000E, 0x0000000F,

    0x00000010, 0x00000011, 0x00000012, 0x00000013,
    0x00000014, 0x00000015, 0x00000016, 0x00000017,
    0x00000018, 0x00000019, 0x0000001A, 0x0000001B,
    0x0000001C, 0x0000001D, 0x0000001E, 0x0000001F,

    0x00000020, 0x00000021, 0x00000022, 0x00000023,
    0x00000024, 0x00000025, 0x00000026, 0x00000027,
    0x00000028, 0x00000029, 0x0000002A, 0x0000002B,
    0x0000002C, 0x0000002D, 0x0000002E, 0x0000002F,

    0x00000030, 0x00000031, 0x00000032, 0x00000033,
    0x00000034, 0x00000035, 0x00000036, 0x00000037,
    0x00000038, 0x00000039, 0x0000003A, 0x0000003B,
    0x0000003C, 0x0000003D, 0x0000003E, 0x0000003F);
}

// TODO
void TEST_CASE8(void) {
  volatile uint32_t INP1[] = {
      0x00000000, 0x00000001, 0x00000002, 0x00000003,
      0x00000004, 0x00000005, 0x00000006, 0x00000007,
      0x00000008, 0x00000009, 0x0000000A, 0x0000000B,
      0x0000000C, 0x0000000D, 0x0000000E, 0x0000000F,

      0x00000010, 0x00000011, 0x00000012, 0x00000013,
      0x00000014, 0x00000015, 0x00000016, 0x00000017,
      0x00000018, 0x00000019, 0x0000001A, 0x0000001B,
      0x0000001C, 0x0000001D, 0x0000001E, 0x0000001F,

      0x00000020, 0x00000021, 0x00000022, 0x00000023,
      0x00000024, 0x00000025, 0x00000026, 0x00000027,
      0x00000028, 0x00000029, 0x0000002A, 0x0000002B,
      0x0000002C, 0x0000002D, 0x0000002E, 0x0000002F,

      0x00000030, 0x00000031, 0x00000032, 0x00000033,
      0x00000034, 0x00000035, 0x00000036, 0x00000037,
      0x00000038, 0x00000039, 0x0000003A, 0x0000003B,
      0x0000003C, 0x0000003D, 0x0000003E, 0x0000003F
  };

  VSET(8, e8, m1);
  VCLEAR(v0);
  VLOAD_8(v0, 0xAA, 0xAA, 0xAA, 0xAA, 0xAB, 0xAB, 0xAB, 0xAB);

  VSET(64, e32, m8);
  VCLEAR(v8);
  VLOAD_32(v8,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99,
      99,  99,  99,  99,  99,  99,  99,  99);

  asm volatile("vle32.v v8, (%0), v0.t" ::"r"(INP1) : "memory");

  VCMP_U32(8, v8,
      99, 0x00000001, 99, 0x00000003,
      99, 0x00000005, 99, 0x00000007,
      99, 0x00000009, 99, 0x0000000B,
      99, 0x0000000D, 99, 0x0000000F,

      99, 0x00000011, 99, 0x00000013,
      99, 0x00000015, 99, 0x00000017,
      99, 0x00000019, 99, 0x0000001B,
      99, 0x0000001D, 99, 0x0000001F,

      0x00000020, 0x00000021, 99, 0x00000023,
      99, 0x00000025, 99, 0x00000027,
      0x00000028, 0x00000029, 99, 0x0000002B,
      99, 0x0000002D, 99, 0x0000002F,

      0x00000030, 0x00000031, 99, 0x00000033,
      99, 0x00000035, 99, 0x00000037,
      0x00000038, 0x00000039, 99, 0x0000003B,
      99, 0x0000003D, 99, 0x0000003F);
}

void TEST_CASE9(void) {
  VSET(4, e32, m1);
  volatile uint32_t MEM[4] = {0, 0, 0, 0};
  
  // All indices point to offset 0 (MEM[0])
  uint32_t indices[] = {0, 0, 0, 0};
  
  // Data to write: 1, 2, 3, 4
  uint32_t data[] = {0x01, 0x02, 0x03, 0x04};
  
  asm volatile(
    "vle32.v v2, (%1)\n\t"        // v2 = indices (0, 0, 0, 0)
    "vle32.v v3, (%2)\n\t"        // v3 = data (1, 2, 3, 4)
    "vsoxei32.v v3, (%0), v2\n\t" // Ordered indexed store
    :: "r"(MEM), "r"(indices), "r"(data) : "memory"
  );
  
  // Load the memory back into a vector register to check it
  asm volatile("vle32.v v4, (%0)" :: "r"(MEM) : "memory");
  
  // Since order is preserved, the last element (4) MUST be the final value in MEM[0].
  // MEM[1], MEM[2], and MEM[3] should remain 0.
  VCMP_U32(9, v4, 0x04, 0, 0, 0); 
}

void TEST_CASE10(void) {
  VSET(2, e32, m1);
  volatile uint8_t MEM[] __attribute__((aligned(8))) = {
      0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
      0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
  };
  
  uint32_t *unaligned_base = (uint32_t*)&MEM[6];
  asm volatile("vle32.v v1, (%0)" :: "r"(unaligned_base) : "memory");

  VCMP_U32(10, v1, 0x99887766, 0xddccbbaa);
}

void TEST_CASE11(void) {
  VSET(8, e32, m1);
  
  // Initialize memory with 0s. 
  volatile uint32_t MEM[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  
  // Data we want to write
  uint32_t data[] = {0x0a, 0x0a, 0x0a, 0x0a, 0x32, 0x3c, 0x46, 0x50};
  
  // Elements 0, 1, 2, 3 hit byte offset 0 to force uneven backpressure.
  // Elements 4, 5, 6, 7 point to independent addresses.
  uint32_t indices[] = {0, 0, 0, 0, 0x10, 0x14, 0x18, 0x1c};
  
  // MASK in v0: 10100111 in binary (0xA7)
  // Elements 4, 5, 6, 7 masks: 0, 1, 0, 1
  uint8_t mask = 0xA7; 
  
  asm volatile(
    "vle32.v v1, (%1)\n\t"              // Load data into v1
    "vle32.v v2, (%2)\n\t"              // Load indices into v2
    "vle8.v  v0, (%3)\n\t"              // Load mask into v0
    "vsuxei32.v v1, (%0), v2, v0.t\n\t" // Masked indexed store
    :: "r"(MEM), "r"(data), "r"(indices), "r"(&mask) : "memory"
  );
  
  asm volatile("vle32.v v4, (%0)" :: "r"(MEM) : "memory");
  // Expected for MEM[4..7]: 0x28 masked (0), 0x3c written, 0x46 masked (0), 0x50 written
  VCMP_U32(11, v4, 0x0a, 0, 0, 0, 0, 0x3c, 0, 0x50);
}

void TEST_CASE12(void) {
  volatile uint32_t MEM[] = {0x111, 0x222, 0x333, 0x444};
  
  // 1. Enable the Vector Coprocessor in mstatus (VS field = bits 14:13)
   // This prevents the illegal instruction exception when running alone.
  asm volatile(
    "li t0, (3 << 13)\n\t"
    "csrs mstatus, t0\n\t"
    :: : "t0"
  );
  
  // 2. Configure vector length vl = 0 and valid vtype (e32, m1)
  int zero_val = 0;
  asm volatile(
    "mv t0, %0\n\t"
    "vsetvli zero, t0, e32, m1, ta, ma"
    :: "r"(zero_val) : "t0", "memory"
  );
  
  // 3. Issue the vl = 0 load (Triggers Bug D28)
  asm volatile("vle32.v v1, (%0)" :: "r"(MEM) : "memory");
  
  // 4. Set vl = 4 for normal execution
  int len_val = 4;
  asm volatile(
    "mv t0, %0\n\t"
    "vsetvli zero, t0, e32, m1, ta, ma"
    :: "r"(len_val) : "t0", "memory"
  );
  
  // 5. Execute a subsequent load. 
  // With Bug D28 present, the stuck ID causes this to hang the core.
  asm volatile("vle32.v v2, (%0)" :: "r"(MEM) : "memory");
  
  VCMP_U32(12, v2, 0x111, 0x222, 0x333, 0x444);
}

void TEST_CASE13(void) {
  VSET(4, e32, m1);
  volatile uint32_t MEM[] = {0x01, 0x02, 0x03, 0x04};
  
  // Clear v1
  asm volatile("vmv.v.i v1, 0");
  
  // Set vstart to 2.
  // Correct behavior: the VERY NEXT vector instruction skips elements 0 and 1.
  asm volatile("csrw vstart, 2");
  
  // Load MEM. Elements 0 and 1 should NOT be updated (should remain 0).
  // Elements 2 and 3 should be loaded with MEM[2] and MEM[3].
  asm volatile("vle32.v v1, (%0)" :: "r"(MEM) : "memory");
  
  // Clear vstart for subsequent tests
  asm volatile("csrw vstart, 0");
  
  // If bug D29 is present, this load started at vstart=0 (from the instruction before csrw)
  // and v1 will be {0x01, 0x02, 0x03, 0x04}.
  // If correct, v1 is {0, 0, 0x03, 0x04}.
  VCMP_U32(13, v1, 0, 0, 0x03, 0x04);
}

void TEST_CASE_D23_Aggressive(void) {
  // We will write to 4 distinct memory locations
  volatile uint32_t MEM[4] = {0, 0, 0, 0};
  
  uint32_t indices[32];
  uint32_t data[32];
  
  // Create an overlapping pattern: 
  // Indices map to byte offsets: 0, 4, 8, 12, 0, 4, 8, 12...
  // Data is the element index + 1 (1 to 32)
  for(int i = 0; i < 32; i++) {
      indices[i] = (i % 4) * 4; 
      data[i] = i + 1;
  }
  
  // 1. Use LMUL=8 to generate a massive stream of accesses (32 elements).
  // This guarantees elements are spread across all Spatz memory ports.
  VSET(32, e32, m8);
  
  asm volatile(
    "vle32.v v8, (%1)\n\t"        // Load indices into v8-v15
    "vle32.v v16, (%2)\n\t"       // Load data into v16-v23
    "vsoxei32.v v16, (%0), v8\n\t" // Ordered store (should preserve order)
    :: "r"(MEM), "r"(indices), "r"(data) : "memory"
  );
  
  // 2. Read back the 4 memory locations to verify the final written values.
  VSET(4, e32, m1);
  asm volatile("vle32.v v1, (%0)" :: "r"(MEM) : "memory");
  
  // 3. Because it's an ordered store (vsoxei), the LAST element written to 
  // each address MUST be the final value left in memory.
  // MEM[0] last written by i=28 -> data = 29
  // MEM[1] last written by i=29 -> data = 30
  // MEM[2] last written by i=30 -> data = 31
  // MEM[3] last written by i=31 -> data = 32
  //
  // If the bug is present, the independent ports will write out of order, 
  // and these final values will be random earlier data (e.g., 1, 5, 9...).
  VCMP_U32(14, v1, 29, 30, 31, 32); 
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
  //TEST_CASE_D23_Aggressive();

  EXIT_CHECK();
}