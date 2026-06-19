// Copyright 2026 ETH Zurich and University of Bologna.
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

static inline uintptr_t min3(uintptr_t a, uintptr_t b, uintptr_t c) {
    uintptr_t tmp;
    tmp = (a < b) ? a : b;
    return (tmp < c) ? tmp : c;
}

void TEST_CASE1(void) {
    uintptr_t vl_val;
    uintptr_t req_tn = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(1, vl_val, 4);
}

void TEST_CASE2(void) {
    uintptr_t vl_val;
    uintptr_t req_tn = 0xFFFF;
    uintptr_t expected;
    uintptr_t vlenb, vlen_bits, tew_bits;
    uintptr_t lmul_eve, ete;

    asm volatile("csrr %0, vlenb" : "=r"(vlenb));
    vlen_bits = vlenb * 8u;
    tew_bits  = 8u * 4u;      // SEW=e8, TWIDEN=4 => TEW=32
    lmul_eve  = vlen_bits / tew_bits;
    ete       = (vlen_bits / 2u) / tew_bits;
    expected  = min3(req_tn, lmul_eve, ete);

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(2, vl_val, expected);
}

void TEST_CASE3(void) {
    uintptr_t vl_val;
    uintptr_t req_tn = 0;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(3, vl_val, 0);
}

void TEST_CASE4(void) {
    uintptr_t vl_val;
    uintptr_t req_tn = 8;

    asm volatile("msetmtypei 0x0, 0x0" ::: "memory");
    asm volatile("msettn x0, %[tn]" :: [tn]"r"(req_tn) : "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

    XCMP(4, vl_val, 8);
}

void TEST_CASE5(void) {
    uintptr_t rd_val = 0xDEAD;
    uintptr_t vl_val;
    uintptr_t req_tn = 4;

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettn %[rd], %[tn]" : [rd]"=r"(rd_val) : [tn]"r"(req_tn) : "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_val));

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
