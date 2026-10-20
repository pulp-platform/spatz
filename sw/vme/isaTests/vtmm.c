// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Pei-Yu Lin, ETH Zürich
//
// Test cases:
//   TC1: vtmmu with unsigned A and unsigned B.
//   TC2: vtmmu with unsigned A and signed B.
//   TC3: vtmms with signed A and unsigned B.
//   TC4: vtmms with signed A and signed B, read back by columns.

#include <stdint.h>

#include "vector_macros.h"
#include "vector_matrix_macros.h"

#define LOAD_A(a0, a1, a2, a3)                                               \
    do {                                                                      \
        VLOAD_8(v8,  a0, a2, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);                 \
        VLOAD_8(v10, a1, a3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);                 \
        VLOAD_8(v12, 0, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);                   \
        VLOAD_8(v14, 0, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);                   \
    } while (0)

#define LOAD_B(b0, b1, b2, b3)                                               \
    do {                                                                      \
        VLOAD_8(v16, b0, b1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);                 \
        VLOAD_8(v18, b2, b3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);                 \
        VLOAD_8(v20, 0, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);                   \
        VLOAD_8(v22, 0, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);                   \
    } while (0)

#define CONFIG_I8(altfmt)                                                     \
    do {                                                                      \
        asm volatile("msetmtypei 3, 0, " #altfmt ::: "memory");             \
        asm volatile("msettn zero, %0" :: "r"((uintptr_t)2) : "memory");   \
        asm volatile("msettm zero, %0" :: "r"((uintptr_t)2) : "memory");   \
        asm volatile("msettk zero, %0"                                       \
                     :: "r"((uintptr_t)KMAX(8, 4)) : "memory");             \
    } while (0)

#define CONFIG_I32_RESULT()                                                   \
    do {                                                                      \
        asm volatile("msetmtypei 1, 2, 0" ::: "memory");                    \
        asm volatile("msettn zero, %0" :: "r"((uintptr_t)2) : "memory");   \
        asm volatile("msettm zero, %0" :: "r"((uintptr_t)2) : "memory");   \
    } while (0)

// vtmmu, altfmt=0: uint8 * uint8 -> int32.
static void TEST_CASE1(void) {
    VSET(2, e8, m1);
    LOAD_A(1, 2, 3, 4);
    LOAD_B(5, 7, 6, 8);
    CONFIG_I8(0);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    CONFIG_I32_RESULT();
    uintptr_t tss = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %0" :: "r"(tss) : "memory");
    tss = TSS_ROW(0, 1);
    asm volatile("vtmv.v.t v2, %0" :: "r"(tss) : "memory");
    VCMP_I32(1, v1, 17, 23);
    VCMP_I32(2, v2, 39, 53);
    asm volatile("vtdiscard" ::: "memory");
}

// vtmmu, altfmt=1: uint8 * int8 -> int32.
static void TEST_CASE2(void) {
    VSET(2, e8, m1);
    LOAD_A(1, 2, 3, 4);
    LOAD_B(0xfb, 0xf9, 0xfa, 0xf8);
    CONFIG_I8(1);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmmu.tvv mt0, v8, v16" ::: "memory");

    CONFIG_I32_RESULT();
    uintptr_t tss = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %0" :: "r"(tss) : "memory");
    tss = TSS_ROW(0, 1);
    asm volatile("vtmv.v.t v2, %0" :: "r"(tss) : "memory");
    VCMP_I32(3, v1, -17, -23);
    VCMP_I32(4, v2, -39, -53);
    asm volatile("vtdiscard" ::: "memory");
}

// vtmms, altfmt=0: int8 * uint8 -> int32.
static void TEST_CASE3(void) {
    VSET(2, e8, m1);
    LOAD_A(0xff, 0xfe, 0xfd, 0xfc);
    LOAD_B(5, 7, 6, 8);
    CONFIG_I8(0);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmms.tvv mt0, v8, v16" ::: "memory");

    CONFIG_I32_RESULT();
    uintptr_t tss = TSS_ROW(0, 0);
    asm volatile("vtmv.v.t v1, %0" :: "r"(tss) : "memory");
    tss = TSS_ROW(0, 1);
    asm volatile("vtmv.v.t v2, %0" :: "r"(tss) : "memory");
    VCMP_I32(5, v1, -17, -23);
    VCMP_I32(6, v2, -39, -53);
    asm volatile("vtdiscard" ::: "memory");
}

// vtmms, altfmt=1: int8 * int8 -> int32; read the result by columns.
static void TEST_CASE4(void) {
    VSET(2, e8, m1);
    LOAD_A(0xff, 0xfe, 0xfd, 0xfc);
    LOAD_B(0xfb, 0xf9, 0xfa, 0xf8);
    CONFIG_I8(1);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtmms.tvv mt0, v8, v16" ::: "memory");

    CONFIG_I32_RESULT();
    uintptr_t tss = TSS_COL(0, 0);
    asm volatile("vtmv.v.t v1, %0" :: "r"(tss) : "memory");
    tss = TSS_COL(0, 1);
    asm volatile("vtmv.v.t v2, %0" :: "r"(tss) : "memory");
    VCMP_I32(7, v1, 17, 39);
    VCMP_I32(8, v2, 23, 53);
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
