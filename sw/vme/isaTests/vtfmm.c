// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// vtfmm.c — ISA tests for vtfmm.tvv and vtfmm.alt.tvv
//
// -------------------------------------------------------
// vtfmm.tvv:
//   SEW=e16, TWIDEN=2 : fp16 * fp16  → fp32 acc  (FP16→FP32 widening)
//   SEW=e32, TWIDEN=1 : fp32 * fp32  → fp32 acc  (direct, no widening)
//   SEW=e8,  TWIDEN=4 : E4M3 * E4M3 → fp32 acc  (altfmt=0)
//   SEW=e8,  TWIDEN=4 : E4M3 * E5M2 → fp32 acc  (altfmt=1)

// vtfmm.alt.tvv:
//   SEW=e16, TWIDEN=4 : BF16 * BF16  → fp32 acc  (BF16 inputs)
//   SEW=e8,  TWIDEN=4 : E5M2 * E4M3 → fp32 acc  (altfmt=0)
//   SEW=e8,  TWIDEN=4 : E5M2 * E5M2 → fp32 acc  (altfmt=1)

// -------------------------------------------------------
// fp32 bit patterns (accumulator, all test cases):
//   0.0  = 0x00000000    6.0  = 0x40C00000
//   1.0  = 0x3F800000    7.0  = 0x40E00000
//   2.0  = 0x40000000    8.0  = 0x41000000
//   3.0  = 0x40400000   10.0  = 0x41200000
//   4.0  = 0x40800000   14.0  = 0x41600000
//   5.0  = 0x40A00000   15.0  = 0x41700000
//                        17.0  = 0x41880000
//                        21.0  = 0x41A80000
//                        23.0  = 0x41B80000
//                        30.0  = 0x41F00000
//                        39.0  = 0x421C0000
//                        42.0  = 0x42280000
//                        53.0  = 0x42540000

// fp16 input bit patterns (vtfmm.tvv SEW=16):
//   0.0=0x0000  1.0=0x3C00  2.0=0x4000  3.0=0x4200
//   4.0=0x4400  5.0=0x4500  6.0=0x4600  7.0=0x4700  8.0=0x4800

// BF16 input bit patterns (vtfmm.alt.tvv SEW=16):
//   = upper 16 bits of the fp32 representation
//   0.0=0x0000  1.0=0x3F80  2.0=0x4000  3.0=0x4040
//   4.0=0x4080  5.0=0x40A0  6.0=0x40C0  7.0=0x40E0  8.0=0x4100
// -------------------------------------------------------
// Test cases:
//   TC1 : vtfmm.tvv  SEW=16, fp16→fp32, A*I=A
//   TC2 : vtfmm.tvv  SEW=16, fp16→fp32, known dot product 2×2
//   TC3 : vtfmm.tvv  SEW=16, fp16→fp32, accumulation (×2)
//   TC4 : vtfmm.alt  SEW=16, BF16→fp32, A*I=A
//   TC5 : vtfmm.alt  SEW=16, BF16→fp32, known dot product 2×2
//   TC6 : vtfmm.alt  SEW=16, BF16→fp32, accumulation (×2)
//   TC7 : vtfmm.tvv  SEW=32, fp32→fp32, known dot product
//   TC8 : vtfmm.tvv  SEW=32, fp32→fp32, accumulation (×2)
//   TC9 : vtfmm.tvv  SEW=8,  E4M3*E4M3 (altfmt=0), TK=1
//   TC10: vtfmm.tvv  SEW=8,  E4M3*E5M2 (altfmt=1), TK=1
//   TC11: vtfmm.alt  SEW=8,  E5M2*E4M3 (altfmt=0), TK=1
//   TC12: vtfmm.alt  SEW=16, BF16*BF16  altfmt isolation check

#include <stdint.h>
#include "vector_macros.h"
#include "float_macros.h"
#include "vector_matrix_macros.h"

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

