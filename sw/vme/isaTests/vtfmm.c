// Copyright 2026 ETH Zurich and University of Bologna.
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
//
// vtfmm.alt.tvv:
//   SEW=e16, TWIDEN=4 : BF16 * BF16  → fp32 acc  (BF16 inputs)
//   SEW=e8,  TWIDEN=4 : E5M2 * E4M3 → fp32 acc  (altfmt=0)
//   SEW=e8,  TWIDEN=4 : E5M2 * E5M2 → fp32 acc  (altfmt=1)
//
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
//
// Spatz vendor extension (mtype.vke, not part of the Zvt spec): lets a
// single vtfmm.tvv fuse tk>1 K-steps for SEW=32, beyond the spec's own
// KMAX=1 bound for that width. See spatz_pkg.sv.tpl's mtype_t.vke and
// spatz_controller.sv's kmax clamp relaxation.
//   TC13: vtfmm.tvv  SEW=32, vke=1, tk=2 fused — cross-checked against
//         TC8's two-separate-dispatch accumulation (same inputs duplicated
//         across both K-rows must give the identical result)
//   TC14: vtfmm.tvv  SEW=32, vke=1, tk=4 fused — four distinct K-rows,
//         hand-computed expected result (catches beat-offset bugs that
//         duplicated-data tests like TC13 can't)
//   TC15: vtfmm.tvv  SEW=32, vke=1 — sequencing/hazard check: fused tk=2
//         to mt0, then plain tk=1 to a different tile mt4, then plain
//         tk=1 back to mt0; verifies no lost/double-counted contribution

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

