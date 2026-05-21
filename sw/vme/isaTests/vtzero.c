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
//    TC1: FP32, TEW=32, 4×4 — vtfmm then vtzero → zeros
//    TC2: FP32, TEW=32, two tiles — vtzero mt0 only, mt4 stays intact
//    TC3: FP32, TEW=32, 4×4 — pre-fill with vtfmm, vtzero → zeros
//    TC4: FP32, TEW=32, 4×4 — I*I then vtzero → zeros

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

#define F32_0 0x00000000u
#define F32_1 0x3f800000u
#define F32_2 0x40000000u
#define F32_3 0x40400000u
#define F32_4 0x40800000u
#define F32_5 0x40a00000u
#define F32_6 0x40c00000u
#define F32_7 0x40e00000u
#define F32_8 0x41000000u
#define F32_9 0x41100000u
#define F32_10 0x41200000u
#define F32_11 0x41300000u
#define F32_12 0x41400000u
#define F32_13 0x41500000u
#define F32_14 0x41600000u
#define F32_15 0x41700000u
#define F32_16 0x41800000u

// TC1: FP32/TEW=32, 4×4 — vtfmm.tvv then vtzero → all zeros
void TEST_CASE1(void) {
    const uintptr_t TN = 4, TM = 4, TK = 1;
    VSET(4, e32, m1);

    VLOAD_32(v8,  F32_1,  F32_5,  F32_9,  F32_13);
    VLOAD_32(v10, F32_2,  F32_6,  F32_10, F32_14);
    VLOAD_32(v12, F32_3,  F32_7,  F32_11, F32_15);
    VLOAD_32(v14, F32_4,  F32_8,  F32_12, F32_16);

    VLOAD_32(v16, F32_1, F32_0, F32_0, F32_0);
    VLOAD_32(v18, F32_0, F32_1, F32_0, F32_0);
    VLOAD_32(v20, F32_0, F32_0, F32_1, F32_0);
    VLOAD_32(v22, F32_0, F32_0, F32_0, F32_1);

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
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

// TC2: FP32/TEW=32, two tiles — vtzero mt0 only, mt4 stays intact
void TEST_CASE2(void) {
    const uintptr_t TN = 4, TM = 4, TK = 1;
    VSET(4, e32, m1);

    VLOAD_32(v8,  F32_1,  F32_5,  F32_9,  F32_13);
    VLOAD_32(v10, F32_2,  F32_6,  F32_10, F32_14);
    VLOAD_32(v12, F32_3,  F32_7,  F32_11, F32_15);
    VLOAD_32(v14, F32_4,  F32_8,  F32_12, F32_16);

    VLOAD_32(v16, F32_1, F32_0, F32_0, F32_0);
    VLOAD_32(v18, F32_0, F32_1, F32_0, F32_0);
    VLOAD_32(v20, F32_0, F32_0, F32_1, F32_0);
    VLOAD_32(v22, F32_0, F32_0, F32_0, F32_1);

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt4, v8, v16" ::: "memory");
    asm volatile("vtzero mt0" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");

    tss = TSS_ROW(4, 0);
    asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VSET(4, e32, m1);
    VCMP_I32(1, v1, 0, 0, 0, 0);
    VCMP_I32(2, v2, F32_1, F32_5, F32_9, F32_13);

    asm volatile("vtdiscard" ::: "memory");
}

// TC3: FP32/TEW=32, 4×4 — pre-fill with vtfmm, vtzero → zeros
void TEST_CASE3(void) {
    const uintptr_t TN = 4, TM = 4, TK = 1;
    VSET(4, e32, m1);

    VLOAD_32(v8,  F32_1, F32_2, F32_3, F32_4);
    VLOAD_32(v10, F32_1, F32_2, F32_3, F32_4);
    VLOAD_32(v12, F32_1, F32_2, F32_3, F32_4);
    VLOAD_32(v14, F32_1, F32_2, F32_3, F32_4);

    VLOAD_32(v16, F32_1, F32_0, F32_0, F32_0);
    VLOAD_32(v18, F32_0, F32_1, F32_0, F32_0);
    VLOAD_32(v20, F32_0, F32_0, F32_1, F32_0);
    VLOAD_32(v22, F32_0, F32_0, F32_0, F32_1);

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
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

// TC4: FP32/TEW=32, 4×4 — vtfmm I*I then vtzero → all zeros
void TEST_CASE4(void) {
    const uintptr_t TN = 4, TM = 4, TK = 1;
    VSET(4, e32, m1);

    VLOAD_32(v8,  F32_1, F32_0, F32_0, F32_0);
    VLOAD_32(v10, F32_0, F32_1, F32_0, F32_0);
    VLOAD_32(v12, F32_0, F32_0, F32_1, F32_0);
    VLOAD_32(v14, F32_0, F32_0, F32_0, F32_1);

    VLOAD_32(v16, F32_1, F32_0, F32_0, F32_0);
    VLOAD_32(v18, F32_0, F32_1, F32_0, F32_0);
    VLOAD_32(v20, F32_0, F32_0, F32_1, F32_0);
    VLOAD_32(v22, F32_0, F32_0, F32_0, F32_1);

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");

    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
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