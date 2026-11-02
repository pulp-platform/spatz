// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Pei-Yu Lin, ETH Zürich
//
// Test cases:
//   TC1-TC6: FP32 products, accumulation, and tile interleaving.
//   TC7: FP32 full 8x8 tile accumulation (disabled).
//   TC8-TC11: FP16-to-FP32 products, accumulation, and packed K lanes.
//   TC12-TC15: BF16-to-FP32 products and altfmt selection (disabled).
//   TC16-TC18: FP8-to-FP32 format combinations (disabled).

#include <stdint.h>
#include "vector_macros.h"
#include "float_macros.h"
#include "vector_matrix_macros.h"

// Check every element so a failed tile reports its complete output row.
#define VTFMM_CMP_U32(casenum, vect, ...) do {                         \
    const uint32_t expected[] = {__VA_ARGS__};                         \
    unsigned int mismatches = 0;                                       \
    VSTORE_U32(vect);                                                    \
    for (unsigned int i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) { \
        if (Ru32[i] != expected[i]) {                                   \
            printf("[TC %d] Index %u FAILED. Got 0x%08x, expected 0x%08x.\n", \
                   (casenum), i, (unsigned)Ru32[i], (unsigned)expected[i]); \
            ++mismatches;                                               \
        }                                                               \
    }                                                                   \
    if (mismatches) ++num_failed;                                       \
    else printf("[TC %d] PASSED.\n", (casenum));                       \
} while (0)

// -------------------------------------------------------
// FP8 constants
// -------------------------------------------------------

// E4M3 (bias=7): 0 [exp:4] [mant:3]
#define FP8_E4M3_0_0    0x00  // 0.0   (0 0000 000)
#define FP8_E4M3_1_0    0x38  // 1.0   (0 0111 000)
#define FP8_E4M3_2_0    0x40  // 2.0   (0 1000 000)
#define FP8_E4M3_3_0    0x44  // 3.0   (0 1000 100)
#define FP8_E4M3_4_0    0x48  // 4.0   (0 1001 000)
#define FP8_E4M3_5_0    0x4A  // 5.0   (0 1001 010)
#define FP8_E4M3_6_0    0x4C  // 6.0   (0 1001 100)
#define FP8_E4M3_7_0    0x4E  // 7.0   (0 1001 110)
#define FP8_E4M3_8_0    0x50  // 8.0   (0 1010 000)

// E5M2 (bias=15): 0 [exp:5] [mant:2]
#define FP8_E5M2_0_0    0x00  // 0.0   (0 00000 00)
#define FP8_E5M2_1_0    0x3C  // 1.0   (0 01111 00)
#define FP8_E5M2_2_0    0x40  // 2.0   (0 10000 00)
#define FP8_E5M2_4_0    0x48  // 4.0   (impl-verified: passes TC10)

// BF16 = upper 16 bits of fp32 (bias=127, exp=8, mant=7)
#define BF16_0_0        0x0000  // 0.0
#define BF16_1_0        0x3F80  // 1.0  (fp32 0x3F800000 >> 16)
#define BF16_2_0        0x4000  // 2.0  (fp32 0x40000000 >> 16)
#define BF16_3_0        0x4040  // 3.0  (fp32 0x40400000 >> 16)
#define BF16_4_0        0x4080  // 4.0  (fp32 0x40800000 >> 16)
#define BF16_5_0        0x40A0  // 5.0  (fp32 0x40A00000 >> 16)
#define BF16_6_0        0x40C0  // 6.0  (fp32 0x40C00000 >> 16)
#define BF16_7_0        0x40E0  // 7.0  (fp32 0x40E00000 >> 16)
#define BF16_8_0        0x4100  // 8.0  (fp32 0x41000000 >> 16)

