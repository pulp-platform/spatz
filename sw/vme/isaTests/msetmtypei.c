// Copyright 2026 ETH Zurich and University of Bologna.
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

void TEST_CASE1(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(1, mtype_val, 3u);
    XCMP(2, vtype_val, 0xC0u);
    XCMP(3, vl_val,    0);
}

void TEST_CASE2(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x2, 0x1" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(4, mtype_val, 2u);
    XCMP(5, vtype_val, 0xC8u);
    XCMP(6, vl_val,    0);
}

void TEST_CASE3(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x1, 0x2" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(7,  mtype_val, 1u);
    XCMP(8,  vtype_val, 0xD0u);
    XCMP(9,  vl_val,    0);
}

void TEST_CASE4(void) {
    uintptr_t mtype_val, vtype_val, vl_val;

    asm volatile("msetmtypei 0x0, 0x0" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(10, mtype_val, 0u);
    XCMP(11, vtype_val, 0u);
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

    asm volatile("msetmtypei 0x2, 0x1" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    XCMP(13, mtype_val, 2u);

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vtype" : [BUF]"=r"(vtype_val));
    XCMP(14, mtype_val, 3u);
    XCMP(15, vtype_val, 0xC0u);
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
    XCMP(16, vl_val, 8);

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));
    XCMP(17, vl_val, 0);
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