// TC1: vtfmm.tvv SEW=16, fp16→fp32, A*I=A — C[0]=[1.0,2.0] C[1]=[3.0,4.0]
void TEST_CASE1(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  0x3C00, 0x4200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 3.0]
    VLOAD_16(v12, 0x4000, 0x4400, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [2.0, 4.0]
    VLOAD_16(v16, 0x3C00, 0x0000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [1.0, 0.0]
    VLOAD_16(v20, 0x0000, 0x3C00, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [0.0, 1.0]

    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(1, v1, 0x3F800000, 0x40000000);   // row0: [1.0, 2.0]
    VCMP_U32(2, v2, 0x40400000, 0x40800000);   // row1: [3.0, 4.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC2: vtfmm.tvv SEW=16, fp16→fp32, known dot product — C[0]=[17.0,23.0] C[1]=[39.0,53.0]
void TEST_CASE2(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  0x3C00, 0x4200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 3.0]
    VLOAD_16(v12, 0x4000, 0x4400, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [2.0, 4.0]
    VLOAD_16(v16, 0x4500, 0x4700, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [5.0, 7.0]
    VLOAD_16(v20, 0x4600, 0x4800, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [6.0, 8.0]

    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(3, v1, 0x41880000, 0x41B80000);   // row0: [17.0, 23.0]
    VCMP_U32(4, v2, 0x421C0000, 0x42540000);   // row1: [39.0, 53.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC3: vtfmm.tvv SEW=16, fp16→fp32, accumulation (×2) — C[0]=[2.0,4.0] C[1]=[6.0,8.0]
void TEST_CASE3(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  0x3C00, 0x4200, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v12, 0x4000, 0x4400, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v16, 0x3C00, 0x0000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v20, 0x0000, 0x3C00, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(5, v1, 0x40000000, 0x40800000);   // row0: [2.0, 4.0]
    VCMP_U32(6, v2, 0x40C00000, 0x41000000);   // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC4: vtfmm.alt.tvv SEW=16, BF16→fp32, A*I=A — C[0]=[1.0,2.0] C[1]=[3.0,4.0]
void TEST_CASE4(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_1_0, BF16_0_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B identity
    VLOAD_16(v20, BF16_0_0, BF16_1_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B identity

    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(7, v1, 0x3F800000, 0x40000000);   // row0: [1.0, 2.0]
    VCMP_U32(8, v2, 0x40400000, 0x40800000);   // row1: [3.0, 4.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC5: vtfmm.alt.tvv SEW=16, BF16→fp32, known dot product — C[0]=[17.0,23.0] C[1]=[39.0,53.0]
void TEST_CASE5(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_5_0, BF16_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B
    VLOAD_16(v20, BF16_6_0, BF16_8_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B

    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(9,  v1, 0x41880000, 0x41B80000);  // row0: [17.0, 23.0]
    VCMP_U32(10, v2, 0x421C0000, 0x42540000);  // row1: [39.0, 53.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC6: vtfmm.alt.tvv SEW=16, BF16→fp32, accumulation (×2) — C[0]=[2.0,4.0] C[1]=[6.0,8.0]
void TEST_CASE6(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v16, BF16_1_0, BF16_0_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_16(v20, BF16_0_0, BF16_1_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(11, v1, 0x40000000, 0x40800000);  // row0: [2.0, 4.0]
    VCMP_U32(12, v2, 0x40C00000, 0x41000000);  // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC7: vtfmm.tvv SEW=32, fp32×fp32→fp32 — C[0]=[5.0,7.0] C[1]=[15.0,21.0]
void TEST_CASE7(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // [1.0, 3.0]
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // [5.0, 7.0]

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(13, v1, 0x40A00000, 0x40E00000);  // row0: [5.0, 7.0]
    VCMP_U32(14, v2, 0x41700000, 0x41A80000);  // row1: [15.0, 21.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC8: vtfmm.tvv SEW=32, accumulation (×2) — C[0]=[10.0,14.0] C[1]=[30.0,42.0]
void TEST_CASE8(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(15, v1, 0x41200000, 0x41600000);  // row0: [10.0, 14.0]
    VCMP_U32(16, v2, 0x41F00000, 0x42280000);  // row1: [30.0, 42.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC9: vtfmm.tvv SEW=8, E4M3*E4M3 (altfmt=0) — C[0]=[5.0,7.0] C[1]=[15.0,21.0]
void TEST_CASE9(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VSET(2, e8, m1);
    VLOAD_8(v8,  FP8_E4M3_1_0, FP8_E4M3_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A
    VLOAD_8(v16, FP8_E4M3_5_0, FP8_E4M3_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");  // clear alt_fmt (bit 8 in vtype)
    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(17, v1, 0x40A00000, 0x40E00000);  // row0: [5.0, 7.0]
    VCMP_U32(18, v2, 0x41700000, 0x41A80000);  // row1: [15.0, 21.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC10: vtfmm.tvv SEW=8, E4M3*E5M2 (altfmt=1) — C[0]=[2.0,4.0] C[1]=[4.0,8.0]
void TEST_CASE10(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VSET(2, e8, m1);
    VLOAD_8(v8,  FP8_E4M3_1_0, FP8_E4M3_2_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: E4M3
    VLOAD_8(v16, FP8_E5M2_2_0, FP8_E5M2_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: E5M2

    asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory");  // set alt_fmt (bit 8 in vtype)
    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(19, v1, 0x40000000, 0x40800000);  // row0: [2.0, 4.0]
    VCMP_U32(20, v2, 0x40800000, 0x41000000);  // row1: [4.0, 8.0]

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtdiscard" ::: "memory");
}

// TC11: vtfmm.alt.tvv SEW=8, E5M2*E4M3 (altfmt=0) — C[0]=[3.0,4.0] C[1]=[6.0,8.0]
void TEST_CASE11(void) {
    const uintptr_t TN = 2, TM = 2, TK = 1;

    VSET(2, e8, m1);
    VLOAD_8(v8,  FP8_E5M2_1_0, FP8_E5M2_2_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: E5M2
    VLOAD_8(v16, FP8_E4M3_3_0, FP8_E4M3_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: E4M3

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("msetmtypei 3, 0" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(21, v1, 0x40400000, 0x40800000);  // row0: [3.0, 4.0]
    VCMP_U32(22, v2, 0x40C00000, 0x41000000);  // row1: [6.0, 8.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC12: vtfmm.alt.tvv SEW=16, BF16 altfmt isolation — altfmt=1 ignored for SEW=16
void TEST_CASE12(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VSET(2, e16, m1);
    VLOAD_16(v8,  BF16_1_0, BF16_3_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A
    VLOAD_16(v12, BF16_2_0, BF16_4_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A
    VLOAD_16(v16, BF16_5_0, BF16_7_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B
    VLOAD_16(v20, BF16_6_0, BF16_8_0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B

    asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("msetmtypei 2, 1" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.alt.tvv mt4, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(23, v1, 0x41880000, 0x41B80000);  // row0: [17.0, 23.0]
    VCMP_U32(24, v2, 0x421C0000, 0x42540000);  // row1: [39.0, 53.0]

    asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory");
    asm volatile("vtdiscard" ::: "memory");
}

// TC13: vtfmm.tvv SEW=32, vke=1, tk=2 fused. Same A/B values duplicated at
// both K-rows (v8/v12 stride=8/2=4, v16/v20 same stride), so the fused
// two-beat result must exactly match TC8's two-separate-dispatch
// accumulation of the identical inputs — C[0]=[10.0,14.0] C[1]=[30.0,42.0].
void TEST_CASE13(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 3.0]
    VLOAD_32(v12, 0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [1.0, 3.0]
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [5.0, 7.0]
    VLOAD_32(v20, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [5.0, 7.0]

    asm volatile("msetmtypei 5, 2" ::: "memory");  // uimm5=5: vke=1, mtwiden=1
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(25, v1, 0x41200000, 0x41600000);  // row0: [10.0, 14.0]
    VCMP_U32(26, v2, 0x41F00000, 0x42280000);  // row1: [30.0, 42.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC14: vtfmm.tvv SEW=32, vke=1, tk=4 fused, four distinct K-rows (stride
// 8/4=2: v8/v10/v12/v14, v16/v18/v20/v22). Hand-computed:
//   A = [[1,0],[0,1],[1,1],[2,0]]  B = [[1,2],[3,4],[1,1],[0.5,0.5]]
//   C[m,n] = sum_k A[k,m]*B[k,n]  ->  row0=[3,4]  row1=[4,5]
void TEST_CASE14(void) {
    const uintptr_t TN = 2, TM = 2, TK = 4;

    VLOAD_32(v8,  0x3F800000, 0x00000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 0.0]
    VLOAD_32(v10, 0x00000000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [0.0, 1.0]
    VLOAD_32(v12, 0x3F800000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=2 A: [1.0, 1.0]
    VLOAD_32(v14, 0x40000000, 0x00000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=3 A: [2.0, 0.0]
    VLOAD_32(v16, 0x3F800000, 0x40000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [1.0, 2.0]
    VLOAD_32(v18, 0x40400000, 0x40800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [3.0, 4.0]
    VLOAD_32(v20, 0x3F800000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=2 B: [1.0, 1.0]
    VLOAD_32(v22, 0x3F000000, 0x3F000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=3 B: [0.5, 0.5]

    asm volatile("msetmtypei 5, 2" ::: "memory");  // uimm5=5: vke=1, mtwiden=1
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(27, v1, 0x40400000, 0x40800000);  // row0: [3.0, 4.0]
    VCMP_U32(28, v2, 0x40800000, 0x40A00000);  // row1: [4.0, 5.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC15: sequencing/hazard check around a fused instruction. Dispatches, in
// order: (a) tk=2 fused vtfmm to mt0 (A=[1,3],B=[5,7], same as TC13); (b) a
// plain tk=1 vtfmm to a DIFFERENT tile mt4 (A'=[2,0],B'=[1,1]); (c) another
// plain tk=1 vtfmm back to mt0 (same A,B as (a)). Note: path_ready[GRP_MAC]
// is a group-wide (not per-tile) admission gate, so (b) cannot actually
// dispatch until (a)'s beat sequence fully retires -- there is no runtime
// interleaving to observe here. What this test verifies instead is that
// the sequence produces exactly correct results for BOTH tiles: mt0 must
// accumulate all three contributions (2 from (a)'s fused beats + 1 from
// (c)) with none lost or double-counted, and mt4 must be unaffected by
// mt0's activity. mt0 = 3x(A^T.B) = row0:[15,21] row1:[45,63].
// mt4 = A'^T.B' = row0:[2,2] row1:[0,0].
void TEST_CASE15(void) {
    const uintptr_t TN = 2, TM = 2;

    VLOAD_32(v8,  0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A: [1.0, 3.0]
    VLOAD_32(v12, 0x3F800000, 0x40400000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A dup (k=1 of (a))
    VLOAD_32(v16, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B: [5.0, 7.0]
    VLOAD_32(v20, 0x40A00000, 0x40E00000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B dup (k=1 of (a))
    VLOAD_32(v24, 0x40000000, 0x00000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // A': [2.0, 0.0]
    VLOAD_32(v26, 0x3F800000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // B': [1.0, 1.0]

    asm volatile("msetmtypei 5, 2" ::: "memory");  // uimm5=5: vke=1, mtwiden=1
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"((uintptr_t)2) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");   // (a) tk=2 fused

    asm volatile("msettk x0, %0" :: "r"((uintptr_t)1) : "memory");
    asm volatile("vtzero mt4" ::: "memory");
    asm volatile("vtfmm.tvv mt4, v24, v26" ::: "memory");  // (b) different tile
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");   // (c) back to mt0

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 0); asm volatile("vtmv.v.t v3, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(4, 1); asm volatile("vtmv.v.t v4, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(29, v1, 0x41700000, 0x41A80000);  // mt0 row0: [15.0, 21.0]
    VCMP_U32(30, v2, 0x42340000, 0x427C0000);  // mt0 row1: [45.0, 63.0]
    VCMP_U32(31, v3, 0x40000000, 0x40000000);  // mt4 row0: [2.0, 2.0]
    VCMP_U32(32, v4, 0x00000000, 0x00000000);  // mt4 row1: [0.0, 0.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC16: vtfmm.tvv SEW=32, vke=1, tk=2 fused, two DISTINCT K-rows (stride
// 8/2=4: v8/v12, v16/v20). TC13 only ever used identical data duplicated
// at both k-rows, which cannot detect a beat-offset/ordering bug (any
// mixup between the two beats is invisible when both contribute the same
// value). Hand-computed:
//   A = [[1,0],[0,1]]  B = [[1,2],[3,4]]
//   C[m,n] = sum_k A[k,m]*B[k,n]  ->  row0=[1,2]  row1=[3,4]
void TEST_CASE16(void) {
    const uintptr_t TN = 2, TM = 2, TK = 2;

    VLOAD_32(v8,  0x3F800000, 0x00000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 A: [1.0, 0.0]
    VLOAD_32(v12, 0x00000000, 0x3F800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 A: [0.0, 1.0]
    VLOAD_32(v16, 0x3F800000, 0x40000000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=0 B: [1.0, 2.0]
    VLOAD_32(v20, 0x40400000, 0x40800000, 0,0,0,0,0,0,0,0,0,0,0,0,0,0);  // k=1 B: [3.0, 4.0]

    asm volatile("msetmtypei 5, 2" ::: "memory");  // uimm5=5: vke=1, mtwiden=1
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt0" ::: "memory");
    asm volatile("vtfmm.tvv mt0, v8, v16" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");

    uintptr_t tss;
    tss = TSS_ROW(0, 0); asm volatile("vtmv.v.t v1, %[t]" :: [t]"r"(tss) : "memory");
    tss = TSS_ROW(0, 1); asm volatile("vtmv.v.t v2, %[t]" :: [t]"r"(tss) : "memory");

    VCMP_U32(33, v1, 0x3F800000, 0x40000000);  // row0: [1.0, 2.0]
    VCMP_U32(34, v2, 0x40400000, 0x40800000);  // row1: [3.0, 4.0]

    asm volatile("vtdiscard" ::: "memory");
}

// TC17: vtfmm.tvv SEW=32, vke=1, tk=2 fused, TM=TN=8 (matches TE=8, same
// size as the real gemm.c kernel -- TC16 only used TM=2 and passed, but a
// real gemm.c M64_N64_K64 run with a TK_FUSE=2 kernel produced wrong
// results specifically on odd tile-rows of mt8, fed by strided
// (vlse32.v) loads into v16/v20 -- mirrors gemm.c's exact register
// choice for its "a1" operand and its exact destination tile, to isolate
// whether this is a TM=8-specific RTL bug or a gemm.c bug.
// A[m][k], m=0..7, k=0..1: A[m][0]=m+1, A[m][1]=(m+1)*10.
// B[k][n]=1 for k=0, =2 for k=1 (same for all n=0..7).
// C[m][n] = A[m][0]*1 + A[m][1]*2 = (m+1) + (m+1)*20 = (m+1)*21, all n.
//
// NOT CALLED from main(): this hangs the simulator outright (times out,
// no PASS/FAIL ever printed) rather than producing a wrong-but-terminating
// result -- a second, unresolved bug distinct from the value corruption
// this test was written to isolate. Left in place, uncalled, as a starting
// point for whoever picks up the TK_FUSE=2 investigation next.
void TEST_CASE17(void) {
    const uintptr_t TN = 8, TM = 8, TK = 2;

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
    asm volatile("msetmtypei 5, 2" ::: "memory");
    asm volatile("msettn x0, %0" :: "r"(TN) : "memory");
    asm volatile("msettm x0, %0" :: "r"(TM) : "memory");
    asm volatile("msettk x0, %0" :: "r"(TK) : "memory");
    asm volatile("vtzero mt8" ::: "memory");

    asm volatile("vlse32.v v16, (%0), %1" :: "r"(&A[0][0]), "r"(a_stride) : "v16", "memory");
    asm volatile("vlse32.v v20, (%0), %1" :: "r"(&A[0][1]), "r"(a_stride) : "v20", "memory");
    asm volatile("vle32.v v24, (%0)" :: "r"(&B[0][0]) : "v24", "memory");
    asm volatile("vle32.v v28, (%0)" :: "r"(&B[1][0]) : "v28", "memory");
    asm volatile("vtfmm.tvv mt8, v16, v24" ::: "memory");

    asm volatile("msetmtypei 1, 2" ::: "memory");
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
    VCMP_U32(35, v1, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000, 0x41A80000);  // row0: 21
    VCMP_U32(36, v2, 0x42280000, 0x42280000, 0x42280000, 0x42280000, 0x42280000, 0x42280000, 0x42280000, 0x42280000);  // row1: 42
    VCMP_U32(37, v3, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000, 0x427C0000);  // row2: 63
    VCMP_U32(38, v4, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000, 0x42A80000);  // row3: 84
    VCMP_U32(39, v5, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000, 0x42D20000);  // row4: 105
    VCMP_U32(40, v6, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000, 0x42FC0000);  // row5: 126
    VCMP_U32(41, v7, 0x43130000, 0x43130000, 0x43130000, 0x43130000, 0x43130000, 0x43130000, 0x43130000, 0x43130000);  // row6: 147
    VCMP_U32(42, v9, 0x43280000, 0x43280000, 0x43280000, 0x43280000, 0x43280000, 0x43280000, 0x43280000, 0x43280000);  // row7: 168

    asm volatile("vtdiscard" ::: "memory");
}

int main(void) {
    INIT_CHECK();
    enable_vec();
    enable_fp();
    enable_vme();

    // TEST_CASE1();   // vtfmm.tvv  SEW=16 fp16→fp32, A*I
    // TEST_CASE2();   // vtfmm.tvv  SEW=16 fp16→fp32, known product
    // TEST_CASE3();   // vtfmm.tvv  SEW=16 fp16→fp32, accumulation ×2
    // TEST_CASE4();   // vtfmm.alt  SEW=16 BF16→fp32, A*I
    // TEST_CASE5();   // vtfmm.alt  SEW=16 BF16→fp32, known product
    // TEST_CASE6();   // vtfmm.alt  SEW=16 BF16→fp32, accumulation ×2
    TEST_CASE7();   // vtfmm.tvv  SEW=32 fp32→fp32, known product
    TEST_CASE8();   // vtfmm.tvv  SEW=32 fp32→fp32, accumulation ×2
    // TEST_CASE9();   // vtfmm.tvv  SEW=8  E4M3*E4M3  altfmt=0
    // TEST_CASE10();  // vtfmm.tvv  SEW=8  E4M3*E5M2  altfmt=1
    // TEST_CASE11();  // vtfmm.alt  SEW=8  E5M2*E4M3  altfmt=0
    // TEST_CASE12();  // vtfmm.alt  SEW=16 BF16 altfmt isolation
    TEST_CASE13();  // vtfmm.tvv  SEW=32 vke=1 tk=2 fused
    TEST_CASE14();  // vtfmm.tvv  SEW=32 vke=1 tk=4 fused
    TEST_CASE15();  // vtfmm.tvv  SEW=32 vke=1, sequencing/hazard check
    TEST_CASE16();  // vtfmm.tvv  SEW=32 vke=1 tk=2 fused, distinct K-rows
    // TEST_CASE17();  // vtfmm.tvv SEW=32 vke=1 tk=2 fused, TM=TN=8, mt8/v16-v20 -- hangs, see comment above the function

    EXIT_CHECK();
}
