// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// msettn — Set tile N dimension (tn = number of output columns)
//
// -------------------------------------------------------
// Test cases:
//   TC1: mtwiden=3, SEW=e8, request tn=4  → min(4, ETE)=4
//   TC2: mtwiden=3, SEW=e8, request tn>ETE → clamped to ETE
//   TC3: mtwiden=3, SEW=e8, request tn=0  → 0
//   TC4: mtwiden=0 — standard vl path, no ETE limit
//   TC5: returned rd == vl CSR

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// Discover ETE at runtime instead of hardcoding
static uintptr_t get_ete(void) {
    uintptr_t vl_val;
    uintptr_t large = 0xFFFFFFFF;
    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");   // mtwiden=3, e8
    asm volatile("msettn x0, %[v]" :: [v]"r"(large) : "memory");
    read_vl(vl_val);
    return vl_val;   // this is min(vlmax, ETE) = ETE (since vlmax >> ETE)
}

// -------------------------------------------------------
// TC1: mtwiden=3, SEW=e8, request tn=4
//   new_vl = min(4, ETE) = 4  (since 4 < ETE)
// -------------------------------------------------------
void TEST_CASE1(void) {
    uintptr_t vl_val;
    uintptr_t req_tn = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    read_vl(vl_val);

    XCMP(1, vl_val, 4);
}

// -------------------------------------------------------
// TC2: mtwiden=3, SEW=e8, request tn > ETE
//   new_vl = ETE  (clamped)
//   We discover ETE at runtime via get_ete()
// -------------------------------------------------------
void TEST_CASE2(void) {
    uintptr_t vl_val;
    uintptr_t ete = get_ete();
    uintptr_t req_tn = 0xFFFF;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    read_vl(vl_val);

    XCMP(2, vl_val, ete);
}

// -------------------------------------------------------
// TC3: mtwiden=3, SEW=e8, request tn=0
//   new_vl = 0
// -------------------------------------------------------
void TEST_CASE3(void) {
    uintptr_t vl_val;
    uintptr_t req_tn = 0;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    read_vl(vl_val);

    XCMP(3, vl_val, 0);
}

// -------------------------------------------------------
// TC4: mtwiden=0 — standard vector path
//   vlmax = (512/8/1)*1 = 64
//   new_vl = min(8, 64) = 8  (no ETE limit)
// -------------------------------------------------------
void TEST_CASE4(void) {
    uintptr_t vl_val;
    uintptr_t req_tn = 8;

    asm volatile("msetmtypei 0x0, 0x0" ::: "memory");   // mtwiden=0, e8
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    read_vl(vl_val);

    XCMP(4, vl_val, 8);
}

// -------------------------------------------------------
// TC5: returned rd value == vl CSR
// -------------------------------------------------------
void TEST_CASE5(void) {
    uintptr_t rd_val = 0xDEAD;
    uintptr_t vl_val;
    uintptr_t req_tn = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn %[rd], %[tn]"
                 : [rd]"=r"(rd_val)
                 : [tn]"r"(req_tn)
                 : "memory");
    read_vl(vl_val);

    XCMP(5, rd_val, 4);
    XCMP(6, vl_val, rd_val);
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

    EXIT_CHECK();
}