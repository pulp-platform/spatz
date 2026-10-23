// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Pei-Yu Lin, ETH Zürich
//
// Test cases:
//   TC1-TC4: configure each supported MTWIDEN/VSEW combination and disable it.
//   TC5: replace prior TM, TK, and altfmt state with immediate configuration.
//   TC6: reset vl to zero after immediate configuration.

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

void TEST_CASE1(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x3, 0x0, 0" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(1, mtype_val, 3u);
    XCMP(2, vtype_val, 0xC0u);
    XCMP(3, vl_val,    0);
}

void TEST_CASE2(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x2, 0x1, 0" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(4, mtype_val, 2u);
    XCMP(5, vtype_val, 0xC8u);
    XCMP(6, vl_val,    0);
}

void TEST_CASE3(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x1, 0x2, 0" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(7,  mtype_val, 1u);
    XCMP(8,  vtype_val, 0xD0u);
    XCMP(9,  vl_val,    0);
}

void TEST_CASE4(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x0, 0x0, 0" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(10, mtype_val, 0u);
    XCMP(11, vtype_val, 0u);
    XCMP(12, vl_val,    0);
}

// Immediate configuration clears previous TM/TK and replaces altfmt.
void TEST_CASE5(void) {
    uintptr_t mtype_val, vtype_val;
    uintptr_t n = 4, k = KMAX(16, 2);

    asm volatile("msetmtypei 2, 1, 1" ::: "memory");
    asm volatile("msettm zero, %0" :: "r"(n) : "memory");
    asm volatile("msettk zero, %0" :: "r"(k) : "memory");
    asm volatile("csrr %0, 0xC23" : "=r"(mtype_val));
    asm volatile("csrr %0, vtype" : "=r"(vtype_val));
    XCMP(13, MTYPE_TM(mtype_val), n);
    XCMP(14, MTYPE_TK(mtype_val), k);
    XCMP(15, ((vtype_val >> 8) & 1), 1);

    asm volatile("msetmtypei 3, 0, 0" ::: "memory");
    asm volatile("csrr %0, 0xC23" : "=r"(mtype_val));
    asm volatile("csrr %0, vtype" : "=r"(vtype_val));
    XCMP(16, mtype_val, 3u);
    XCMP(17, ((vtype_val >> 8) & 1), 0);
}

// -------------------------------------------------------
// TC6: vl is always reset to 0 by msetmtypei
//   Set vl to non-zero via vsetvli, then msetmtypei,
//   vl must be 0 after.
// -------------------------------------------------------
void TEST_CASE6(void) {
    uintptr_t vl_val;
    uintptr_t avl = 8;

    asm volatile("vsetvli x0, %[avl], e8, m1, ta, ma" :: [avl] "r"(avl) : "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));
    XCMP(18, vl_val, 8);

    asm volatile("msetmtypei 0x3, 0x0, 0" ::: "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));
    XCMP(19, vl_val, 0);
}

int main(void) {
    INIT_CHECK();

    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
    TEST_CASE4();
    TEST_CASE5();
    TEST_CASE6();

    EXIT_CHECK();
}