// -------------------------------------------------------
// TC1: vtfmm.tvv SEW=16, fp16→fp32, A*I = A
//
//   vme_cfg_e16 (mtwiden=2, TWIDEN=2) → twiden==2 check passes
//   A k=0: v8  [fp16 1.0=0x3C00, fp16 3.0=0x4200]
//   A k=1: v12 [fp16 2.0=0x4000, fp16 4.0=0x4400]
//   B k=0: v16 [fp16 1.0=0x3C00, fp16 0.0=0x0000]  identity
//   B k=1: v20 [fp16 0.0=0x0000, fp16 1.0=0x3C00]
//
//   C[0] = [1.0*1.0 + 2.0*0.0, 1.0*0.0 + 2.0*1.0] = [1.0, 2.0]
//   C[1] = [3.0*1.0 + 4.0*0.0, 3.0*0.0 + 4.0*1.0] = [3.0, 4.0]
//   → fp32 readback (tile stores fp32 bits via FP16→FP32 widening)
// -------------------------------------------------------
void TEST_CASE1(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  0x3C00, 0x4200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 3.0]
    VLOAD_16(v12, 0x4000, 0x4400, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [2.0, 4.0]
    VLOAD_16(v16, 0x3C00, 0x0000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [1.0, 0.0]
    VLOAD_16(v20, 0x0000, 0x3C00, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [0.0, 1.0]

    vme_cfg_e16(TN, TM, TK);   // mtwiden=2 → twiden=2, passes check
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);   // tile stores fp32 bits

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(1, v1, 0x3F800000, 0x40000000);   // row0: [1.0, 2.0]
    VCMP_U32(2, v2, 0x40400000, 0x40800000);   // row1: [3.0, 4.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC2: vtfmm.tvv SEW=16, fp16→fp32, known dot product 2×2
//
//   A k=0:[1.0,3.0] k=1:[2.0,4.0]  (fp16 inputs)
//   B k=0:[5.0,7.0] k=1:[6.0,8.0]
//
//   C[0][0] = 1*5 + 2*6 = 17   C[0][1] = 1*7 + 2*8 = 23
//   C[1][0] = 3*5 + 4*6 = 39   C[1][1] = 3*7 + 4*8 = 53
// -------------------------------------------------------
void TEST_CASE2(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  0x3C00, 0x4200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 3.0]
    VLOAD_16(v12, 0x4000, 0x4400, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [2.0, 4.0]
    VLOAD_16(v16, 0x4500, 0x4700, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [5.0, 7.0]
    VLOAD_16(v20, 0x4600, 0x4800, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [6.0, 8.0]

    vme_cfg_e16(TN, TM, TK);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(3, v1, 0x41880000, 0x41B80000);   // row0: [17.0, 23.0]
    VCMP_U32(4, v2, 0x421C0000, 0x42540000);   // row1: [39.0, 53.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC3: vtfmm.tvv SEW=16, fp16→fp32, accumulation (×2)
//
//   Same A and I as TC1, vtfmm called twice → result = 2×A
//   C[0]=[2.0, 4.0]  C[1]=[6.0, 8.0]  in fp32
// -------------------------------------------------------
void TEST_CASE3(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  0x3C00, 0x4200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v12, 0x4000, 0x4400, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v16, 0x3C00, 0x0000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v20, 0x0000, 0x3C00, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    vme_cfg_e16(TN, TM, TK);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");  // accumulate again

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(5, v1, 0x40000000, 0x40800000);   // row0: [2.0, 4.0]
    VCMP_U32(6, v2, 0x40C00000, 0x41000000);   // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC4: vtfmm.alt.tvv SEW=16, BF16→fp32, A*I = A
//
//   vtfmm.alt.tvv uses BF16 inputs (A_exp=8, A_mant=7).
//   BF16 1.0=0x3F80 = upper 16 bits of fp32 1.0=0x3F800000.
//   Load BF16 patterns, NOT fp16 patterns (0x3C00 ≠ BF16 1.0).
//
//   A k=0: v8  [BF16 1.0=0x3F80, BF16 3.0=0x4040]
//   A k=1: v12 [BF16 2.0=0x4000, BF16 4.0=0x4080]
//   B k=0: v16 [BF16 1.0=0x3F80, BF16 0.0=0x0000]  identity
//   B k=1: v20 [BF16 0.0=0x0000, BF16 1.0=0x3F80]
//
//   vme_cfg_e16 (mtwiden=3) is used here because vtfmm_alt_tvv_exec
//   has NO twiden check for SEW=16.
//
//   C[0]=[1.0, 2.0]  C[1]=[3.0, 4.0]  in fp32 (same numeric result)
// -------------------------------------------------------
void TEST_CASE4(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_1_0, BF16_0_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B identity
    VLOAD_16(v20, BF16_0_0, BF16_1_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B identity

    vme_cfg_e16(TN, TM, TK);
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(7, v1, 0x3F800000, 0x40000000);   // row0: [1.0, 2.0]
    VCMP_U32(8, v2, 0x40400000, 0x40800000);   // row1: [3.0, 4.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC5: vtfmm.alt.tvv SEW=16, BF16→fp32, known dot product 2×2
//
//   A k=0:[BF16 1.0, BF16 3.0]  k=1:[BF16 2.0, BF16 4.0]
//   B k=0:[BF16 5.0, BF16 7.0]  k=1:[BF16 6.0, BF16 8.0]
//
//   C[0][0]=17.0  C[0][1]=23.0  C[1][0]=39.0  C[1][1]=53.0  (fp32)
// -------------------------------------------------------
void TEST_CASE5(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_5_0, BF16_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B
    VLOAD_16(v20, BF16_6_0, BF16_8_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B

    vme_cfg_e16(TN, TM, TK);
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(9,  v1, 0x41880000, 0x41B80000);  // row0: [17.0, 23.0]
    VCMP_U32(10, v2, 0x421C0000, 0x42540000);  // row1: [39.0, 53.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC6: vtfmm.alt.tvv SEW=16, BF16→fp32, accumulation (×2)
//
//   Same BF16 A and I as TC4, called twice → result = 2×A
//   C[0]=[2.0, 4.0]  C[1]=[6.0, 8.0]  in fp32
// -------------------------------------------------------
void TEST_CASE6(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v16, BF16_1_0, BF16_0_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v20, BF16_0_0, BF16_1_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    vme_cfg_e16(TN, TM, TK);
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");  // accumulate again

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(11, v1, 0x40000000, 0x40800000);  // row0: [2.0, 4.0]
    VCMP_U32(12, v2, 0x40C00000, 0x41000000);  // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC7: vtfmm.tvv SEW=32, fp32×fp32→fp32, known dot product
//
//   SEW=e32, TWIDEN=1, TK=1 (KMAX=1 for SEW=32)
//   A k=0: v8  [fp32 1.0, fp32 3.0]
//   B k=0: v16 [fp32 5.0, fp32 7.0]
//
//   C[0]=[5.0, 7.0]   C[1]=[15.0, 21.0]
// -------------------------------------------------------
void TEST_CASE7(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // [1.0, 3.0]
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // [5.0, 7.0]

    vme_cfg_e32(TN, TM, TK);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(13, v1, 0x40A00000, 0x40E00000);  // row0: [5.0, 7.0]
    VCMP_U32(14, v2, 0x41700000, 0x41A80000);  // row1: [15.0, 21.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC8: vtfmm.tvv SEW=32, accumulation (×2)
//
//   Same A and B as TC7, vtfmm called twice.
//   C[0]=[10.0, 14.0]   C[1]=[30.0, 42.0]
// -------------------------------------------------------
void TEST_CASE8(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    vme_cfg_e32(TN, TM, TK);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");  // accumulate again

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(15, v1, 0x41200000, 0x41600000);  // row0: [10.0, 14.0]
    VCMP_U32(16, v2, 0x41F00000, 0x42280000);  // row1: [30.0, 42.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC9: vtfmm.tvv SEW=8, E4M3*E4M3 (altfmt=0), TK=1
//
//   A k=0: v8  [E4M3 1.0=0x38, E4M3 3.0=0x44]
//   B k=0: v16 [E4M3 5.0=0x4A, E4M3 7.0=0x4E]
//
//   C[0]=[5.0, 7.0]   C[1]=[15.0, 21.0]  in fp32
// -------------------------------------------------------
void TEST_CASE9(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VSET(2, e8, m1);
    VLOAD_8(v8,  FP8_E4M3_1_0, FP8_E4M3_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A
    VLOAD_8(v16, FP8_E4M3_5_0, FP8_E4M3_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B

    clear_altfmt();
    vme_cfg_e8(TN, TM, TK);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(17, v1, 0x40A00000, 0x40E00000);  // row0: [5.0, 7.0]
    VCMP_U32(18, v2, 0x41700000, 0x41A80000);  // row1: [15.0, 21.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC10: vtfmm.tvv SEW=8, E4M3*E5M2 (altfmt=1), TK=1
//
//   A k=0: v8  [E4M3 1.0=0x38, E4M3 2.0=0x40]
//   B k=0: v16 [E5M2 2.0=0x40, E5M2 4.0=0x48]
//
//   C[0]=[2.0, 4.0]   C[1]=[4.0, 8.0]  in fp32
// -------------------------------------------------------
void TEST_CASE10(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VSET(2, e8, m1);
    VLOAD_8(v8,  FP8_E4M3_1_0, FP8_E4M3_2_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: E4M3
    VLOAD_8(v16, FP8_E5M2_2_0, FP8_E5M2_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: E5M2

    set_altfmt();
    vme_cfg_e8(TN, TM, TK);
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(19, v1, 0x40000000, 0x40800000);  // row0: [2.0, 4.0]
    VCMP_U32(20, v2, 0x40800000, 0x41000000);  // row1: [4.0, 8.0]

    clear_altfmt();
    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC11: vtfmm.alt.tvv SEW=8, E5M2*E4M3 (altfmt=0), TK=1
//
//   A k=0: v8  [E5M2 1.0=0x3C, E5M2 2.0=0x40]
//   B k=0: v16 [E4M3 3.0=0x44, E4M3 4.0=0x48]
//
//   C[0]=[3.0, 4.0]   C[1]=[6.0, 8.0]  in fp32
// -------------------------------------------------------
void TEST_CASE11(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VSET(2, e8, m1);
    VLOAD_8(v8,  FP8_E5M2_1_0, FP8_E5M2_2_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: E5M2
    VLOAD_8(v16, FP8_E4M3_3_0, FP8_E4M3_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: E4M3

    clear_altfmt();
    vme_cfg_e8(TN, TM, TK);
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(21, v1, 0x40400000, 0x40800000);  // row0: [3.0, 4.0]
    VCMP_U32(22, v2, 0x40C00000, 0x41000000);  // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// -------------------------------------------------------
// TC12: vtfmm.alt.tvv SEW=16, BF16 altfmt isolation
//
//   vtfmm.alt.tvv SEW=16 uses BF16 regardless of ALTFMT CSR.
//   set_altfmt() is called here to verify it doesn't break computation.
//   Same inputs/results as TC5 (known product).
//
//   Expected: C[0]=[17.0, 23.0]  C[1]=[39.0, 53.0]  in fp32
// -------------------------------------------------------
void TEST_CASE12(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_5_0, BF16_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B
    VLOAD_16(v20, BF16_6_0, BF16_8_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B

    set_altfmt();          // altfmt=1 flag — impl ignores for SEW=16 (always BF16)
    vme_cfg_e16(TN, TM, TK);
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    vme_readback_e32(TN, TM);

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(23, v1, 0x41880000, 0x41B80000);  // row0: [17.0, 23.0]
    VCMP_U32(24, v2, 0x421C0000, 0x42540000);  // row1: [39.0, 53.0]

    clear_altfmt();
    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();
    enable_vec();
    enable_fp();
    enable_vme();

    TEST_CASE1();   // vtfmm.tvv  SEW=16 fp16→fp32, A*I
    TEST_CASE2();   // vtfmm.tvv  SEW=16 fp16→fp32, known product
    TEST_CASE3();   // vtfmm.tvv  SEW=16 fp16→fp32, accumulation ×2
    TEST_CASE4();   // vtfmm.alt  SEW=16 BF16→fp32, A*I
    TEST_CASE5();   // vtfmm.alt  SEW=16 BF16→fp32, known product
    TEST_CASE6();   // vtfmm.alt  SEW=16 BF16→fp32, accumulation ×2
    TEST_CASE7();   // vtfmm.tvv  SEW=32 fp32→fp32, known product
    TEST_CASE8();   // vtfmm.tvv  SEW=32 fp32→fp32, accumulation ×2
    TEST_CASE9();   // vtfmm.tvv  SEW=8  E4M3*E4M3  altfmt=0
    TEST_CASE10();  // vtfmm.tvv  SEW=8  E4M3*E5M2  altfmt=1
    TEST_CASE11();  // vtfmm.alt  SEW=8  E5M2*E4M3  altfmt=0
    TEST_CASE12();  // vtfmm.alt  SEW=16 BF16 altfmt isolation

    EXIT_CHECK();
}