// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// msetmtype — Register matrix-type configuration (analogous to vsetvl)
//
// -------------------------------------------------------
// Test cases:
//   TC1: mtype=0x03 (mtwiden=3), vsew=e8
//   TC2: mtype=0x02 (mtwiden=2), vsew=e16
//   TC3: mtype=0x01 (mtwiden=1), vsew=e32
//   TC4: mtype=0x00 (mtwiden=0, no widening)
//   TC5: Overwrite — second write must override first
//   TC6: vl always reset to 0 after msetmtype
//   TC7: msetmtype vs msetmtypei equivalence

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// -------------------------------------------------------
// TC1: mtype=0x03 (mtwiden=3), vsew=e8
// -------------------------------------------------------
void TEST_CASE1(void) {
    uintptr_t mtype_val, vtype_val, vl_val;
    uintptr_t new_mtype = MTYPE_VAL(3);
    uintptr_t new_vtype = VME_VTYPE(0);

    msetmtype(new_mtype, new_vtype);

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    read_vl(vl_val);

    XCMP(1, mtype_val, new_mtype);
    XCMP(2, vtype_val, new_vtype);
    XCMP(3, vl_val,    0);
}

// -------------------------------------------------------
// TC2: mtype=0x02 (mtwiden=2), vsew=e16
// -------------------------------------------------------
void TEST_CASE2(void) {
    uintptr_t mtype_val, vtype_val, vl_val;
    uintptr_t new_mtype = MTYPE_VAL(2);
    uintptr_t new_vtype = VME_VTYPE(1);

    msetmtype(new_mtype, new_vtype);

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    read_vl(vl_val);

    XCMP(4, mtype_val, new_mtype);
    XCMP(5, vtype_val, new_vtype);
    XCMP(6, vl_val,    0);
}

// -------------------------------------------------------
// TC3: mtype=0x01 (mtwiden=1), vsew=e32
// -------------------------------------------------------
void TEST_CASE3(void) {
    uintptr_t mtype_val, vtype_val, vl_val;
    uintptr_t new_mtype = MTYPE_VAL(1);
    uintptr_t new_vtype = VME_VTYPE(2);

    msetmtype(new_mtype, new_vtype);

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    read_vl(vl_val);

    XCMP(7,  mtype_val, new_mtype);
    XCMP(8,  vtype_val, new_vtype);
    XCMP(9,  vl_val,    0);
}

// -------------------------------------------------------
// TC4: mtype=0x00 (mtwiden=0, no widening), vsew=e8
// -------------------------------------------------------
void TEST_CASE4(void) {
    uintptr_t mtype_val, vtype_val, vl_val;
    uintptr_t new_mtype = MTYPE_VAL(0);
    uintptr_t new_vtype = STD_VTYPE(0);

    msetmtype(new_mtype, new_vtype);

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    read_vl(vl_val);

    XCMP(10, mtype_val, new_mtype);
    XCMP(11, vtype_val, new_vtype);
    XCMP(12, vl_val,    0);
}

// -------------------------------------------------------
// TC5: Overwrite — second write must override first completely
//   First:  mtype=0x02, vtype=e16
//   Second: mtype=0x03, vtype=e8
//   CSR must reflect SECOND write only
// -------------------------------------------------------
void TEST_CASE5(void) {
    uintptr_t mtype_val, vtype_val;

    // First write
    uintptr_t mtype_a = MTYPE_VAL(2);
    uintptr_t vtype_a = VME_VTYPE(1);
    msetmtype(mtype_a, vtype_a);
    read_mtype(mtype_val);
    XCMP(13, mtype_val, mtype_a);   // intermediate check

    // Second write — must overwrite entirely
    uintptr_t mtype_b = MTYPE_VAL(3);
    uintptr_t vtype_b = VME_VTYPE(0);
    msetmtype(mtype_b, vtype_b);

    read_mtype(mtype_val);
    read_vtype(vtype_val);
    XCMP(14, mtype_val, mtype_b);
    XCMP(15, vtype_val, vtype_b);
}

// -------------------------------------------------------
// TC6: vl always reset to 0 after msetmtype
//   Set vl to non-zero via vsetvli, then msetmtype,
//   vl must become 0.
// -------------------------------------------------------
void TEST_CASE6(void) {
    uintptr_t vl_val;
    uintptr_t avl = 8;

    // Set vl to 8 using standard vsetvli
    asm volatile("vsetvli x0, %[avl], e8, m1, ta, ma"
                 :: [avl] "r"(avl) : "memory");
    read_vl(vl_val);
    XCMP(16, vl_val, 8);   // confirm vl=8 before msetmtype

    // msetmtype must reset vl to 0
    uintptr_t new_mtype = MTYPE_VAL(3);
    uintptr_t new_vtype = VME_VTYPE(0);
    msetmtype(new_mtype, new_vtype);
    read_vl(vl_val);
    XCMP(17, vl_val, 0);
}

// -------------------------------------------------------
// TC7: msetmtype vs msetmtypei equivalence
//   Both must produce identical mtype and vtype CSR values.
//   Note: msetmtypei builds vtype internally;
//         msetmtype uses the rs1 value directly —
//         so rs1 must match what msetmtypei would compute.
// -------------------------------------------------------
void TEST_CASE7(void) {
    uintptr_t mtype_imm, vtype_imm;
    uintptr_t mtype_reg, vtype_reg;

    // --- (mtwiden=3, vsew=e8) ---
    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    read_mtype(mtype_imm);
    read_vtype(vtype_imm);

    uintptr_t m3 = MTYPE_VAL(3);
    uintptr_t v0 = VME_VTYPE(0);
    asm volatile("msetmtype %[rs2], %[rs1]"
                 :: [rs2] "r"(m3), [rs1] "r"(v0) : "memory");
    read_mtype(mtype_reg);
    read_vtype(vtype_reg);

    XCMP(18, mtype_reg, mtype_imm);
    XCMP(19, vtype_reg, vtype_imm);

    // --- (mtwiden=2, vsew=e16) ---
    asm volatile("msetmtypei 0x2, 0x1" ::: "memory");
    read_mtype(mtype_imm);
    read_vtype(vtype_imm);

    uintptr_t m2 = MTYPE_VAL(2);
    uintptr_t v1 = VME_VTYPE(1);
    asm volatile("msetmtype %[rs2], %[rs1]"
                 :: [rs2] "r"(m2), [rs1] "r"(v1) : "memory");
    read_mtype(mtype_reg);
    read_vtype(vtype_reg);

    XCMP(20, mtype_reg, mtype_imm);
    XCMP(21, vtype_reg, vtype_imm);

    // --- (mtwiden=1, vsew=e32) ---
    asm volatile("msetmtypei 0x1, 0x2" ::: "memory");
    read_mtype(mtype_imm);
    read_vtype(vtype_imm);

    uintptr_t m1 = MTYPE_VAL(1);
    uintptr_t v2 = VME_VTYPE(2);
    asm volatile("msetmtype %[rs2], %[rs1]"
                 :: [rs2] "r"(m1), [rs1] "r"(v2) : "memory");
    read_mtype(mtype_reg);
    read_vtype(vtype_reg);

    XCMP(22, mtype_reg, mtype_imm);
    XCMP(23, vtype_reg, vtype_imm);
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
    TEST_CASE7();

    EXIT_CHECK();
}