// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// msettm — Set tile M dimension (tm = number of output rows)
//
// -------------------------------------------------------
// Test cases:
//   TC1: mtwiden=3, SEW=e8, request tm=4  → 4
//   TC2: mtwiden=3, SEW=e8, request tm>ETE → clamped to ETE
//   TC3: mtwiden=3, SEW=e8, request tm=0  → 0
//   TC4: mtwiden=0 → forced to 0
//   TC5: returned rd == mtype[23:10]
//   TC6: msettm does NOT change vl

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// Discover ETE at runtime: clamp a large request and read back
static uintptr_t get_ete(void) {
    uintptr_t vl_val;
    uintptr_t large = 0xFFFFFFFF;
    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[v]" :: [v]"r"(large) : "memory");
    asm volatile("csrr %[B], vl" : [B]"=r"(vl_val));
    return vl_val;
}

// -------------------------------------------------------
// TC1: mtwiden=3, request tm=4
//   new_tm = min(4, ETE) = 4
// -------------------------------------------------------
void TEST_CASE1(void) {
    uintptr_t mtype_val;
    uintptr_t req_tm = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettm x0, %[tm]" :: [tm]"r"(req_tm) : "memory");
    read_mtype(mtype_val);

    XCMP(1, MTYPE_TM(mtype_val), 4);
}

// -------------------------------------------------------
// TC2: mtwiden=3, request tm > ETE → clamped to ETE
// -------------------------------------------------------
void TEST_CASE2(void) {
    uintptr_t mtype_val;
    uintptr_t ete = get_ete();
    uintptr_t req_tm = 0xFFFF;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettm x0, %[tm]" :: [tm]"r"(req_tm) : "memory");
    read_mtype(mtype_val);

    XCMP(2, MTYPE_TM(mtype_val), ete);
}

// -------------------------------------------------------
// TC3: mtwiden=3, request tm=0 → 0
// -------------------------------------------------------
void TEST_CASE3(void) {
    uintptr_t mtype_val;
    uintptr_t req_tm = 0;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettm x0, %[tm]" :: [tm]"r"(req_tm) : "memory");
    read_mtype(mtype_val);

    XCMP(3, MTYPE_TM(mtype_val), 0);
}

// -------------------------------------------------------
// TC4: mtwiden=0 → new_tm forced to 0
// -------------------------------------------------------
void TEST_CASE4(void) {
    uintptr_t mtype_val;
    uintptr_t req_tm = 4;

    asm volatile("msetmtypei 0x0, 0x0" ::: "memory");
    asm volatile("msettm x0, %[tm]" :: [tm]"r"(req_tm) : "memory");
    read_mtype(mtype_val);

    XCMP(4, MTYPE_TM(mtype_val), 0);
}

// -------------------------------------------------------
// TC5: returned rd == mtype[23:10]
// -------------------------------------------------------
void TEST_CASE5(void) {
    uintptr_t rd_val = 0xDEAD;
    uintptr_t mtype_val;
    uintptr_t req_tm = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettm %[rd], %[tm]"
                 : [rd]"=r"(rd_val)
                 : [tm]"r"(req_tm)
                 : "memory");
    read_mtype(mtype_val);

    XCMP(5, rd_val, 4);
    XCMP(6, MTYPE_TM(mtype_val), rd_val);
}

// -------------------------------------------------------
// TC6: msettm does NOT change vl
// -------------------------------------------------------
void TEST_CASE6(void) {
    uintptr_t vl_before, vl_after;
    uintptr_t req_tn = 4, req_tm = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    read_vl(vl_before);

    asm volatile("msettm x0, %[tm]" :: [tm]"r"(req_tm) : "memory");
    read_vl(vl_after);

    XCMP(7, vl_before, 4);
    XCMP(8, vl_after,  vl_before);   // vl unchanged by msettm
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