// Copyright 2024 ETH Zurich and University of Bologna.
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

// Helper: load known values into tile via vtmv.t.v (SEW=e32)
static inline void load_tile_row_i32(uint32_t tile, uint32_t row,
                                      uintptr_t tn, uintptr_t tm, uintptr_t tk,
                                      int32_t v0, int32_t v1,
                                      int32_t v2, int32_t v3) {
    vme_readback_e32(tn, tm);
    VSET(4, e32, m1);
    VLOAD_32(v8, v0, v1, v2, v3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    uintptr_t tss = TSS_ROW(tile, row);
    asm volatile("vtmv.t.v %[tss], v8" :: [tss]"r"(tss) : "memory");
    // Restore to e8 config for vtse operations
    vme_cfg_e8(tn, tm, tk);
}

// -------------------------------------------------------
// TC1: vtse32 row store — int32 stored directly
//
//   Load [111, 222, 333, 444] into tile row 0 via vtmv.t.v
//   Store to dst[] via vtse32
//   dst must equal [111, 222, 333, 444]
// -------------------------------------------------------
void TEST_CASE1(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    asm volatile("vtzero mt0" ::: "memory");
    load_tile_row_i32(0, 0, TN, TM, TK, 111, 222, 333, 444);

    static int32_t dst[4] = {0, 0, 0, 0};
    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base = (uintptr_t)dst;

    vme_cfg_e8(TN, TM, TK);
    asm volatile("vtse32 %[tss], (%[base])"
                 :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e32, m1);
    asm volatile("vle32.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_I32(1, v1, 111, 222, 333, 444);

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC2: vtse8 row store — truncate int32 to low 8 bits
//
//   Load [1, 2, 3, 4] into tile row 0
//   vtse8: store low 8 bits → dst[j] = (uint8_t)tile[0][0][j]
//   Expected: [1, 2, 3, 4]
// -------------------------------------------------------
void TEST_CASE2(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    asm volatile("vtzero mt0" ::: "memory");
    load_tile_row_i32(0, 0, TN, TM, TK, 1, 2, 3, 4);

    static uint8_t dst[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base = (uintptr_t)dst;

    vme_cfg_e8(TN, TM, TK);
    asm volatile("vtse8 %[tss], (%[base])"
                 :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e8, m1);
    asm volatile("vle8.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_U8(2, v1, 1, 2, 3, 4);

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC3: vtse16 row store — truncate int32 to low 16 bits
//
//   Load [100, 200, 300, 400] into tile row 0
//   vtse16: store low 16 bits → dst[j] = (uint16_t)tile[0][0][j]
// -------------------------------------------------------
void TEST_CASE3(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    asm volatile("vtzero mt0" ::: "memory");
    load_tile_row_i32(0, 0, TN, TM, TK, 100, 200, 300, 400);

    static uint16_t dst[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base = (uintptr_t)dst;

    vme_cfg_e8(TN, TM, TK);
    asm volatile("vtse16 %[tss], (%[base])"
                 :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(4, e16, m1);
    asm volatile("vle16.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_U16(3, v1, 100, 200, 300, 400);

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC4: vtse32 col store — pattern=1
//
//   Load col 0 = [10, 20, 30, 40] via vtmv.t.v col pattern
//   vtse32 with col TSS → store to dst[]
//   dst[i] = tile[0][i][0]
// -------------------------------------------------------
void TEST_CASE4(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    asm volatile("vtzero mt0" ::: "memory");

    // Load column 0 via vtmv.t.v
    vme_readback_e32(TN, TM);
    VSET(4, e32, m1);
    VLOAD_32(v8, 10, 20, 30, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    uintptr_t tss_col = TSS_COL(0, 0);
    asm volatile("vtmv.t.v %[tss], v8" :: [tss]"r"(tss_col) : "memory");

    // Store col 0 to memory
    static int32_t dst[4] = {0, 0, 0, 0};
    uintptr_t base = (uintptr_t)dst;
    vme_cfg_e8(TN, TM, TK);
    asm volatile("vtse32 %[tss], (%[base])"
                 :: [tss]"r"(tss_col), [base]"r"(base) : "memory");

    VSET(4, e32, m1);
    asm volatile("vle32.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_I32(4, v1, 10, 20, 30, 40);

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC5: vtse8 all rows — store all 4 rows to separate buffers
// -------------------------------------------------------
void TEST_CASE5(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    // Load 4 rows into tile via vtmv.t.v
    vme_readback_e32(TN, TM);
    VSET(4, e32, m1);

    uintptr_t t;
    VLOAD_32(v8, 1, 2, 3, 4,  0,0,0,0,0,0,0,0,0,0,0,0);
    t = TSS_ROW(0, 0); asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");
    VLOAD_32(v8, 5, 6, 7, 8,  0,0,0,0,0,0,0,0,0,0,0,0);
    t = TSS_ROW(0, 1); asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");
    VLOAD_32(v8, 9,10,11,12,  0,0,0,0,0,0,0,0,0,0,0,0);
    t = TSS_ROW(0, 2); asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");
    VLOAD_32(v8,13,14,15,16,  0,0,0,0,0,0,0,0,0,0,0,0);
    t = TSS_ROW(0, 3); asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");

    // Store all 4 rows via vtse8
    static uint8_t r0[4], r1[4], r2[4], r3[4];
    uintptr_t tss, base;

    vme_cfg_e8(TN, TM, TK);

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

// -------------------------------------------------------
// TC6: vtse32 tile isolation
//   Store mt0 row 0. mt4 row 0 must NOT be stored.
// -------------------------------------------------------
void TEST_CASE6(void) {
    const uintptr_t TN = 6, TM = 6, TK = 2;

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtzero mt4" ::: "memory");

    // Load mt0 row 0 with [77, 88]
    vme_readback_e32(TN, TM);
    VSET(2, e32, m1);
    VLOAD_32(v8, 77, 88, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    uintptr_t t = TSS_ROW(0, 0);
    asm volatile("vtmv.t.v %[t], v8" :: [t]"r"(t) : "memory");

    // Store only mt0 row 0
    static int32_t dst0[2] = {-1, -1};
    int32_t *dst4 = (int32_t *)snrt_l1alloc(2 * sizeof(int32_t));
    dst4[0] = -1; dst4[1] = -1;

    vme_cfg_e8(TN, TM, TK);

    uintptr_t tss, base;
    tss = TSS_ROW(0, 0); base = (uintptr_t)dst0;
    asm volatile("vtse32 %[tss], (%[base])"
                 :: [tss]"r"(tss), [base]"r"(base) : "memory");

    // Do NOT store mt4 — dst4 must remain unchanged
    VSET(2, e32, m1);
    asm volatile("vle32.v v1, (%0)" :: "r"(dst0) : "memory");
    VCMP_I32(9, v1, 77, 88);
    asm volatile("vle32.v v1, (%0)" :: "r"(dst4) : "memory");
    VCMP_I32(10, v1, -1, -1);   // unchanged

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC7: vtmmu then vtse32 — store a computed result
//
//   A = [[1,3],[2,4]] (stored as k-slices), B = identity
//   vtmmu result: mt0 = [[1,2],[3,4]]
//   vtse32 row 0 → dst = [1, 2]
//   vtse32 row 1 → dst = [3, 4]
// -------------------------------------------------------
void TEST_CASE7(void) {
    const uintptr_t TN = 4, TM = 4, TK = 2;
    vme_cfg_e8(TN, TM, TK);

    VSET(2, e8, m1);
    VLOAD_8(v8,  1, 3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v10, 2, 4, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v16, 1, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_8(v18, 0, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    static int32_t row0[2] = {0, 0};
    static int32_t row1[2] = {0, 0};

    uintptr_t tss, base;
    tss = TSS_ROW(0, 0); base = (uintptr_t)row0;
    asm volatile("vtse32 %[tss], (%[base])"
                 :: [tss]"r"(tss), [base]"r"(base) : "memory");
    tss = TSS_ROW(0, 1); base = (uintptr_t)row1;
    asm volatile("vtse32 %[tss], (%[base])"
                 :: [tss]"r"(tss), [base]"r"(base) : "memory");

    VSET(2, e32, m1);
    asm volatile("vle32.v v1, (%0)" :: "r"(row0) : "memory");
    VCMP_I32(11, v1, 1, 2);
    asm volatile("vle32.v v1, (%0)" :: "r"(row1) : "memory");
    VCMP_I32(12, v1, 3, 4);

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC8: vtse8 truncation — values > 255 lose high bits
//
//   Load tile row 0 with [256, 257, 0x1FF, -1]
//   vtse8: store low 8 bits
//   Expected: [0, 1, 0xFF, 0xFF]
// -------------------------------------------------------
void TEST_CASE8(void) {
    const uintptr_t TN = 4, TM = 4, TK = 4;

    asm volatile("vtzero mt0" ::: "memory");
    load_tile_row_i32(0, 0, TN, TM, TK, 256, 257, 0x1FF, -1);

    static uint8_t dst[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    uintptr_t tss  = TSS_ROW(0, 0);
    uintptr_t base = (uintptr_t)dst;

    vme_cfg_e8(TN, TM, TK);
    asm volatile("vtse8 %[tss], (%[base])"
                 :: [tss]"r"(tss), [base]"r"(base) : "memory");

    // 256 = 0x100 → 0x00
    // 257 = 0x101 → 0x01
    // 0x1FF      → 0xFF
    // -1 = 0xFFFFFFFF → 0xFF
    VSET(4, e8, m1);
    asm volatile("vle8.v v1, (%0)" :: "r"(dst) : "memory");
    VCMP_U8(13, v1, 0x00, 0x01, 0xFF, 0xFF);

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
    TEST_CASE5();
    TEST_CASE6();
    TEST_CASE7();
    TEST_CASE8();

    EXIT_CHECK();
}