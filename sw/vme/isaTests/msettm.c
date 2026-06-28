// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// msettm — Set tile M dimension (tm = number of output rows)

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

static inline uintptr_t min3(uintptr_t a, uintptr_t b, uintptr_t c) {
    uintptr_t tmp;
    tmp = (a < b) ? a : b;
    return (tmp < c) ? tmp : c;
}

void TEST_CASE1(void) {
    uintptr_t rd_val, mtype_val;
    uintptr_t req_tm   = 4;
    uintptr_t expected;
    uintptr_t vlenb, vlen_bits, tew_bits;
    uintptr_t lmul_eve, ete;

    asm volatile("csrr %0, vlenb" : "=r"(vlenb));
    vlen_bits = vlenb * 8u;
    tew_bits  = 8u * 4u;      // SEW=e8, TWIDEN=4 => TEW=32
    lmul_eve  = vlen_bits / tew_bits;
    ete       = (vlen_bits / 2u) / tew_bits;
    expected  = min3(req_tm, lmul_eve, ete);

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettm %[rd], %[tm]" : [rd]"=r"(rd_val) : [tm]"r"(req_tm) : "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));

    XCMP(1, rd_val, expected);
    XCMP(2, MTYPE_TM(mtype_val), expected);
    XCMP(3, MTYPE_TM(mtype_val), rd_val);
}

void TEST_CASE2(void) {
    uintptr_t rd_val, mtype_val;
    uintptr_t req_tm   = (uintptr_t)-1;
    uintptr_t expected;
    uintptr_t vlenb, vlen_bits, tew_bits;
    uintptr_t lmul_eve, ete;

    asm volatile("csrr %0, vlenb" : "=r"(vlenb));
    vlen_bits = vlenb * 8u;
    tew_bits  = 8u * 4u;      // SEW=e8, TWIDEN=4 => TEW=32
    lmul_eve  = vlen_bits / tew_bits;
    ete       = (vlen_bits / 2u) / tew_bits;
    expected  = min3(req_tm, lmul_eve, ete);

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettm %[rd], %[tm]" : [rd]"=r"(rd_val) : [tm]"r"(req_tm) : "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));

    XCMP(4, rd_val, expected);
    XCMP(5, MTYPE_TM(mtype_val), expected);
    XCMP(6, MTYPE_TM(mtype_val), rd_val);
}

void TEST_CASE3(void) {
    uintptr_t rd_val, mtype_val;
    uintptr_t req_tm   = 0;
    uintptr_t expected;
    uintptr_t vlenb, vlen_bits, tew_bits;
    uintptr_t lmul_eve, ete;

    asm volatile("csrr %0, vlenb" : "=r"(vlenb));
    vlen_bits = vlenb * 8u;
    tew_bits  = 8u * 4u;      // SEW=e8, TWIDEN=4 => TEW=32
    lmul_eve  = vlen_bits / tew_bits;
    ete       = (vlen_bits / 2u) / tew_bits;
    expected  = min3(req_tm, lmul_eve, ete);

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettm %[rd], %[tm]" : [rd]"=r"(rd_val) : [tm]"r"(req_tm) : "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));

    XCMP(7, rd_val, expected);
    XCMP(8, MTYPE_TM(mtype_val), expected);
    XCMP(9, MTYPE_TM(mtype_val), rd_val);
}

void TEST_CASE4(void) {
    uintptr_t rd_val, mtype_val;
    uintptr_t req_tm   = 4;
    uintptr_t expected = 0;

    asm volatile("msetmtypei 0x0, 0x0" ::: "memory");
    asm volatile("msettm %[rd], %[tm]" : [rd]"=r"(rd_val) : [tm]"r"(req_tm) : "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));

    XCMP(10, rd_val, expected);
    XCMP(11, MTYPE_TM(mtype_val), expected);
    XCMP(12, MTYPE_TM(mtype_val), rd_val);
}

void TEST_CASE5(void) {
    uintptr_t rd_val, mtype_val;
    uintptr_t req_tm   = 4;
    uintptr_t expected;
    uintptr_t vlenb, vlen_bits, tew_bits;
    uintptr_t lmul_eve, ete;

    asm volatile("csrr %0, vlenb" : "=r"(vlenb));
    vlen_bits = vlenb * 8u;
    tew_bits  = 8u * 4u;      // SEW=e8, TWIDEN=4 => TEW=32
    lmul_eve  = vlen_bits / tew_bits;
    ete       = (vlen_bits / 2u) / tew_bits;
    expected  = min3(req_tm, lmul_eve, ete);

    asm volatile("msetmtypei 0x3, 0x0" ::: "memory");
    asm volatile("msettm %[rd], %[tm]" : [rd]"=r"(rd_val) : [tm]"r"(req_tm) : "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));

    XCMP(13, rd_val, expected);
    XCMP(14, MTYPE_TM(mtype_val), expected);
    XCMP(15, MTYPE_TM(mtype_val), rd_val);
}

void TEST_CASE6(void) {
    uintptr_t rd_val, mtype_val;
    uintptr_t vl_before, vl_after;
    uintptr_t req_tm   = 4;
    uintptr_t expected = 0;

    asm volatile("msetmtypei 0x0, 0x0" ::: "memory");
    asm volatile("vsetivli x0, 7, e8, m1, ta, ma" ::: "memory");
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_before));

    asm volatile("msettm %[rd], %[tm]" : [rd]"=r"(rd_val) : [tm]"r"(req_tm) : "memory");
    asm volatile("csrr %[B], 0xC23" : [B]"=r"(mtype_val));
    asm volatile("csrr %[BUF], vl" : [BUF]"=r"(vl_after));

    XCMP(16, rd_val, expected);
    XCMP(17, MTYPE_TM(mtype_val), expected);
    XCMP(18, MTYPE_TM(mtype_val), rd_val);
    XCMP(19, vl_after, vl_before);
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
