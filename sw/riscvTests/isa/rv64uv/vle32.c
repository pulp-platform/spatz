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
  VSET(2, e32, m1);
  volatile uint8_t MEM[] __attribute__((aligned(8))) = {
      0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
      0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
  };

  uint32_t *unaligned_base = (uint32_t*)&MEM[6];
  asm volatile("vle32.v v1, (%0)" :: "r"(unaligned_base) : "memory");

  VCMP_U32(9, v1, 0x99887766, 0xddccbbaa);
}

void TEST_CASE10(void) {
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

  VCMP_U32(10, v2, 0x111, 0x222, 0x333, 0x444);
}

void TEST_CASE11(void) {
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
  VCMP_U32(11, v1, 0, 0, 0x03, 0x04);
}

// vstart whose byte offset is not aligned to the VRF word (ELENB = 8 with RVD),
// on a vl that spans all FUs: vl=16 e32 = 64 bytes = 2 VRF rows of 4 FUs x 8 B.
// The elements before vstart must keep the guard value. A hang here (rather
// than a wrong value) points at the VLSU commit catch-up logic.
static void vle32_unaligned_vstart(int casenum, unsigned long vstart) {
  volatile uint32_t MEM[16] __attribute__((aligned(64)));
  for (int i = 0; i < 16; i++) MEM[i] = i + 1;

  // LMUL=2 so VLMAX >= 16 also with VLEN=256 (with m1, vl would be cut to 8)
  VSET(16, e32, m2);
  asm volatile("vmv.v.x v2, %0" :: "r"(0xdeadbeef) : "v2", "v3");
  asm volatile(
      "csrw    vstart, %1\n\t"
      "vle32.v v2, (%0)"
      ::"r"(MEM), "r"(vstart) : "memory", "v2", "v3");
  asm volatile("csrw vstart, 0");

  if (vstart == 1) {
    VCMP_U32(casenum, v2, 0xdeadbeef, 2, 3, 4, 5, 6, 7, 8,
                          9, 10, 11, 12, 13, 14, 15, 16);
  } else { // vstart == 5: byte 20, in the middle of FU 2's word
    VCMP_U32(casenum, v2, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
                          0xdeadbeef, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  }
}

// vstart = 1 -> byte 4, middle of FU 0's word
void TEST_CASE12(void) { vle32_unaligned_vstart(12, 1); }

// vstart = 5 -> byte 20, middle of FU 2's word
void TEST_CASE13(void) { vle32_unaligned_vstart(13, 5); }

// Same as above, but vl=32 e32 = 128 B. With the double-bandwidth VLSU the
// vector is split 64 B / 64 B between the two interfaces, so each interface
// spans 2 VRF rows (4 FUs x 8 B) and its FUs below the starting FU are a full
// row ahead. On the standard VLSU this covers 4 rows on one interface.
// LMUL=4 so VLMAX >= 32 also with VLEN=256.
static void vle32_unaligned_vstart_long(int casenum, unsigned long vstart) {
  // MEM is reused for the read-back to keep the frame small: main's frame plus
  // this one plus printf must fit in the 1 KiB per-core stack
  volatile uint32_t MEM[32] __attribute__((aligned(64)));
  volatile uint32_t *RES = MEM;
  for (int i = 0; i < 32; i++) MEM[i] = i + 1;

  VSET(32, e32, m4);
  asm volatile("vmv.v.x v4, %0" :: "r"(0xdeadbeef) : "v4", "v5", "v6", "v7");
  asm volatile(
      "csrw    vstart, %1\n\t"
      "vle32.v v4, (%0)"
      ::"r"(MEM), "r"(vstart) : "memory", "v4", "v5", "v6", "v7");
  asm volatile("csrw vstart, 0");
  asm volatile("vse32.v v4, (%0)" ::"r"(RES) : "memory");

  for (int i = 0; i < 32; i++) {
    uint32_t exp = (i < (int)vstart) ? 0xdeadbeef : (uint32_t)(i + 1);
    if (RES[i] != exp) {
      printf("[TC %d] Index %d FAILED. Got \"%u\", expected \"%u\".\n", casenum, i, RES[i], exp);
      num_failed++;
      return;
    }
  }
  printf("[TC %d] PASSED.\n", casenum);
}

// vstart = 5 -> byte 20: interface 0, middle of FU 2's word, FUs 0-1 a row ahead
void TEST_CASE14(void) { vle32_unaligned_vstart_long(14, 5); }

// vstart = 21 -> byte 84: interface 1 (local byte 20), same pattern there;
// interface 0 has nothing to do
void TEST_CASE15(void) { vle32_unaligned_vstart_long(15, 21); }

// vstart = 9 -> byte 36: interface 0 row 1 (local byte 36, middle of FU 0's
// word), so interface 0 writes 1 row while interface 1 writes 2. After the
// first paired write, interface 1 must be able to write its second row alone.
void TEST_CASE16(void) { vle32_unaligned_vstart_long(16, 9); }

// Same layout as the vl=32 tests above, but the destination is consumed by a
// VFU instruction issued right after the load (RAW through the scoreboard).
// With the double-bandwidth VLSU, vstart=21 makes interface 1 write alone and
// vstart=9 makes its second row go out alone; the vadd must still wait for
// those writes. Expected: 2*0xdeadbeef (mod 2^32) below vstart, 2*(i+1) above.
static void vle32_vstart_raw(int casenum, unsigned long vstart) {
  volatile uint32_t MEM[32] __attribute__((aligned(64)));
  volatile uint32_t *RES = MEM; // reused for the read-back (stack space)
  for (int i = 0; i < 32; i++) MEM[i] = i + 1;

  VSET(32, e32, m4);
  asm volatile("vmv.v.x v4, %0\n\t"
               "vmv.v.i v8, 0"
               :: "r"(0xdeadbeef) : "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11");
  asm volatile(
      "csrw    vstart, %1\n\t"
      "vle32.v v4, (%0)\n\t"
      "vadd.vv v8, v4, v4"
      ::"r"(MEM), "r"(vstart) : "memory", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11");
  asm volatile("csrw vstart, 0");
  asm volatile("vse32.v v8, (%0)" ::"r"(RES) : "memory");

  for (int i = 0; i < 32; i++) {
    uint32_t exp = (i < (int)vstart) ? (uint32_t)(0xdeadbeefu * 2u) : (uint32_t)(2 * (i + 1));
    if (RES[i] != exp) {
      printf("[TC %d] Index %d FAILED. Got \"%u\", expected \"%u\".\n", casenum, i, RES[i], exp);
      num_failed++;
      return;
    }
  }
  printf("[TC %d] PASSED.\n", casenum);
}

// vstart = 21 -> byte 84: only interface 1 writes (double-bandwidth VLSU)
void TEST_CASE17(void) { vle32_vstart_raw(17, 21); }

// vstart = 9 -> byte 36: interface 1's second row is written alone
void TEST_CASE18(void) { vle32_vstart_raw(18, 9); }

// Single-row load with vstart in FU 2: vl=8 e32 = one VRF row. With vstart=5
// FUs 0 and 1 have no work at all. Load counterpart of vse32 TEST_CASE12.
static void vle32_single_row_vstart(int casenum, unsigned long vstart) {
  volatile uint32_t MEM[8] __attribute__((aligned(64)));
  for (int i = 0; i < 8; i++) MEM[i] = i + 1;

  VSET(8, e32, m1);
  asm volatile("vmv.v.x v2, %0" :: "r"(0xdeadbeef) : "v2");
  asm volatile(
      "csrw    vstart, %1\n\t"
      "vle32.v v2, (%0)"
      ::"r"(MEM), "r"(vstart) : "memory", "v2");
  asm volatile("csrw vstart, 0");
  asm volatile("vse32.v v2, (%0)" ::"r"(MEM) : "memory");

  for (int i = 0; i < 8; i++) {
    uint32_t exp = (i < (int)vstart) ? 0xdeadbeef : (uint32_t)(i + 1);
    if (MEM[i] != exp) {
      printf("[TC %d] Index %d FAILED. Got \"%u\", expected \"%u\".\n", casenum, i, MEM[i], exp);
      num_failed++;
      return;
    }
  }
  printf("[TC %d] PASSED.\n", casenum);
}

// vstart = 5 -> byte 20: FUs 0-1 idle, FU 2 mid-word, FU 3 catches up
void TEST_CASE19(void) { vle32_single_row_vstart(19, 5); }

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
  TEST_CASE14();
  TEST_CASE15();
  TEST_CASE16();
  TEST_CASE17();
  TEST_CASE18();
  TEST_CASE19();

  EXIT_CHECK();
}