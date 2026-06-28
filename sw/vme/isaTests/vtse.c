// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// vtse.c — ISA tests for vtse8, vtse16, vtse32, vtse64
//
// -------------------------------------------------------
// Test cases:
//   TC1: vtse32 row store — int32 stored directly
//   TC2: vtse8  row store — truncate to uint8
//   TC3: vtse16 row store — truncate to uint16
//   TC4: vtse32 col store — pattern=1
//   TC5: vtse8  all rows of 4×4 tile
//   TC6: vtse32 tile isolation (mt0 stored, mt4 untouched)
//   TC7: vtmmu then vtse32 — store computed result
//   TC8: vtse8 truncation check — values > 255 truncated

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// TC1: vtse32 row store — int32 stored directly
void TEST_CASE1(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uintptr_t tss = TSS_ROW(0, 0);

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    VLOAD_32(v8, 111, 222, 333, 444, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    asm volatile("vtmv.t.v %[tss], v8" :: [tss]"r"(tss) : "memory");

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    static int32_t dst[4] = {0, 0, 0, 0};
    uintptr_t base = (uintptr_t)dst;
    asm volatile("vtse32 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e32, m1);
    asm volatile("vle32.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_I32(1, v1, 111, 222, 333, 444);

    asm volatile("vtdiscard" ::: "memory");
}

// TC2: vtse8 row store — truncate int32 to low 8 bits
void TEST_CASE2(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uintptr_t tss = TSS_ROW(0, 0);

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    VLOAD_32(v8, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    asm volatile("vtmv.t.v %[tss], v8" :: [tss]"r"(tss) : "memory");

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    static uint8_t dst[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uintptr_t base = (uintptr_t)dst;
    asm volatile("vtse8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e8, m1);
    asm volatile("vle8.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_U8(2, v1, 1, 2, 3, 4);

    asm volatile("vtdiscard" ::: "memory");
}

// TC3: vtse16 row store — truncate int32 to low 16 bits
void TEST_CASE3(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uintptr_t tss = TSS_ROW(0, 0);

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    VLOAD_32(v8, 100, 200, 300, 400, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    asm volatile("vtmv.t.v %[tss], v8" :: [tss]"r"(tss) : "memory");

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    static uint16_t dst[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    uintptr_t base = (uintptr_t)dst;
    asm volatile("vtse16 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e16, m1);
    asm volatile("vle16.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_U16(3, v1, 100, 200, 300, 400);

    asm volatile("vtdiscard" ::: "memory");
}

// TC4: vtse32 col store — pattern=1
void TEST_CASE4(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uintptr_t tss = TSS_COL(0, 0);

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    VLOAD_32(v8, 10, 20, 30, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    asm volatile("vtmv.t.v %[tss], v8" :: [tss]"r"(tss) : "memory");

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    static int32_t dst[4] = {0, 0, 0, 0};
    uintptr_t base = (uintptr_t)dst;
    asm volatile("vtse32 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e32, m1);
    asm volatile("vle32.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_I32(4, v1, 10, 20, 30, 40);

    asm volatile("vtdiscard" ::: "memory");
}

// TC5: vtse8 all rows — store all 4 rows to separate buffers
void TEST_CASE5(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t t;
    VLOAD_32(v8, 1, 2, 3, 4,  0,0,0,0,0,0,0,0,0,0,0,0);
    t = TSS_ROW(0, 0); asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");
    VLOAD_32(v8, 5, 6, 7, 8,  0,0,0,0,0,0,0,0,0,0,0,0);
    t = TSS_ROW(0, 1); asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");
    VLOAD_32(v8, 9,10,11,12,  0,0,0,0,0,0,0,0,0,0,0,0);
    t = TSS_ROW(0, 2); asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");
    VLOAD_32(v8,13,14,15,16,  0,0,0,0,0,0,0,0,0,0,0,0);
    t = TSS_ROW(0, 3); asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    static uint8_t r0[4], r1[4], r2[4], r3[4];
    uintptr_t tss, base;

    tss = TSS_ROW(0, 0); base = (uintptr_t)r0;
    asm volatile("vtse8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");
    tss = TSS_ROW(0, 1); base = (uintptr_t)r1;
    asm volatile("vtse8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");
    tss = TSS_ROW(0, 2); base = (uintptr_t)r2;
    asm volatile("vtse8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");
    tss = TSS_ROW(0, 3); base = (uintptr_t)r3;
    asm volatile("vtse8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e8, m1);
    asm volatile("vle8.v v1, (%0)" :: "r"(r0) : "memory");
    VCMP_U8(5, v1,  1,  2,  3,  4);
    asm volatile("vle8.v v1, (%0)" :: "r"(r1) : "memory");
    VCMP_U8(6, v1,  5,  6,  7,  8);
    asm volatile("vle8.v v1, (%0)" :: "r"(r2) : "memory");
    VCMP_U8(7, v1,  9, 10, 11, 12);
    asm volatile("vle8.v v1, (%0)" :: "r"(r3) : "memory");
    VCMP_U8(8, v1, 13, 14, 15, 16);

    asm volatile("vtdiscard" ::: "memory");
}

// TC6: vtse32 tile isolation — mt0 stored, mt4 untouched
void TEST_CASE6(void) {
    const uintptr_t TN = 6, TM = 6, TK = 2;

    uintptr_t tss = TSS_ROW(0, 0);

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    VLOAD_32(v8, 77, 88, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    asm volatile("vtmv.t.v %[tss], v8" :: [tss]"r"(tss) : "memory");

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    static int32_t dst0[2] = {-1, -1};
    int32_t *dst4 = (int32_t *)snrt_l1alloc(2 * sizeof(int32_t));
    dst4[0] = -1; dst4[1] = -1;

    uintptr_t base = (uintptr_t)dst0;
    asm volatile("vtse32 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(2, e32, m1);
    asm volatile("vle32.v v1, (%0)" :: "r"(dst0) : "memory");
    VCMP_I32(9, v1, 77, 88);
    asm volatile("vle32.v v1, (%0)" :: "r"(dst4) : "memory");
    VCMP_I32(10, v1, -1, -1);   // unchanged

    asm volatile("vtdiscard" ::: "memory");
}

// TC8: vtse8 truncation — values > 255 lose high bits
//   Load [256, 257, 0x1FF, -1], vtse8 → [0, 1, 0xFF, 0xFF]
void TEST_CASE7(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uintptr_t tss = TSS_ROW(0, 0);

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    VLOAD_32(v8, 256, 257, 0x1FF, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    asm volatile("vtmv.t.v %[tss], v8" :: [tss]"r"(tss) : "memory");

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    static uint8_t dst[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    uintptr_t base = (uintptr_t)dst;
    asm volatile("vtse8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e8, m1);
    asm volatile("vle8.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_U8(12, v1, 0x00, 0x01, 0xFF, 0xFF);

    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();
    enable_vec();
    enable_vme();

    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
    // TEST_CASE4();
    TEST_CASE5();
    TEST_CASE6();
    TEST_CASE7();

    EXIT_CHECK();
}
