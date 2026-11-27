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
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;

  asm volatile("vl2re8.v v2, (%0)" :: "r"(INP) : "memory", "v2", "v3");

  VCMP_U8(1, v2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  if (vlenb == 32) {
    VCMP_U8(2, v3, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(2, v3, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }
}

void TEST_CASE2(void) {
  uint32_t vlenb = get_vlenb_hw();
  VSET(16, e8, m1);
  volatile uint8_t INP[4 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 4 * 64; i++) INP[i] = i;   // wraps at 256, fine for uint8_t

  asm volatile("vl4re32.v v4, (%0)" :: "r"(INP) : "memory", "v4", "v5", "v6", "v7");

  VCMP_U8(3, v4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  if (vlenb == 32) {
    VCMP_U8(4, v5, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
    VCMP_U8(5, v6, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
    VCMP_U8(6, v7, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111);
  } else {
    VCMP_U8(4, v5, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
    VCMP_U8(5, v6, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143);
    VCMP_U8(6, v7, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207);
  }
}

void TEST_CASE3(void) {
  uint32_t vlenb = get_vlenb_hw();
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
  if (vlenb == 32) {
    VCMP_U8(8, v5, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(8, v5, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }

  // Check the store worked (by verifying the load-back registers)
  VCMP_U8(9, v6, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  if (vlenb == 32) {
    VCMP_U8(10, v7, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(10, v7, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }
}

void TEST_CASE4(void) {
  uint32_t vlenb = get_vlenb_hw();
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;

  // e32 instruction should move the exact same 128 bytes into v8 and v9
  asm volatile("vl2re32.v v8, (%0)" :: "r"(INP) : "memory", "v8", "v9");

  VCMP_U8(11, v8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  if (vlenb == 32) {
    VCMP_U8(12, v9, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(12, v9, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }
}

void TEST_CASE5(void) {
  uint32_t vlenb = get_vlenb_hw();
  VSET(1, e32, m8);  // Set a tiny VL and odd vtype
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;

  asm volatile("vl2re8.v v10, (%0)" :: "r"(INP) : "memory", "v10", "v11");

  VSET(16, e8, m1);
  // Even with vl=1, whole register load must move all 64 bytes per register
  VCMP_U8(13, v10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  if (vlenb == 32) {
    VCMP_U8(14, v11, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(14, v11, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }
}

void TEST_CASE6(void) {
  uint32_t vlenb = get_vlenb_hw();
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = 255 - i; // reverse to differentiate

  asm volatile("vl2re8.v v30, (%0)" :: "r"(INP) : "memory", "v30", "v31");

  VCMP_U8(15, v30, 255, 254, 253, 252, 251, 250, 249, 248, 247, 246, 245, 244);
  if (get_vlenb_hw() == 32) {
    VCMP_U8(16, v31, 223, 222, 221, 220, 219, 218, 217, 216, 215, 214, 213, 212);
  } else {
    VCMP_U8(16, v31, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 180);
  }
}

void TEST_CASE7(void) {
  uint32_t vlenb = get_vlenb_hw();
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
  if (vlenb == 32) {
    VCMP_U8(18, v15, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47);
  } else {
    VCMP_U8(18, v15, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79);
  }
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
  uint32_t vlenb = get_vlenb_hw();
  VSET(16, e8, m1);
  volatile uint8_t INP[2 * 64 + 4] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64 + 4; i++) INP[i] = i; // 0..131

  // Load from an unaligned offset (+1 byte)
  asm volatile("vl2re8.v v16, (%0)" :: "r"(INP + 1) : "memory", "v16", "v17");

  // Everything shifts by +1
  VCMP_U8(23, v16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  if (get_vlenb_hw() == 32) {
    VCMP_U8(24, v17, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48);
  } else {
    VCMP_U8(24, v17, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80);
  }
}

// Write a reserved vsew into vtype: per spec this sets vill=1 and vl=0
static inline void set_vill(void) {
  unsigned long vtype = (4 << 3) | 0; // vsew=4 is reserved -> vill
  unsigned long avl   = 16;
  asm volatile("vsetvl zero, %0, %1" :: "r"(avl), "r"(vtype));
}

// Fill v20..v21 entirely with a guard byte so a dropped load is visible
static inline void fill_v20_v21(uint8_t val) {
  VSETMAX(e8, m2);
  asm volatile("vmv.v.x v20, %0" :: "r"(val) : "v20", "v21");
}

// Whole-register load must execute when vill=1 (and therefore vl=0)
void TEST_CASE10(void) {
  uint32_t vlenb = get_vlenb_hw();
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = 200 - i;

  fill_v20_v21(170);

  set_vill();
  unsigned long vl_csr, vtype_csr;
  asm volatile("csrr %0, vl"    : "=r"(vl_csr));
  asm volatile("csrr %0, vtype" : "=r"(vtype_csr));
  asm volatile("vl2re8.v v20, (%0)" :: "r"(INP) : "memory", "v20", "v21");

  // Sanity: the scenario was really vill=1, vl=0
  const unsigned long exp_csr[] = {0, 1};
  VCMP(unsigned long, "%lu", 25, exp_csr, vl_csr,
       (vtype_csr >> (8 * sizeof(unsigned long) - 1)) & 1);

  // Restore a legal vtype; VCMP_U8 consumes v20/v21 right away (RAW on the load)
  VSET(16, e8, m1);
  VCMP_U8(26, v20, 200, 199, 198, 197, 196, 195, 194, 193, 192, 191, 190, 189, 188, 187, 186, 185);
  if (vlenb == 32) {
    VCMP_U8(27, v21, 168, 167, 166, 165, 164, 163, 162, 161, 160, 159, 158, 157, 156, 155, 154, 153);
  } else {
    VCMP_U8(27, v21, 136, 135, 134, 133, 132, 131, 130, 129, 128, 127, 126, 125, 124, 123, 122, 121);
  }
}

// Whole-register load must execute with a legal vtype but vl=0
void TEST_CASE11(void) {
  uint32_t vlenb = get_vlenb_hw();
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i + 3;

  fill_v20_v21(170);

  VSET(0, e8, m1);
  unsigned long vl_csr;
  asm volatile("csrr %0, vl" : "=r"(vl_csr));
  asm volatile("vl2re8.v v20, (%0)" :: "r"(INP) : "memory", "v20", "v21");

  const unsigned long exp_vl[] = {0};
  VCMP(unsigned long, "%lu", 28, exp_vl, vl_csr);

  VSET(16, e8, m1);
  VCMP_U8(29, v20, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18);
  if (vlenb == 32) {
    VCMP_U8(30, v21, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50);
  } else {
    VCMP_U8(30, v21, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82);
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

// RAW hazard across units: a vl4re8.v issued with vill=1 (vl=0) must still be
// tracked by the scoreboard, so a VFU and an SLD consumer issued right after
// it must wait for the load. v15 is the last register written by the load,
// which gives the widest window for an untracked read to see stale data.
// Kept to 4 registers (256 B buffer) so the frame plus the printf call chain
// fits in the 1 KiB per-core stack.
void TEST_CASE12(void) {
  uint32_t vlenb = get_vlenb_hw();
  volatile uint8_t INP[4 * 64] __attribute__((aligned(64)));
  volatile uint8_t RES_VFU[16] __attribute__((aligned(16)));
  volatile uint8_t RES_SLD[16] __attribute__((aligned(16)));
  for (int i = 0; i < 4 * 64; i++) INP[i] = (uint8_t)(i * 7 + 1);

  // Guard the whole destination group
  VSETMAX(e8, m4);
  asm volatile("vmv.v.x v12, %0" :: "r"(170) : "v12", "v13", "v14", "v15");

  unsigned long vtype_ill = (4 << 3) | 0; // reserved vsew -> vill=1, vl=0
  unsigned long avl       = 16;
  unsigned long vl_tmp;
  asm volatile(
    "vsetvl   zero, %[avl], %[vill]\n\t"
    "vl4re8.v v12, (%[inp])\n\t"
    "vsetvli  %[tmp], %[avl], e8, m1, ta, ma\n\t"
    "vor.vv   v24, v15, v15\n\t"        // VFU reads v15
    "vslidedown.vi v26, v15, 0\n\t"     // SLD reads v15
    "vse8.v   v24, (%[rv])\n\t"
    "vse8.v   v26, (%[rs])"
    : [tmp] "=&r"(vl_tmp)
    : [avl] "r"(avl), [vill] "r"(vtype_ill), [inp] "r"(INP),
      [rv] "r"(RES_VFU), [rs] "r"(RES_SLD)
    : "memory", "v12", "v13", "v14", "v15", "v24", "v26");

  // v15 holds bytes [3*vlenb, 4*vlenb) of INP
  check_pattern(31, RES_VFU, 3 * vlenb);
  check_pattern(32, RES_SLD, 3 * vlenb);
}

// vstart >= evl (evl = 2 * vlenb for vl2re8.v) must be a no-op: v20/v21 keep
// the guard, and the load must still complete. The vor.vv depends on v21, so
// it waits for the load in the scoreboard and would hang if the VLSU never
// sent its response (a no-work load has no VRF write to carry it).
static void vlre_no_work(int casenum, unsigned long vstart) {
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;

  fill_v20_v21(170);
  VSET(16, e8, m1);
  asm volatile("vmv.v.i v22, 0" ::: "v22");
  asm volatile(
      "csrw     vstart, %1\n\t"
      "vl2re8.v v20, (%0)\n\t"
      "vor.vv   v22, v21, v21"
      ::"r"(INP), "r"(vstart) : "memory", "v20", "v21", "v22");
  asm volatile("csrw vstart, 0");

  VCMP_U8(casenum, v22, 170, 170, 170, 170, 170, 170, 170, 170,
                        170, 170, 170, 170, 170, 170, 170, 170);
  VCMP_U8(casenum + 1, v20, 170, 170, 170, 170, 170, 170, 170, 170,
                            170, 170, 170, 170, 170, 170, 170, 170);
}

// vstart == evl
void TEST_CASE13(void) { vlre_no_work(33, 2 * get_vlenb_hw()); }

// vstart > evl: counters start past their limit
void TEST_CASE14(void) { vlre_no_work(35, 2 * get_vlenb_hw() + 3); }

// vstart counts EEW elements for vl<nf>re<eew>.v, not bytes:
// vl1re32.v with vstart=1 must skip element 0 (bytes 0..3) and resume at byte 4.
// v21 is outside the 1-register group and must stay untouched.
void TEST_CASE15(void) {
  volatile uint8_t INP[64] __attribute__((aligned(64)));
  for (int i = 0; i < 64; i++) INP[i] = i + 10;

  fill_v20_v21(170);
  VSET(16, e8, m1);
  asm volatile(
      "csrw      vstart, %1\n\t"
      "vl1re32.v v20, (%0)"
      ::"r"(INP), "r"(1) : "memory", "v20");
  asm volatile("csrw vstart, 0");

  VCMP_U8(37, v20, 170, 170, 170, 170, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25);
  VCMP_U8(38, v21, 170, 170, 170, 170, 170, 170, 170, 170,
                   170, 170, 170, 170, 170, 170, 170, 170);
}

// vstart == evl in EEW elements (evl = 2 * vlenb / 4 for vl2re32.v) must be a
// no-op. Read as a byte offset it would only be half of v20, so the load would
// wrongly write the rest of v20 and all of v21. The vor.vv depends on v21 and
// checks that the no-work load still completes.
void TEST_CASE16(void) {
  uint32_t vlenb = get_vlenb_hw();
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 2 * 64; i++) INP[i] = i;

  fill_v20_v21(170);
  VSET(16, e8, m1);
  asm volatile("vmv.v.i v22, 0" ::: "v22");
  asm volatile(
      "csrw      vstart, %1\n\t"
      "vl2re32.v v20, (%0)\n\t"
      "vor.vv    v22, v21, v21"
      ::"r"(INP), "r"((unsigned long)(2 * vlenb / 4)) : "memory", "v20", "v21", "v22");
  asm volatile("csrw vstart, 0");

  VCMP_U8(39, v22, 170, 170, 170, 170, 170, 170, 170, 170,
                   170, 170, 170, 170, 170, 170, 170, 170);
  VCMP_U8(40, v21, 170, 170, 170, 170, 170, 170, 170, 170,
                   170, 170, 170, 170, 170, 170, 170, 170);
}

// Whole-register load with EEW=8 and a vstart not aligned to the VRF word
// (ELENB = 8 with RVD). vstart counts bytes here, so the VLSU sees it unchanged.
// The whole group is read back with vs<nf>r.v: bytes below vstart keep the
// guard (170), the rest must match memory.
static void vlre8_unaligned_vstart(int casenum, int nreg, unsigned long vstart) {
  uint32_t vlenb = get_vlenb_hw();
  volatile uint8_t INP[2 * 64] __attribute__((aligned(64)));
  volatile uint8_t *RES = INP; // reused for the read-back (stack space)
  for (int i = 0; i < 2 * 64; i++) INP[i] = (uint8_t)(i * 3 + 1);

  fill_v20_v21(170);
  VSET(16, e8, m1);
  if (nreg == 1) {
    asm volatile(
        "csrw     vstart, %1\n\t"
        "vl1re8.v v20, (%0)"
        ::"r"(INP), "r"(vstart) : "memory", "v20");
    asm volatile("csrw vstart, 0");
    asm volatile("vs1r.v v20, (%0)" ::"r"(RES) : "memory");
  } else {
    asm volatile(
        "csrw     vstart, %1\n\t"
        "vl2re8.v v20, (%0)"
        ::"r"(INP), "r"(vstart) : "memory", "v20", "v21");
    asm volatile("csrw vstart, 0");
    asm volatile("vs2r.v v20, (%0)" ::"r"(RES) : "memory");
  }

  for (uint32_t i = 0; i < nreg * vlenb; i++) {
    uint8_t exp = (i < vstart) ? 170 : (uint8_t)(i * 3 + 1);
    if (RES[i] != exp) {
      printf("[TC %d] Index %u FAILED. Got \"%hhu\", expected \"%hhu\".\n", casenum, i, RES[i], exp);
      num_failed++;
      return;
    }
  }
  printf("[TC %d] PASSED.\n", casenum);
}

// vl1re8.v, vstart = 1 -> byte 1, middle of FU 0's word
void TEST_CASE17(void) { vlre8_unaligned_vstart(41, 1, 1); }

// vl2re8.v, vstart = 45 -> byte 45, row 1, middle of FU 1's word
void TEST_CASE18(void) { vlre8_unaligned_vstart(42, 2, 45); }

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

  EXIT_CHECK();
}
