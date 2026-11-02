// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Pei-Yu Lin, ETH Zürich
//
// Test cases:
//   TC1: round-trip all rows of a 4x4 tile.
//   TC2: preserve zeroed rows while updating non-consecutive rows.
//   TC3: keep two tile registers independent.
//   TC4: round-trip a 2x2 tile.

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// TC1: 4x4 tile, TEW=32 round-trip: vector → tile → vector
void TEST_CASE1(void) {
    const uintptr_t TN = 4, TM = 4;

    VSET(4, e32, m1);
    VLOAD_32(v8,  10, 20, 30, 40,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_32(v10, 50, 60, 70, 80,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_32(v12, 11, 22, 33, 44,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_32(v14, 55, 66, 77, 88,  0,0,0,0,  0,0,0,0,  0,0,0,0);

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("vtzero mt0" ::: "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.t.v %[tss], v8"  :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.t.v %[tss], v10" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 2); asm volatile("vtmv.t.v %[tss], v12" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 3); asm volatile("vtmv.t.v %[tss], v14" :: [tss]"r"(tss) : "memory");

    // Preserve matrix configuration while reading the tile back.
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[tss]" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[tss]" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 2); asm volatile("vtmv.v.t v3, %[tss]" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 3); asm volatile("vtmv.v.t v4, %[tss]" :: [tss]"r"(tss) : "memory");

    VCMP_U32(1, v1, 10, 20, 30, 40);
    VCMP_U32(2, v2, 50, 60, 70, 80);
    VCMP_U32(3, v3, 11, 22, 33, 44);
    VCMP_U32(4, v4, 55, 66, 77, 88);

    asm volatile("vtdiscard" ::: "memory");
}

// TC2: write to two non-consecutive rows, verify unwritten rows stay zero
void TEST_CASE2(void) {
    const uintptr_t TN = 4, TM = 4;

    VSET(4, e32, m1);
    VLOAD_32(v8,  1, 2, 3, 4,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_32(v10, 9,10,11,12,  0,0,0,0,  0,0,0,0,  0,0,0,0);

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("vtzero mt0" ::: "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.t.v %[tss], v8"  :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 2); asm volatile("vtmv.t.v %[tss], v10" :: [tss]"r"(tss) : "memory");

    // Preserve matrix configuration while reading the tile back.
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[tss]" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[tss]" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 2); asm volatile("vtmv.v.t v3, %[tss]" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 3); asm volatile("vtmv.v.t v4, %[tss]" :: [tss]"r"(tss) : "memory");

    VCMP_U32(1, v1,  1,  2,  3,  4);   // row 0: written
    VCMP_U32(2, v2,  0,  0,  0,  0);   // row 1: still zero
    VCMP_U32(3, v3,  9, 10, 11, 12);   // row 2: written
    VCMP_U32(4, v4,  0,  0,  0,  0);   // row 3: still zero

    asm volatile("vtdiscard" ::: "memory");
}

// TC3: write to two different tile registers (mt0 and mt4), verify independence
void TEST_CASE3(void) {
    const uintptr_t TN = 4, TM = 4;

    VSET(4, e32, m1);
    VLOAD_32(v8,  0xA, 0xB, 0xC, 0xD,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_32(v10, 0x1, 0x2, 0x3, 0x4,  0,0,0,0,  0,0,0,0,  0,0,0,0);

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtzero mt4" ::: "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.t.v %[tss], v8"  :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(4, 0); asm volatile("vtmv.t.v %[tss], v10" :: [tss]"r"(tss) : "memory");

    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[tss]" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v2, %[tss]" :: [tss]"r"(tss) : "memory");

    VSET(4, e32, m1);
    VCMP_U32(1, v1, 0xA, 0xB, 0xC, 0xD);
    VCMP_U32(2, v2, 0x1, 0x2, 0x3, 0x4);

    asm volatile("vtdiscard" ::: "memory");
}

// TC4: 2x2 tile, TEW=32 round-trip with small dimensions
void TEST_CASE4(void) {
    const uintptr_t TN = 2, TM = 2;

    VSET(2, e32, m1);
    VLOAD_32(v8,  100, 200,  0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_32(v10, 300, 400,  0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("vtzero mt0" ::: "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.t.v %[tss], v8"  :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.t.v %[tss], v10" :: [tss]"r"(tss) : "memory");

    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[tss]" :: [tss]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[tss]" :: [tss]"r"(tss) : "memory");

    VSET(2, e32, m1);
    VCMP_U32(1, v1, 100, 200);
    VCMP_U32(2, v2, 300, 400);

    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();

    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
    TEST_CASE4();

    EXIT_CHECK();
}
