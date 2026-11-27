// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Navaneeth Kunhi Purayil <nkunhi@iis.ee.ethz.ch>

#include "vector_macros.h"

static inline uint32_t get_vlenb_hw(void) {
  uint32_t vl;
  asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(1024));
  return vl;                       // 32 on smallvrf, 64 on standard
}

void TEST_CASE1(void) {
  uint32_t vlenb = get_vlenb_hw();
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
  if (vlenb == 32) {
    VCMP_U8(2, v5, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(2, v5, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }
}

void TEST_CASE2(void) {
  uint32_t vlenb = get_vlenb_hw();
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
  if (vlenb == 32) {
    VCMP_U8(4, v9, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(4, v9, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }
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
  uint32_t vlenb = get_vlenb_hw();
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
  if (get_vlenb_hw() == 32) {
    VCMP_U8(7, v15, 223, 222, 221, 220, 219, 218, 217, 216, 215, 214, 213, 212);
  } else {
    VCMP_U8(7, v15, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 180);
  }
}

void TEST_CASE5(void) {
  uint32_t vlenb = get_vlenb_hw();
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
  if (get_vlenb_hw() == 32) {
    VCMP_U8(9, v5, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107);
  } else {
    VCMP_U8(9, v5, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139);
  }
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
  uint32_t vlenb = get_vlenb_hw();
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
  if (vlenb == 32) {
    VCMP_U8(13, v13, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(13, v13, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }
}

// Write a reserved vsew into vtype: per spec this sets vill=1 and vl=0
static inline void set_vill(void) {
  unsigned long vtype = (4 << 3) | 0; // vsew=4 is reserved -> vill
  unsigned long avl   = 16;
  asm volatile("vsetvl zero, %0, %1" :: "r"(avl), "r"(vtype));
}

// Whole-register store must execute when vill=1 (and therefore vl=0)
void TEST_CASE8(void) {
  uint32_t vlenb = get_vlenb_hw();
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  volatile uint8_t OUT[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) { INP[i] = 200 - i; OUT[i] = 170; }

  asm volatile("vl2re8.v v20, (%0)" :: "r"(INP) : "memory", "v20", "v21");

  set_vill();
  unsigned long vl_csr, vtype_csr;
  asm volatile("csrr %0, vl"    : "=r"(vl_csr));
  asm volatile("csrr %0, vtype" : "=r"(vtype_csr));
  asm volatile("vs2r.v v20, (%0)" :: "r"(OUT) : "memory");

  // Sanity: the scenario was really vill=1, vl=0
  const unsigned long exp_csr[] = {0, 1};
  VCMP(unsigned long, "%lu", 14, exp_csr, vl_csr,
       (vtype_csr >> (8 * sizeof(unsigned long) - 1)) & 1);

  // Restore a legal vtype and read OUT back
  VSET(16, e8, m1);
  asm volatile("vl2re8.v v24, (%0)" :: "r"(OUT) : "memory", "v24", "v25");

  VCMP_U8(15, v24, 200, 199, 198, 197, 196, 195, 194, 193, 192, 191, 190, 189, 188, 187, 186, 185);
  if (vlenb == 32) {
    VCMP_U8(16, v25, 168, 167, 166, 165, 164, 163, 162, 161, 160, 159, 158, 157, 156, 155, 154, 153);
  } else {
    VCMP_U8(16, v25, 136, 135, 134, 133, 132, 131, 130, 129, 128, 127, 126, 125, 124, 123, 122, 121);
  }
}

// Whole-register store must execute with a legal vtype but vl=0
void TEST_CASE9(void) {
  uint32_t vlenb = get_vlenb_hw();
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  volatile uint8_t OUT[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) { INP[i] = i + 3; OUT[i] = 170; }

  asm volatile("vl2re8.v v20, (%0)" :: "r"(INP) : "memory", "v20", "v21");

  VSET(0, e8, m1);
  unsigned long vl_csr;
  asm volatile("csrr %0, vl" : "=r"(vl_csr));
  asm volatile("vs2r.v v20, (%0)" :: "r"(OUT) : "memory");

  const unsigned long exp_vl[] = {0};
  VCMP(unsigned long, "%lu", 17, exp_vl, vl_csr);

  VSET(16, e8, m1);
  asm volatile("vl2re8.v v24, (%0)" :: "r"(OUT) : "memory", "v24", "v25");

  VCMP_U8(18, v24, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18);
  if (vlenb == 32) {
    VCMP_U8(19, v25, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50);
  } else {
    VCMP_U8(19, v25, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82);
  }
}

// Context-switch style save/restore entirely under vill=1:
// vs2r.v then vl2re8.v into a different register, back to back (RAW in memory)
void TEST_CASE10(void) {
  uint32_t vlenb = get_vlenb_hw();
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  volatile uint8_t OUT[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) { INP[i] = i ^ 0x5A; OUT[i] = 0; }

  VSET(16, e8, m1);
  asm volatile("vl2re8.v v20, (%0)" :: "r"(INP) : "memory", "v20", "v21");
  // Guard the restore target so a dropped load is visible
  VSETMAX(e8, m2);
  asm volatile("vmv.v.x v26, %0" :: "r"(170) : "v26", "v27");

  set_vill();
  asm volatile(
    "vs2r.v   v20, (%0)\n\t"
    "vl2re8.v v26, (%0)"
    :: "r"(OUT) : "memory", "v26", "v27"
  );

  VSET(16, e8, m1);
  VCMP_U8(20, v26, 0x5A, 0x5B, 0x58, 0x59, 0x5E, 0x5F, 0x5C, 0x5D,
                   0x52, 0x53, 0x50, 0x51, 0x56, 0x57, 0x54, 0x55);
  if (vlenb == 32) {
    // 32..47 ^ 0x5A
    VCMP_U8(21, v27, 0x7A, 0x7B, 0x78, 0x79, 0x7E, 0x7F, 0x7C, 0x7D,
                     0x72, 0x73, 0x70, 0x71, 0x76, 0x77, 0x74, 0x75);
  } else {
    // 64..79 ^ 0x5A
    VCMP_U8(21, v27, 0x1A, 0x1B, 0x18, 0x19, 0x1E, 0x1F, 0x1C, 0x1D,
                     0x12, 0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15);
  }
}

// Compare 16 bytes against the pattern (i * 7 + 1) starting at byte index `start`
static void check_pattern(int casenum, const volatile uint8_t *got, uint32_t start) {
  for (int k = 0; k < 16; k++) {
    uint8_t exp = (uint8_t)((start + k) * 7 + 1);
    if (got[k] != exp) {
      printf("[TC %d] Index %d FAILED. Got %hhu, expected %hhu.\n", casenum, k, got[k], exp);
      num_failed++;
      return;
    }
  }
  printf("[TC %d] PASSED.\n", casenum);
}

// WAR hazard across units: a vs8r.v issued with vill=1 (vl=0) must still be
// tracked by the scoreboard, so a VFU and an SLD instruction that overwrite
// its source registers right after it must wait until the store has read them.
// v15/v14 are the last registers read by the store (widest window).
void TEST_CASE11(void) {
  uint32_t vlenb = get_vlenb_hw();
  // Single buffer (source, then destination): the per-core stack is only 1 KiB,
  // and separate INP/OUT arrays overflow into the next core's TLS.
  volatile uint8_t BUF[8 * 64] __attribute__((aligned(64)));
  volatile uint8_t RES[16] __attribute__((aligned(16)));
  for (int i = 0; i < 8 * 64; i++) BUF[i] = (uint8_t)(i * 7 + 1);

  // Source group v8..v15 with the pattern; v24 = zeros for the slide
  VSET(16, e8, m1);
  asm volatile(
    "vl8re8.v v8, (%0)\n\t"
    "vmv.v.i  v24, 0"
    :: "r"(BUF) : "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v24");

  // Overwrite the buffer with a guard so only the store under test can restore
  // the pattern. Done with a vector store (not a scalar loop) so it is ordered
  // after the vl8re8.v inside the VLSU and cannot race the load.
  VSETMAX(e8, m8);
  asm volatile(
    "vmv.v.x v16, %0\n\t"
    "vs8r.v  v16, (%1)"
    :: "r"(170), "r"(BUF) : "memory", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23");
  volatile uint8_t *OUT = BUF;

  unsigned long vtype_ill = (4 << 3) | 0; // reserved vsew -> vill=1, vl=0
  unsigned long avl       = 16;
  unsigned long vl_tmp;
  asm volatile(
    "vsetvl   zero, %[avl], %[vill]\n\t"
    "vs8r.v   v8, (%[out])\n\t"
    "vsetvli  %[tmp], %[avl], e8, m1, ta, ma\n\t"
    "vmv.v.i  v15, 0\n\t"               // VFU overwrites v15
    "vslidedown.vi v14, v24, 0"         // SLD overwrites v14
    : [tmp] "=&r"(vl_tmp)
    : [avl] "r"(avl), [vill] "r"(vtype_ill), [out] "r"(OUT)
    : "memory", "v14", "v15");

  // Read back the parts of OUT that came from v15 and v14
  VSET(16, e8, m1);
  asm volatile("vl1re8.v v26, (%0)\n\t"
               "vse8.v   v26, (%1)"
               :: "r"(OUT + 7 * vlenb), "r"(RES) : "memory", "v26");
  check_pattern(22, RES, 7 * vlenb);

  asm volatile("vl1re8.v v26, (%0)\n\t"
               "vse8.v   v26, (%1)"
               :: "r"(OUT + 6 * vlenb), "r"(RES) : "memory", "v26");
  check_pattern(23, RES, 6 * vlenb);
}

// vstart >= evl (evl = 2 * vlenb for vs2r.v) must be a no-op: OUT, including
// a guard region past evl, must stay untouched, and the store must still
// complete. The vmv.v.i overwrites vs3 right after the store (WAR).
static void vsr_no_work(int casenum, unsigned long vstart) {
  uint32_t vlenb = get_vlenb_hw();
  volatile uint8_t OUT[3 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 3 * 64; i++) OUT[i] = 170;

  VSET(16, e8, m1);
  asm volatile("vmv.v.i v8, 1\n\t"
               "vmv.v.i v9, 2" ::: "v8", "v9");
  asm volatile(
      "csrw    vstart, %1\n\t"
      "vs2r.v  v8, (%0)\n\t"
      "vmv.v.i v8, 0"
      ::"r"(OUT), "r"(vstart) : "memory", "v8");
  asm volatile("csrw vstart, 0");

  // Read back through the VLSU: start of v8's region, start of v9's region,
  // and the guard just past evl
  asm volatile("vl1re8.v v24, (%0)\n\t"
               "vl1re8.v v25, (%1)\n\t"
               "vl1re8.v v26, (%2)"
               ::"r"(OUT), "r"(OUT + vlenb), "r"(OUT + 2 * vlenb)
               : "memory", "v24", "v25", "v26");
  VCMP_U8(casenum, v24, 170, 170, 170, 170, 170, 170, 170, 170,
                        170, 170, 170, 170, 170, 170, 170, 170);
  VCMP_U8(casenum + 1, v25, 170, 170, 170, 170, 170, 170, 170, 170,
                            170, 170, 170, 170, 170, 170, 170, 170);
  VCMP_U8(casenum + 2, v26, 170, 170, 170, 170, 170, 170, 170, 170,
                            170, 170, 170, 170, 170, 170, 170, 170);
}

// vstart == evl
void TEST_CASE12(void) { vsr_no_work(24, 2 * get_vlenb_hw()); }

// vstart > evl: counters start past their limit
void TEST_CASE13(void) { vsr_no_work(27, 2 * get_vlenb_hw() + 3); }

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