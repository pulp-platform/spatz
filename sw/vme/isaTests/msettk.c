// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// msettk — Set tile K dimension (tk = inner loop / reduction dimension)
//
// -------------------------------------------------------
// Test cases:
//   TC1: mtwiden=3, request tk=4 → expect min(4, KMAX=4) = 4
//   TC2: mtwiden=3, request tk > KMAX → expect clamped to 4
//   TC3: mtwiden=3, request tk=0 → expect 0
//   TC4: mtwiden=3, request tk=1 → expect 1
//   TC5: mtwiden=0 → new_tk forced to 0
//   TC6: returned rd value matches mtype[7:5]
//   TC7: msettk does NOT change vl or tm

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// -------------------------------------------------------
// TC1: mtwiden=3, request tk=4
//   new_tk = min(4, 4) = 4
//   mtype bits[7:5] = 4 = 0b100
// -------------------------------------------------------
void TEST_CASE1(void) {
    uintptr_t mtype_val;
    uintptr_t req_tk = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettk x0, %[tk]" :: [tk]"r"(req_tk) : "memory");
    read_mtype(mtype_val);

    XCMP(1, MTYPE_TK(mtype_val), 4);
}

// -------------------------------------------------------
// TC2: mtwiden=3, request tk > KMAX
//   new_tk = min(100, 4) = 4  (clamped to KMAX)
// -------------------------------------------------------
void TEST_CASE2(void) {
    uintptr_t mtype_val;
    uintptr_t req_tk = 100;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettk x0, %[tk]" :: [tk]"r"(req_tk) : "memory");
    read_mtype(mtype_val);

    XCMP(2, MTYPE_TK(mtype_val), KMAX);
}

// -------------------------------------------------------
// TC3: mtwiden=3, request tk=0
//   new_tk = 0
// -------------------------------------------------------
void TEST_CASE3(void) {
    uintptr_t mtype_val;
    uintptr_t req_tk = 0;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettk x0, %[tk]" :: [tk]"r"(req_tk) : "memory");
    read_mtype(mtype_val);

    XCMP(3, MTYPE_TK(mtype_val), 0);
}

// -------------------------------------------------------
// TC4: mtwiden=3, request tk=1
//   new_tk = 1
//   mtype bits[7:5] = 1 = 0b001
// -------------------------------------------------------
void TEST_CASE4(void) {
    uintptr_t mtype_val;
    uintptr_t req_tk = 1;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettk x0, %[tk]" :: [tk]"r"(req_tk) : "memory");
    read_mtype(mtype_val);

    XCMP(4, MTYPE_TK(mtype_val), 1);
}

// -------------------------------------------------------
// TC5: mtwiden=0 — new_tk forced to 0
// -------------------------------------------------------
void TEST_CASE5(void) {
    uintptr_t mtype_val;
    uintptr_t req_tk = 4;

    asm volatile("msetmtypei 0x0, 0x0" ::: "memory");   // mtwiden=0
    asm volatile("msettk x0, %[tk]" :: [tk]"r"(req_tk) : "memory");
    read_mtype(mtype_val);

    XCMP(5, MTYPE_TK(mtype_val), 0);
}

// -------------------------------------------------------
// TC6: returned rd value matches mtype[7:5]
// -------------------------------------------------------
void TEST_CASE6(void) {
    uintptr_t rd_val = 0xDEAD;
    uintptr_t mtype_val;
    uintptr_t req_tk = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettk %[rd], %[tk]"
                 : [rd]"=r"(rd_val)
                 : [tk]"r"(req_tk)
                 : "memory");
    read_mtype(mtype_val);

    XCMP(6, rd_val, 4);
    XCMP(7, MTYPE_TK(mtype_val), rd_val);
}

// -------------------------------------------------------
// TC7: msettk does NOT change vl or tm
//   Set tn=4, tm=4 first, then msettk → vl and tm unchanged
// -------------------------------------------------------
void TEST_CASE7(void) {
    uintptr_t vl_before, vl_after;
    uintptr_t mtype_before, mtype_after;
    uintptr_t req_tn = 4, req_tm = 4, req_tk = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    asm volatile("msettm x0, %[tm]" :: [tm]"r"(req_tm) : "memory");

    read_vl(vl_before);
    read_mtype(mtype_before);

    asm volatile("msettk x0, %[tk]" :: [tk]"r"(req_tk) : "memory");

    read_vl(vl_after);
    read_mtype(mtype_after);

    // vl (tn) must not change
    XCMP(8,  vl_after, vl_before);

    // tm in mtype must not change
    XCMP(9,  MTYPE_TM(mtype_after), MTYPE_TM(mtype_before));

    // mtwiden must not change
    XCMP(10, MTYPE_MTWIDEN(mtype_after), MTYPE_MTWIDEN(mtype_before));

    // only tk field changed
    XCMP(11, MTYPE_TK(mtype_after), KMAX);
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