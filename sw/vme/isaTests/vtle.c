// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// vtle.c — ISA tests for vtle8, vtle16, vtle32, vtle64
//
// -------------------------------------------------------
// Test cases:
//   TC1: vtle8  row load → uint8 zero-extended to int32
//   TC2: vtle16 row load → uint16 zero-extended to int32
//   TC3: vtle32 row load → int32 direct
//   TC4: vtle8  col load (pattern=1)
//   TC5: vtle8  all rows of 4×4 tile
//   TC6: vtle8  tile isolation (mt0 loaded, mt4 must stay zero)
//   TC7: vtle32 round-trip with vtse32

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// TC1: vtle8 row load — uint8 zero-extended to int32
void TEST_CASE1(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uint8_t tmp[] = {10, 20, 30, 40};
    uint8_t *src = (uint8_t *)snrt_l1alloc(4); memcpy(src, tmp, 4);

    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base = (uintptr_t)src;

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtle8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    uintptr_t t = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(t) : "memory");
    VCMP_I32(1, v1, 10, 20, 30, 40);

    asm volatile("vtdiscard" ::: "memory");
}

// TC2: vtle16 row load — uint16 zero-extended to int32
void TEST_CASE2(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uint16_t tmp[] = {100, 200, 300, 400};
    uint16_t *src = (uint16_t *)snrt_l1alloc(8); memcpy(src, tmp, 8);

    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base = (uintptr_t)src;

    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtle16 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    uintptr_t t = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(t) : "memory");
    VCMP_I32(2, v1, 100, 200, 300, 400);

    asm volatile("vtdiscard" ::: "memory");
}

// TC3: vtle32 row load — int32 direct (including negatives)
void TEST_CASE3(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    int32_t tmp[] = {1000, 2000, -3000, -4000};
    int32_t *src = (int32_t *)snrt_l1alloc(16); memcpy(src, tmp, 16);

    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base = (uintptr_t)src;

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtle32 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    uintptr_t t = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(t) : "memory");
    VCMP_I32(3, v1, 1000, 2000, -3000, -4000);

    asm volatile("vtdiscard" ::: "memory");
}

// TC4: vtle8 col load — pattern=1 (column)
void TEST_CASE4(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uint8_t tmp[] = {5, 6, 7, 8};
    uint8_t *src = (uint8_t *)snrt_l1alloc(4); memcpy(src, tmp, 4);

    uintptr_t tss  = TSS_COL(0, 0);
    uintptr_t base = (uintptr_t)src;

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtle8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    uintptr_t t = TSS_COL(0, 0);
    asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(t) : "memory");
    VCMP_I32(4, v1, 5, 6, 7, 8);

    asm volatile("vtdiscard" ::: "memory");
}

// TC5: vtle8 all rows — load all 4 rows from different buffers
void TEST_CASE5(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    uint8_t tmp0[4] = { 1,  2,  3,  4};
    uint8_t tmp1[4] = { 5,  6,  7,  8};
    uint8_t tmp2[4] = { 9, 10, 11, 12};
    uint8_t tmp3[4] = {13, 14, 15, 16};
    uint8_t *r0 = (uint8_t *)snrt_l1alloc(4); memcpy(r0, tmp0, 4);
    uint8_t *r1 = (uint8_t *)snrt_l1alloc(4); memcpy(r1, tmp1, 4);
    uint8_t *r2 = (uint8_t *)snrt_l1alloc(4); memcpy(r2, tmp2, 4);
    uint8_t *r3 = (uint8_t *)snrt_l1alloc(4); memcpy(r3, tmp3, 4);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");

    uintptr_t tss, base;
    tss = TSS_ROW(0, 0); base = (uintptr_t)r0;
    asm volatile("vtle8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");
    tss = TSS_ROW(0, 1); base = (uintptr_t)r1;
    asm volatile("vtle8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");
    tss = TSS_ROW(0, 2); base = (uintptr_t)r2;
    asm volatile("vtle8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");
    tss = TSS_ROW(0, 3); base = (uintptr_t)r3;
    asm volatile("vtle8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t t;
    t = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(t) : "memory");
    t = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(t) : "memory");
    t = TSS_ROW(0, 2); asm volatile("vtmv.v.t v3, %[t]" :: [t]"r"(t) : "memory");
    t = TSS_ROW(0, 3); asm volatile("vtmv.v.t v4, %[t]" :: [t]"r"(t) : "memory");

    VCMP_I32(5, v1,  1,  2,  3,  4);
    VCMP_I32(6, v2,  5,  6,  7,  8);
    VCMP_I32(7, v3,  9, 10, 11, 12);
    VCMP_I32(8, v4, 13, 14, 15, 16);

    asm volatile("vtdiscard" ::: "memory");
}

// TC6: vtle8 tile isolation — mt0 loaded, mt4 must stay zero
void TEST_CASE6(void) {
    const uintptr_t TN = 6, TM = 6, TK = 2;

    uint8_t tmp[] = {77, 88};
    uint8_t *src = (uint8_t *)snrt_l1alloc(2); memcpy(src, tmp, 2);

    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base = (uintptr_t)src;

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtle8 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t t;
    t = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(t) : "memory");
    t = TSS_ROW(4, 0); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(t) : "memory");

    VCMP_I32(9,  v1, 77, 88);   // mt0: loaded
    VCMP_I32(10, v2,  0,  0);   // mt4: untouched

    asm volatile("vtdiscard" ::: "memory");
}

// TC7: vtle32 + vtse32 round-trip
void TEST_CASE7(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    int32_t tmp[] = {111, 222, 333, 444};
    int32_t *src = (int32_t *)snrt_l1alloc(16); memcpy(src, tmp, 16);
    int32_t *dst = (int32_t *)snrt_l1alloc(4 * sizeof(int32_t));
    dst[0] = 0; dst[1] = 0; dst[2] = 0; dst[3] = 0;

    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base;

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");

    base = (uintptr_t)src;
    asm volatile("vtle32 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    base = (uintptr_t)dst;
    asm volatile("vtse32 %[tss], (%[base])" :: [tss]"r"(tss), [base]"r"(base) : "memory");

    asm volatile("vle32.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_I32(11, v1, 111, 222, 333, 444);

    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();
    enable_vec();
    enable_vme();

    // TEST_CASE1();
    // TEST_CASE2();
    TEST_CASE3();
    // TEST_CASE4();
    // TEST_CASE5();
    // TEST_CASE6();
    // TEST_CASE7();

    EXIT_CHECK();
}
