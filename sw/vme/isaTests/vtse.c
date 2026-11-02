// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Pei-Yu Lin, ETH Zürich
//
// Test cases:
//   TC1-TC2: 8-bit row and column stores.
//   TC3-TC4: 16-bit row and column stores.
//   TC5-TC6: 32-bit row and column stores.

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

static void TEST_CASE1(void) {
    uintptr_t n = 4, vl;
    uintptr_t tss = TSS_ROW(0, 0);
    uint8_t *src = snrt_l1alloc(4 * sizeof(*src));
    uint8_t *dst = snrt_l1alloc(6 * sizeof(*dst));
    for (unsigned i = 0; i < 4; ++i) src[i] = (uint8_t)(~(uint8_t)0 - i);
    for (unsigned i = 0; i < 6; ++i) dst[i] = 0x55;
    asm volatile("msetmtypei 1, 0, 0" ::: "memory");
    asm volatile("msettn %0, %1" : "=r"(vl) : "r"(n) : "memory");
    XCMP(10, vl, n);
    asm volatile("msettm zero, %0" :: "r"(n) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vle8.v v8, (%0)" :: "r"(src) : "v8", "memory");
    asm volatile("vtmv.t.v %0, v8" :: "r"(tss) : "memory");
    asm volatile("vtse8 %0, (%1)" :: "r"(tss), "r"(dst + 1) : "memory");
    asm volatile("fence rw, rw" ::: "memory");
    for (unsigned i = 0; i < 4; ++i) { XCMP(11, dst[i + 1], src[i]); }
    XCMP(12, dst[0], 0x55);
    XCMP(13, dst[5], 0x55);
    asm volatile("vtdiscard" ::: "memory");
}

static void TEST_CASE2(void) {
    uintptr_t n = 4, vl;
    uintptr_t tss = TSS_COL(0, 0);
    uint8_t *src = snrt_l1alloc(4 * sizeof(*src));
    uint8_t *dst = snrt_l1alloc(6 * sizeof(*dst));
    for (unsigned i = 0; i < 4; ++i) src[i] = (uint8_t)(~(uint8_t)0 - i);
    for (unsigned i = 0; i < 6; ++i) dst[i] = 0x55;
    asm volatile("msetmtypei 1, 0, 0" ::: "memory");
    asm volatile("msettn %0, %1" : "=r"(vl) : "r"(n) : "memory");
    XCMP(20, vl, n);
    asm volatile("msettm zero, %0" :: "r"(n) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vle8.v v8, (%0)" :: "r"(src) : "v8", "memory");
    asm volatile("vtmv.t.v %0, v8" :: "r"(tss) : "memory");
    asm volatile("vtse8 %0, (%1)" :: "r"(tss), "r"(dst + 1) : "memory");
    asm volatile("fence rw, rw" ::: "memory");
    for (unsigned i = 0; i < 4; ++i) { XCMP(21, dst[i + 1], src[i]); }
    XCMP(22, dst[0], 0x55);
    XCMP(23, dst[5], 0x55);
    asm volatile("vtdiscard" ::: "memory");
}

static void TEST_CASE3(void) {
    uintptr_t n = 4, vl;
    uintptr_t tss = TSS_ROW(0, 0);
    uint16_t *src = snrt_l1alloc(4 * sizeof(*src));
    uint16_t *dst = snrt_l1alloc(6 * sizeof(*dst));
    for (unsigned i = 0; i < 4; ++i) src[i] = (uint16_t)(~(uint16_t)0 - i);
    for (unsigned i = 0; i < 6; ++i) dst[i] = 0x55;
    asm volatile("msetmtypei 1, 1, 0" ::: "memory");
    asm volatile("msettn %0, %1" : "=r"(vl) : "r"(n) : "memory");
    XCMP(30, vl, n);
    asm volatile("msettm zero, %0" :: "r"(n) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vle16.v v8, (%0)" :: "r"(src) : "v8", "memory");
    asm volatile("vtmv.t.v %0, v8" :: "r"(tss) : "memory");
    asm volatile("vtse16 %0, (%1)" :: "r"(tss), "r"(dst + 1) : "memory");
    asm volatile("fence rw, rw" ::: "memory");
    for (unsigned i = 0; i < 4; ++i) { XCMP(31, dst[i + 1], src[i]); }
    XCMP(32, dst[0], 0x55);
    XCMP(33, dst[5], 0x55);
    asm volatile("vtdiscard" ::: "memory");
}

static void TEST_CASE4(void) {
    uintptr_t n = 4, vl;
    uintptr_t tss = TSS_COL(0, 0);
    uint16_t *src = snrt_l1alloc(4 * sizeof(*src));
    uint16_t *dst = snrt_l1alloc(6 * sizeof(*dst));
    for (unsigned i = 0; i < 4; ++i) src[i] = (uint16_t)(~(uint16_t)0 - i);
    for (unsigned i = 0; i < 6; ++i) dst[i] = 0x55;
    asm volatile("msetmtypei 1, 1, 0" ::: "memory");
    asm volatile("msettn %0, %1" : "=r"(vl) : "r"(n) : "memory");
    XCMP(40, vl, n);
    asm volatile("msettm zero, %0" :: "r"(n) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vle16.v v8, (%0)" :: "r"(src) : "v8", "memory");
    asm volatile("vtmv.t.v %0, v8" :: "r"(tss) : "memory");
    asm volatile("vtse16 %0, (%1)" :: "r"(tss), "r"(dst + 1) : "memory");
    asm volatile("fence rw, rw" ::: "memory");
    for (unsigned i = 0; i < 4; ++i) { XCMP(41, dst[i + 1], src[i]); }
    XCMP(42, dst[0], 0x55);
    XCMP(43, dst[5], 0x55);
    asm volatile("vtdiscard" ::: "memory");
}

static void TEST_CASE5(void) {
    uintptr_t n = 4, vl;
    uintptr_t tss = TSS_ROW(0, 0);
    uint32_t *src = snrt_l1alloc(4 * sizeof(*src));
    uint32_t *dst = snrt_l1alloc(6 * sizeof(*dst));
    for (unsigned i = 0; i < 4; ++i) src[i] = (uint32_t)(~(uint32_t)0 - i);
    for (unsigned i = 0; i < 6; ++i) dst[i] = 0x55;
    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn %0, %1" : "=r"(vl) : "r"(n) : "memory");
    XCMP(50, vl, n);
    asm volatile("msettm zero, %0" :: "r"(n) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vle32.v v8, (%0)" :: "r"(src) : "v8", "memory");
    asm volatile("vtmv.t.v %0, v8" :: "r"(tss) : "memory");
    asm volatile("vtse32 %0, (%1)" :: "r"(tss), "r"(dst + 1) : "memory");
    asm volatile("fence rw, rw" ::: "memory");
    for (unsigned i = 0; i < 4; ++i) { XCMP(51, dst[i + 1], src[i]); }
    XCMP(52, dst[0], 0x55);
    XCMP(53, dst[5], 0x55);
    asm volatile("vtdiscard" ::: "memory");
}

static void TEST_CASE6(void) {
    uintptr_t n = 4, vl;
    uintptr_t tss = TSS_COL(0, 0);
    uint32_t *src = snrt_l1alloc(4 * sizeof(*src));
    uint32_t *dst = snrt_l1alloc(6 * sizeof(*dst));
    for (unsigned i = 0; i < 4; ++i) src[i] = (uint32_t)(~(uint32_t)0 - i);
    for (unsigned i = 0; i < 6; ++i) dst[i] = 0x55;
    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn %0, %1" : "=r"(vl) : "r"(n) : "memory");
    XCMP(60, vl, n);
    asm volatile("msettm zero, %0" :: "r"(n) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vle32.v v8, (%0)" :: "r"(src) : "v8", "memory");
    asm volatile("vtmv.t.v %0, v8" :: "r"(tss) : "memory");
    asm volatile("vtse32 %0, (%1)" :: "r"(tss), "r"(dst + 1) : "memory");
    asm volatile("fence rw, rw" ::: "memory");
    for (unsigned i = 0; i < 4; ++i) { XCMP(61, dst[i + 1], src[i]); }
    XCMP(62, dst[0], 0x55);
    XCMP(63, dst[5], 0x55);
    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();
    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
    TEST_CASE4();
    TEST_CASE5();
    TEST_CASE6();
    EXIT_CHECK();
}