// TC1: vtfmm.tvv SEW=32, fp32×fp32→fp32 — C[0]=[5.0,7.0] C[1]=[15.0,21.0]
void TEST_CASE1(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(32, 1);

    VSET(2, e32, m1);
    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // [1.0, 3.0]
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // [5.0, 7.0]

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(13, v1, 0x40A00000, 0x40E00000);  // row0: [5.0, 7.0]
    VTFMM_CMP_U32(14, v2, 0x41700000, 0x41A80000);  // row1: [15.0, 21.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC2: vtfmm.tvv SEW=32, accumulation (×2) — C[0]=[10.0,14.0] C[1]=[30.0,42.0]
void TEST_CASE2(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(32, 1);

    VSET(2, e32, m1);
    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(15, v1, 0x41200000, 0x41600000);  // row0: [10.0, 14.0]
    VTFMM_CMP_U32(16, v2, 0x41F00000, 0x42280000);  // row1: [30.0, 42.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC3: FP32 accumulation with two explicit single-K instructions.
void TEST_CASE3(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(32, 1);

    VSET(2, e32, m1);
    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 3.0]
    VLOAD_32(v12, 0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [1.0, 3.0]
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [5.0, 7.0]
    VLOAD_32(v20, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [5.0, 7.0]

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v12, v20" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(25, v1, 0x41200000, 0x41600000);  // row0: [10.0, 14.0]
    VTFMM_CMP_U32(26, v2, 0x41F00000, 0x42280000);  // row1: [30.0, 42.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC4: FP32 accumulation with four explicit single-K instructions.
void TEST_CASE4(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(32, 1);

    VSET(2, e32, m1);
    VLOAD_32(v8,  0x3F800000, 0x00000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 0.0]
    VLOAD_32(v10, 0x00000000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [0.0, 1.0]
    VLOAD_32(v12, 0x3F800000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=2 A: [1.0, 1.0]
    VLOAD_32(v14, 0x40000000, 0x00000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=3 A: [2.0, 0.0]
    VLOAD_32(v16, 0x3F800000, 0x40000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [1.0, 2.0]
    VLOAD_32(v18, 0x40400000, 0x40800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [3.0, 4.0]
    VLOAD_32(v20, 0x3F800000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=2 B: [1.0, 1.0]
    VLOAD_32(v22, 0x3F000000, 0x3F000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=3 B: [0.5, 0.5]

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v10, v18" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v12, v20" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v14, v22" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(27, v1, 0x40400000, 0x40800000);  // row0: [3.0, 4.0]
    VTFMM_CMP_U32(28, v2, 0x40800000, 0x40A00000);  // row1: [4.0, 5.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC5: FP32 accumulation interleaved between two destination tiles.
void TEST_CASE5(void) {
    const uintptr_t TN = 2, TM = 2;

    VSET(2, e32, m1);
    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: [1.0, 3.0]
    VLOAD_32(v12, 0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A dup (k=1 of (a))
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: [5.0, 7.0]
    VLOAD_32(v20, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B dup (k=1 of (a))
    VLOAD_32(v24, 0x40000000, 0x00000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A': [2.0, 0.0]
    VLOAD_32(v26, 0x3F800000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B': [1.0, 1.0]

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"((uintptr_t)KMAX(32, 1)) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v12, v20" ::: "memory");   // Complete the second K-step.

    asm volatile("msettk x0, %0" :: "r"((uintptr_t)KMAX(32, 1)) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.tvv mt4, v24, v26" ::: "memory");  // (b) different tile
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");   // (c) back to mt0

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v3, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v4, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(29, v1, 0x41700000, 0x41A80000);  // mt0 row0: [15.0, 21.0]
    VTFMM_CMP_U32(30, v2, 0x42340000, 0x427C0000);  // mt0 row1: [45.0, 63.0]
    VTFMM_CMP_U32(31, v3, 0x40000000, 0x40000000);  // mt4 row0: [2.0, 2.0]
    VTFMM_CMP_U32(32, v4, 0x00000000, 0x00000000);  // mt4 row1: [0.0, 0.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC6: FP32 identity product with two explicit single-K instructions.
void TEST_CASE6(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(32, 1);

    VSET(2, e32, m1);
    VLOAD_32(v8,  0x3F800000, 0x00000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 0.0]
    VLOAD_32(v12, 0x00000000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [0.0, 1.0]
    VLOAD_32(v16, 0x3F800000, 0x40000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [1.0, 2.0]
    VLOAD_32(v20, 0x40400000, 0x40800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [3.0, 4.0]

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v12, v20" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(33, v1, 0x3F800000, 0x40000000);  // row0: [1.0, 2.0]
    VTFMM_CMP_U32(34, v2, 0x40400000, 0x40800000);  // row1: [3.0, 4.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC7: FP32 full 8x8 tile accumulation with two explicit K steps.
void TEST_CASE7(void) {
    const uintptr_t TN = 8, TM = 8, TK = KMAX(32, 1);

    static float A[8][2] = {
        {1, 10}, {2, 20}, {3, 30}, {4, 40},
        {5, 50}, {6, 60}, {7, 70}, {8, 80},
    };
    static float B[2][8] = {
        {1, 1, 1, 1, 1, 1, 1, 1},
        {2, 2, 2, 2, 2, 2, 2, 2},
    };
    uintptr_t a_stride = 2 * sizeof(float);

    asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"(TM) : "memory");
    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt8" ::: "memory");

    asm volatile("vlse32.v v16, (%0), %1" :: "r"(&A[0][0]), "r"(a_stride) : "v16", "memory");
    asm volatile("vlse32.v v20, (%0), %1" :: "r"(&A[0][1]), "r"(a_stride) : "v20", "memory");
    asm volatile("vle32.v v24, (%0)" :: "r"(&B[0][0]) : "v24", "memory");
    asm volatile("vle32.v v28, (%0)" :: "r"(&B[1][0]) : "v28", "memory");
    asm volatile("vtfmm.tvv mt8, v16, v24" ::: "memory");
    asm volatile("vtfmm.tvv mt8, v20, v28" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(8, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "v1", "memory");
    tss = TSS_ROW(8, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "v2", "memory");
    tss = TSS_ROW(8, 2); asm volatile("vtmv.v.t v3, %[t]" :: [t]"r"(tss) : "v3", "memory");
    tss = TSS_ROW(8, 3); asm volatile("vtmv.v.t v4, %[t]" :: [t]"r"(tss) : "v4", "memory");
    tss = TSS_ROW(8, 4); asm volatile("vtmv.v.t v5, %[t]" :: [t]"r"(tss) : "v5", "memory");
    tss = TSS_ROW(8, 5); asm volatile("vtmv.v.t v6, %[t]" :: [t]"r"(tss) : "v6", "memory");
    tss = TSS_ROW(8, 6); asm volatile("vtmv.v.t v7, %[t]" :: [t]"r"(tss) : "v7", "memory");
    tss = TSS_ROW(8, 7); asm volatile("vtmv.v.t v9, %[t]" :: [t]"r"(tss) : "v9", "memory");

    // row m value = (m+1)*21, broadcast across all 8 columns.
    VTFMM_CMP_U32(35, v1, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000);  // row0: 21
    VTFMM_CMP_U32(36, v2, 0x42280000, 0x42280000, 0x42280000, 0x42280000, 0x42280000, 0x42280000, 0x42280000, 0x42280000);  // row1: 42
    VTFMM_CMP_U32(37, v3, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000);  // row2: 63
    VTFMM_CMP_U32(38, v4, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000);  // row3: 84
    VTFMM_CMP_U32(39, v5, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000);  // row4: 105
    VTFMM_CMP_U32(40, v6, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000);  // row5: 126
    VTFMM_CMP_U32(41, v7, 0x43130000, 0x43130000, 0x43130000, 0x43130000, 0x43130000, 0x43130000, 0x43130000, 0x43130000);  // row6: 147
    VTFMM_CMP_U32(42, v9, 0x43280000, 0x43280000, 0x43280000, 0x43280000, 0x43280000, 0x43280000, 0x43280000, 0x43280000);  // row7: 168

    asm volatile("vtdiscard" ::: "memory");
}

// TC8: vtfmm.tvv SEW=16, fp16→fp32, A*I=A — C[0]=[1.0,2.0] C[1]=[3.0,4.0]
void TEST_CASE8(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(16, 2);

    // Spatz SDOTP reads {odd K, even K} from each base register word.
    VSET(2, e32, m1);
    VLOAD_32(v8,  0x40003C00, 0x44004200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: [1,2], [3,4]
    VLOAD_32(v16, 0x00003C00, 0x3C000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: [1,0], [0,1]

    asm volatile("msetmtypei 2, 1, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(1, v1, 0x3F800000, 0x40000000);   // row0: [1.0, 2.0]
    VTFMM_CMP_U32(2, v2, 0x40400000, 0x40800000);   // row1: [3.0, 4.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC9: vtfmm.tvv SEW=16, fp16→fp32, known dot product — C[0]=[17.0,23.0] C[1]=[39.0,53.0]
void TEST_CASE9(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(16, 2);

    VSET(2, e32, m1);
    VLOAD_32(v8,  0x40003C00, 0x44004200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: [1,2], [3,4]
    VLOAD_32(v16, 0x46004500, 0x48004700, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: [5,6], [7,8]

    asm volatile("msetmtypei 2, 1, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(3, v1, 0x41880000, 0x41B80000);   // row0: [17.0, 23.0]
    VTFMM_CMP_U32(4, v2, 0x421C0000, 0x42540000);   // row1: [39.0, 53.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC10: vtfmm.tvv SEW=16, fp16→fp32, accumulation (×2) — C[0]=[2.0,4.0] C[1]=[6.0,8.0]
void TEST_CASE10(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(16, 2);

    VSET(2, e32, m1);
    VLOAD_32(v8,  0x40003C00, 0x44004200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_32(v16, 0x00003C00, 0x3C000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 2, 1, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(5, v1, 0x40000000, 0x40800000);   // row0: [2.0, 4.0]
    VTFMM_CMP_U32(6, v2, 0x40C00000, 0x41000000);   // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC11: Packed FP16 K lanes across the compute-edge row and column boundary.
void TEST_CASE11(void) {
    const uintptr_t TN = 9, TM = 9, TK = KMAX(16, 2);

    VSET(9, e32, m1);
    VLOAD_32(v8,  0x40003C00,0x40004000,0x40004200,0x40004400,0x40004500,
                   0x40004600,0x40004700,0x40004800,0x40004880,0,0,0,0,0,0,0);
    VLOAD_32(v16, 0x48803C00,0x48004000,0x47004200,0x46004400,0x45004500,
                   0x44004600,0x42004700,0x40004800,0x3C004880,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 2, 1, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 8); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");
    VTFMM_CMP_U32(43, v1, 0x41980000, 0x41900000, 0x41880000, 0x41800000,
                         0x41700000, 0x41600000, 0x41500000, 0x41400000, 0x41300000);
    VTFMM_CMP_U32(44, v2, 0x41D80000, 0x42080000, 0x42240000, 0x42400000,
                         0x425C0000, 0x42780000, 0x428A0000, 0x42980000, 0x42A60000);

    asm volatile("vtdiscard" ::: "memory");
}

// TC12: vtfmm.alt.tvv SEW=16, BF16→fp32, A*I=A — C[0]=[1.0,2.0] C[1]=[3.0,4.0]
void TEST_CASE12(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(16, 2);

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_1_0, BF16_0_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B identity
    VLOAD_16(v20, BF16_0_0, BF16_1_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B identity

    asm volatile("msetmtypei 2, 1, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(7, v1, 0x3F800000, 0x40000000);   // row0: [1.0, 2.0]
    VTFMM_CMP_U32(8, v2, 0x40400000, 0x40800000);   // row1: [3.0, 4.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC13: vtfmm.alt.tvv SEW=16, BF16→fp32, known dot product — C[0]=[17.0,23.0] C[1]=[39.0,53.0]
void TEST_CASE13(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(16, 2);

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_5_0, BF16_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B
    VLOAD_16(v20, BF16_6_0, BF16_8_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B

    asm volatile("msetmtypei 2, 1, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(9,  v1, 0x41880000, 0x41B80000);  // row0: [17.0, 23.0]
    VTFMM_CMP_U32(10, v2, 0x421C0000, 0x42540000);  // row1: [39.0, 53.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC14: vtfmm.alt.tvv SEW=16, BF16→fp32, accumulation (×2) — C[0]=[2.0,4.0] C[1]=[6.0,8.0]
void TEST_CASE14(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(16, 2);

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v16, BF16_1_0, BF16_0_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v20, BF16_0_0, BF16_1_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 2, 1, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(11, v1, 0x40000000, 0x40800000);  // row0: [2.0, 4.0]
    VTFMM_CMP_U32(12, v2, 0x40C00000, 0x41000000);  // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC15: vtfmm.alt.tvv SEW=16, BF16 with altfmt=1.
void TEST_CASE15(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(16, 2);

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_5_0, BF16_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B
    VLOAD_16(v20, BF16_6_0, BF16_8_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B

    asm volatile("msetmtypei 2, 1, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(23, v1, 0x41880000, 0x41B80000);  // row0: [17.0, 23.0]
    VTFMM_CMP_U32(24, v2, 0x421C0000, 0x42540000);  // row1: [39.0, 53.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC16: vtfmm.tvv SEW=8, E4M3*E4M3 (altfmt=0) — C[0]=[5.0,7.0] C[1]=[15.0,21.0]
void TEST_CASE16(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(8, 4);

    VSET(2, e8, m1);
    // Zero unused K inputs before loading the test operands.
    asm volatile("vmv.v.i v10, 0; vmv.v.i v12, 0; vmv.v.i v14, 0; "
                 "vmv.v.i v18, 0; vmv.v.i v20, 0; vmv.v.i v22, 0"
                 ::: "v10", "v12", "v14", "v18", "v20", "v22", "memory");
    VLOAD_8(v8,  FP8_E4M3_1_0, FP8_E4M3_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A
    VLOAD_8(v16, FP8_E4M3_5_0, FP8_E4M3_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B

    asm volatile("msetmtypei 3, 0, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(17, v1, 0x40A00000, 0x40E00000);  // row0: [5.0, 7.0]
    VTFMM_CMP_U32(18, v2, 0x41700000, 0x41A80000);  // row1: [15.0, 21.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC17: vtfmm.tvv SEW=8, E4M3*E5M2 (altfmt=1) — C[0]=[2.0,4.0] C[1]=[4.0,8.0]
void TEST_CASE17(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(8, 4);

    VSET(2, e8, m1);
    // Zero unused K inputs before loading the test operands.
    asm volatile("vmv.v.i v10, 0; vmv.v.i v12, 0; vmv.v.i v14, 0; "
                 "vmv.v.i v18, 0; vmv.v.i v20, 0; vmv.v.i v22, 0"
                 ::: "v10", "v12", "v14", "v18", "v20", "v22", "memory");
    VLOAD_8(v8,  FP8_E4M3_1_0, FP8_E4M3_2_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: E4M3
    VLOAD_8(v16, FP8_E5M2_2_0, FP8_E5M2_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: E5M2

    asm volatile("msetmtypei 3, 0, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(19, v1, 0x40000000, 0x40800000);  // row0: [2.0, 4.0]
    VTFMM_CMP_U32(20, v2, 0x40800000, 0x41000000);  // row1: [4.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC18: vtfmm.alt.tvv SEW=8, E5M2*E4M3 (altfmt=0) — C[0]=[3.0,4.0] C[1]=[6.0,8.0]
void TEST_CASE18(void) {
    const uintptr_t TN = 2, TM = 2, TK = KMAX(8, 4);

    VSET(2, e8, m1);
    // Zero unused K inputs before loading the test operands.
    asm volatile("vmv.v.i v10, 0; vmv.v.i v12, 0; vmv.v.i v14, 0; "
                 "vmv.v.i v18, 0; vmv.v.i v20, 0; vmv.v.i v22, 0"
                 ::: "v10", "v12", "v14", "v18", "v20", "v22", "memory");
    VLOAD_8(v8,  FP8_E5M2_1_0, FP8_E5M2_2_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: E5M2
    VLOAD_8(v16, FP8_E4M3_3_0, FP8_E4M3_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: E4M3

    asm volatile("msetmtypei 3, 0, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VTFMM_CMP_U32(21, v1, 0x40400000, 0x40800000);  // row0: [3.0, 4.0]
    VTFMM_CMP_U32(22, v2, 0x40C00000, 0x41000000);  // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();

    // FP32 -> FP32.
    TEST_CASE1();   // Known product.
    TEST_CASE2();   // Accumulation by two.
    TEST_CASE3();   // Two explicit K steps.
    TEST_CASE4();   // Four explicit K steps.
    TEST_CASE5();   // Interleaved tile accumulation.
    TEST_CASE6();   // Identity from two K steps.
    // TEST_CASE7();   // Full 8x8 tile; enable after hardware validation.

    // FP16 -> FP32.
    TEST_CASE8();   // Identity product.
    TEST_CASE9();   // Known product.
    TEST_CASE10();  // Accumulation by two.
    TEST_CASE11();  // Packed K lanes across the compute-edge boundary.

    // Additional Zvt v0.4 formats; enable with matching hardware support.
    // TEST_CASE12();  // BF16 identity product.
    // TEST_CASE13();  // BF16 known product.
    // TEST_CASE14();  // BF16 accumulation by two.
    // TEST_CASE15();  // BF16 altfmt selection.
    // TEST_CASE16();  // E4M3 * E4M3.
    // TEST_CASE17();  // E4M3 * E5M2.
    // TEST_CASE18();  // E5M2 * E4M3.

    EXIT_CHECK();
}
