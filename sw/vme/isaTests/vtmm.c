// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// vtmm.c — ISA tests for vtmmu.tvv and vtmms.tvv
//
// -------------------------------------------------------
// Test cases:
//   TC1: vtmmu  A*I=A  (uint8*uint8, identity)
//   TC2: vtmmu  known dot product
//   TC3: vtmmu  accumulation — call twice = 2x
//   TC4: vtmmu  tile isolation — mt0 and mt4 independent
//   TC5: vtmmu  altfmt=1 (uint8 × int8), A*I=A
//   TC6: vtmmu  altfmt=1 (uint8 × int8), B with negatives
//   TC7: vtmmu  altfmt isolation — B=0xFE as uint8 vs int8
//   TC8: vtmms  int8*uint8 (ALTFMT=0)
//   TC9: vtmms  int8*int8  (ALTFMT=1)
//   TC10: vtmms extreme values — sign extension check
//   TC11: vtmms ALTFMT isolation

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// TC1: vtmmu A*I=A (uint8*uint8, identity)
void TEST_CASE1(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e8, m1);
    VLOAD_8(v8,  1, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v10, 2, 4, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 1, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v18, 0, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(1, v1, 1, 2);
    VCMP_I32(2, v2, 3, 4);

    asm volatile("vtdiscard" ::: "memory");
}

// TC2: vtmmu known dot product — C[0]=[17,23], C[1]=[39,53]
void TEST_CASE2(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e8, m1);
    VLOAD_8(v8,  1, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v10, 2, 4, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 5, 7, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v18, 6, 8, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt12" ::: "memory");
    asm volatile("vtmmu.tvv mt12, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(12, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(12, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(3, v1, 17, 23);
    VCMP_I32(4, v2, 39, 53);

    asm volatile("vtdiscard" ::: "memory");
}

// TC3: vtmmu accumulation — call twice = 2x, C[0]=[2,4], C[1]=[6,8]
void TEST_CASE3(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e8, m1);
    VLOAD_8(v8,  1, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v10, 2, 4, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 1, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v18, 0, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt12" ::: "memory");
    asm volatile("vtmmu.tvv mt12, v8, v16" ::: "memory");
    asm volatile("vtmmu.tvv mt12, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(12, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(12, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(5, v1, 2, 4);
    VCMP_I32(6, v2, 6, 8);

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC4: vtmmu tile isolation — mt0 and mt4 independent
// -------------------------------------------------------
void TEST_CASE4(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e8, m1);
    VLOAD_8(v8,  1, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v10, 2, 4, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v24, 5, 7, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v26, 6, 8, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 1, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v18, 0, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtmmu.tvv mt0, v8,  v16" ::: "memory");
    asm volatile("vtmmu.tvv mt4, v24, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v3, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v4, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(7,  v1, 1, 2);
    VCMP_I32(8,  v2, 3, 4);
    VCMP_I32(9,  v3, 5, 6);
    VCMP_I32(10, v4, 7, 8);

    asm volatile("vtdiscard" ::: "memory");
}

// TC5: vtmmu altfmt=1 (uint8 × int8), A*I=A
void TEST_CASE5(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e8, m1);
    VLOAD_8(v8,  1, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);   // k=0 A
    VLOAD_8(v10, 2, 4, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);   // k=1 A
    VLOAD_8(v16, 1, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);   // k=0 B int8
    VLOAD_8(v18, 0, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);   // k=1 B int8

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(11, v1, 1, 2);
    VCMP_I32(12, v2, 3, 4);

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtdiscard" ::: "memory");
}

// TC6: vtmmu altfmt=1 (uint8 × int8), B with negatives
void TEST_CASE6(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VSET(2, e8, m1);
    VLOAD_8(v8,  2, 3,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A (uint8)
    VLOAD_8(v16, 1, 0xFE, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B (int8: [1, -2])

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(13, v1,  2, -4);
    VCMP_I32(14, v2,  3, -6);

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC7: vtmmu altfmt isolation — B=0xFE as uint8 vs int8
//   altfmt=0: C[0][1]=508   altfmt=1: C[0][1]=-4
// -------------------------------------------------------
void TEST_CASE7(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    // --- altfmt=0: B as uint8 ---
    VSET(2, e8, m1);
    VLOAD_8(v8,  2, 3,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 1, 0xFE, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // 0xFE=254 as uint8

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(15, v1,   2, 508);
    VCMP_I32(16, v2,   3, 762);

    // --- altfmt=1: B as int8 ---
    VSET(2, e8, m1);
    VLOAD_8(v8,  2, 3,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 1, 0xFE, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // 0xFE=-2 as int8

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(17, v1,  2,  -4);
    VCMP_I32(18, v2,  3,  -6);

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtdiscard" ::: "memory");
}

// TC8: vtmms int8*uint8 (ALTFMT=0) — C[0]=[-17,-23], C[1]=[39,53]
void TEST_CASE8(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e8, m1);
    VLOAD_8(v8,  0xFF, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v10, 0xFE, 4, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 5,    7, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v18, 6,    8, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmms.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(19, v1, -17, -23);
    VCMP_I32(20, v2,  39,  53);

    asm volatile("vtdiscard" ::: "memory");
}

// TC9: vtmms int8*int8 (ALTFMT=1) — C[0]=[17,23], C[1]=[-39,-53]
void TEST_CASE9(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e8, m1);
    VLOAD_8(v8,  0xFF, 3,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v10, 0xFE, 4,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 0xFB, 0xF9, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v18, 0xFA, 0xF8, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtmms.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(21, v1,  17,  23);
    VCMP_I32(22, v2, -39, -53);

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtdiscard" ::: "memory");
}

// TC10: vtmms extreme values — sign extension check, A=[-128,127]*B=[1,1]
void TEST_CASE10(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VSET(2, e8, m1);
    VLOAD_8(v8,  0x80, 0x7F, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 1,    1,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtmms.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(23, v1, -128, -128);
    VCMP_I32(24, v2,  127,  127);

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC11: vtmms ALTFMT isolation — B=[0xFF,1]: uint8=255 vs int8=-1
//   ALTFMT=0: C[0]=[255,1]   ALTFMT=1: C[0]=[-1,1]
// -------------------------------------------------------
void TEST_CASE11(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");

    // --- run 1: ALTFMT=0, B is uint8 ---
    VSET(2, e8, m1);
    VLOAD_8(v8,  1,    1,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 0xFF, 1,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmms.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(25, v1, 255, 1);
    VCMP_I32(26, v2, 255, 1);

    // --- run 2: ALTFMT=1, B is int8 ---
    VSET(2, e8, m1);
    VLOAD_8(v8,  1,    1,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 0xFF, 1,    0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtmms.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_I32(27, v1, -1, 1);
    VCMP_I32(28, v2, -1, 1);

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();
    enable_vec();
    enable_vme();
    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");

    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
    TEST_CASE4();
    TEST_CASE5();
    TEST_CASE6();
    TEST_CASE7();
    TEST_CASE8();
    TEST_CASE9();
    TEST_CASE10();
    TEST_CASE11();

    EXIT_CHECK();
}
