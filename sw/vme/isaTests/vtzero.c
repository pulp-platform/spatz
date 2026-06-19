// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// vtzero.c — ISA tests for vtzero
//
// -------------------------------------------------------
// Test cases:
//    TC1: SEW=e8,  TEW=32, 4×4 — vtmmu then vtzero → zeros
//    TC2: SEW=e8,  TEW=32, two tiles — vtzero mt0 only, mt4 stays intact
//    TC3: SEW=e16, TEW=32, 4×4 — vtmmu then vtzero → zeros
//    TC4: SEW=e32, TEW=32, 4×4 — I*I then vtzero → zeros

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// TC1: SEW=e8/TEW=32, 4×4 — vtmmu.tvv then vtzero → all zeros
void TEST_CASE1(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;
    VSET(4, e8, m1);

    VLOAD_8(v8,   1,  2,  3,  4,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v10,  5,  6,  7,  8,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v12,  9, 10, 11, 12,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v14, 13, 14, 15, 16,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v16, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v18, 0,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v20, 0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v22, 0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtzero mt0" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");

    VSET(4, e32, m1);
    VCMP_I32(1, v1, 0, 0, 0, 0);

    asm volatile("vtdiscard" ::: "memory");
}

// TC2: SEW=e8/TEW=32, two tiles — vtzero mt0 only, mt4 stays intact
void TEST_CASE2(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;
    VSET(4, e8, m1);

    VLOAD_8(v8,   1,  2,  3,  4,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v10,  5,  6,  7,  8,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v12,  9, 10, 11, 12,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v14, 13, 14, 15, 16,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v16, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v18, 0,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v20, 0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v22, 0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtmmu.tvv mt4, v8, v16" ::: "memory");
    asm volatile("vtzero mt0" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VSET(4, e32, m1);
    VCMP_I32(1, v1, 0, 0, 0, 0);
    VCMP_I32(2, v2, 1, 5, 9, 13);

    asm volatile("vtdiscard" ::: "memory");
}

// TC3: SEW=e16/TEW=32, 4×4 — pre-fill with vtmmu, reconfigure to e16, vtzero → zeros
void TEST_CASE3(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    VSET(4, e8, m1);
    VLOAD_8(v8,  1, 1, 1, 1,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v10, 2, 2, 2, 2,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v12, 3, 3, 3, 3,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v14, 4, 4, 4, 4,  0,0,0,0,  0,0,0,0,  0,0,0,0);
    VLOAD_8(v16, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v18, 0,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v20, 0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
    VLOAD_8(v22, 0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");

    VSET(4, e32, m1);
    VCMP_I32(1, v1, 0, 0, 0, 0);

    asm volatile("vtdiscard" ::: "memory");
}

// TC4: SEW=e32/TEW=32, 4×4 — vtmmu I*I then vtzero → all zeros
void TEST_CASE4(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;
    VSET(4, e32, m1);

    VLOAD_32(v8,  1, 0, 0, 0);
    VLOAD_32(v10, 0, 1, 0, 0);
    VLOAD_32(v12, 0, 0, 1, 0);
    VLOAD_32(v14, 0, 0, 0, 1);
    VLOAD_32(v16, 1, 0, 0, 0);
    VLOAD_32(v18, 0, 1, 0, 0);
    VLOAD_32(v20, 0, 0, 1, 0);
    VLOAD_32(v22, 0, 0, 0, 1);

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtzero mt0" ::: "memory");

    uintptr_t tss = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");

    VSET(4, e32, m1);
    VCMP_I32(1, v1, 0, 0, 0, 0);

    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();
    enable_vec();
    enable_vme();

    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
    TEST_CASE4();

    EXIT_CHECK();
}
