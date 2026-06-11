// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// msetmtypei — Immediate matrix-type configuration
//
// -------------------------------------------------------
// Test cases:
//   TC1: msetmtypei 0x3, 0x0  →  mtwiden=3, vsew=e8
//   TC2: msetmtypei 0x2, 0x1  →  mtwiden=2, vsew=e16
//   TC3: msetmtypei 0x1, 0x2  →  mtwiden=1, vsew=e32
//   TC4: msetmtypei 0x0, 0x0  →  mtwiden=0, vsew=e8  (no widening)
//   TC5: Overwrite — write twice, CSR reflects last write only
//   TC6: vl is reset to 0 after msetmtypei

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// -------------------------------------------------------
// TC1: msetmtypei 0x3, 0x0
//   uimm5=0x3  → mtwiden=3 → mtype=0x03
//   uimm2=0x0  → vsew=e8   → vtype bits[4:2]=0, bits[7:6]=1
//   Expected mtype = 0x03
//   Expected vtype = 0xC0 | (0<<2) = 0xC0
//   Expected vl    = 0
// -------------------------------------------------------
void TEST_CASE1(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    read_vl(vl_val);

    XCMP(1, mtype_val, MTYPE_EXP(0x3));
    XCMP(2, vtype_val, VTYPE_EXP(0x3, 0x0));
    XCMP(3, vl_val,    0);
}

// -------------------------------------------------------
// TC2: msetmtypei 0x2, 0x1
//   uimm5=0x2  → mtwiden=2 → mtype=0x02
//   uimm2=0x1  → vsew=e16  → vtype bits[4:2]=1, bits[7:6]=1
//   Expected mtype = 0x02
//   Expected vtype = 0xC0 | (1<<2) = 0xC4
//   Expected vl    = 0
// -------------------------------------------------------
void TEST_CASE2(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x2, 0x1" ::: "memory");

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    read_vl(vl_val);

    XCMP(4, mtype_val, MTYPE_EXP(0x2));
    XCMP(5, vtype_val, VTYPE_EXP(0x2, 0x1));
    XCMP(6, vl_val,    0);
}

// -------------------------------------------------------
// TC3: msetmtypei 0x1, 0x2
//   uimm5=0x1  → mtwiden=1 → mtype=0x01
//   uimm2=0x2  → vsew=e32  → vtype bits[4:2]=2, bits[7:6]=1
//   Expected mtype = 0x01
//   Expected vtype = 0xC0 | (2<<2) = 0xC8
//   Expected vl    = 0
// -------------------------------------------------------
void TEST_CASE3(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x1, 0x2" ::: "memory");

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    read_vl(vl_val);

    XCMP(7,  mtype_val, MTYPE_EXP(0x1));
    XCMP(8,  vtype_val, VTYPE_EXP(0x1, 0x2));
    XCMP(9,  vl_val,    0);
}

// -------------------------------------------------------
// TC4: msetmtypei 0x0, 0x0
//   uimm5=0x0  → mtwiden=0 → mtype=0x00
//   uimm2=0x0  → vsew=e8   → vtype bits[4:2]=0, bits[7:6]=0
//   Expected mtype = 0x00
//   Expected vtype = 0x00
//   Expected vl    = 0
// -------------------------------------------------------
void TEST_CASE4(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x0, 0x0" ::: "memory");

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    read_vl(vl_val);

    XCMP(10, mtype_val, MTYPE_EXP(0x0));
    XCMP(11, vtype_val, VTYPE_EXP(0x0, 0x0));
    XCMP(12, vl_val,    0);
}

// -------------------------------------------------------
// TC5: Overwrite
//   First write: mtwiden=2, vsew=e16 → mtype=0x02
//   Second write: mtwiden=3, vsew=e8 → mtype=0x03
//   CSR must reflect LAST write only
// -------------------------------------------------------
void TEST_CASE5(void) {
    uintptr_t mtype_val, vtype_val;

    // First write
    asm volatile("msetmtypei 0x2, 0x1" ::: "memory");
    read_mtype(mtype_val);
    XCMP(13, mtype_val, MTYPE_EXP(0x2));   // intermediate check

    // Second write — must overwrite completely
    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    read_mtype(mtype_val);
    read_vtype(vtype_val);
    XCMP(14, mtype_val, MTYPE_EXP(0x3));   // last write wins
    XCMP(15, vtype_val, VTYPE_EXP(0x3, 0x0));
}

// -------------------------------------------------------
// TC6: vl is always reset to 0 by msetmtypei
//   Set vl to non-zero via vsetvli, then msetmtypei,
//   vl must be 0 after.
// -------------------------------------------------------
void TEST_CASE6(void) {
    uintptr_t vl_val;
    uintptr_t avl = 8;

    // Set vl to a non-zero value using standard vsetvli
    asm volatile("vsetvli x0, %[avl], e8, m1, ta, ma"
                 :: [avl] "r"(avl) : "memory");
    read_vl(vl_val);
    XCMP(16, vl_val, 8);   // confirm vl is 8 before msetmtypei

    // msetmtypei must reset vl to 0
    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    read_vl(vl_val);
    XCMP(17, vl_val, 0);   // vl must be 0 after msetmtypei
}

int main(void) {
    INIT_CHECK();
    enable_vec();
    enable_vme();

    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
    TEST_CASE4();
    TEST_CASE5();
    TEST_CASE6();

    EXIT_CHECK();
}