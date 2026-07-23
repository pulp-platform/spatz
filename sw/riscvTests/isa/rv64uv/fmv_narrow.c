// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Check sign extension of narrow FPR->GPR moves (fmv.x.h / fmv.x.b).

#include "vector_macros.h"

// Positive half-float: fmv.x.h must zero the upper 16 GPR bits (sign bit 0)
void TEST_CASE1(void) {
  register uint32_t in asm("a0") = 0x00003C00; // 1.0 (fp16)
  register uint32_t out asm("a1") = 0;
  asm volatile("fmv.h.x fa0, a0\n"
               "fmv.x.h a1, fa0\n"
               : "+r"(out)
               : "r"(in)
               : "fa0");
  XCMP(1, out, (uint32_t)0x00003C00);
}

// Negative half-float: fmv.x.h must set the upper 16 GPR bits (sign bit 1)
void TEST_CASE2(void) {
  register uint32_t in asm("a0") = 0x0000BC00; // -1.0 (fp16)
  register uint32_t out asm("a1") = 0;
  asm volatile("fmv.h.x fa0, a0\n"
               "fmv.x.h a1, fa0\n"
               : "+r"(out)
               : "r"(in)
               : "fa0");
  XCMP(2, out, (uint32_t)0xFFFFBC00);
}

// Positive byte-float: fmv.x.b must zero the upper 24 GPR bits
void TEST_CASE3(void) {
  register uint32_t in asm("a0") = 0x00000038; // 1.0 (fp8)
  register uint32_t out asm("a1") = 0;
  asm volatile("fmv.b.x fa0, a0\n"
               "fmv.x.b a1, fa0\n"
               : "+r"(out)
               : "r"(in)
               : "fa0");
  XCMP(3, out, (uint32_t)0x00000038);
}

// Control: fmv.x.s round-trip is width-exact on RV32
void TEST_CASE4(void) {
  register uint32_t in asm("a0") = 0x3F800000; // 1.0 (fp32)
  register uint32_t out asm("a1") = 0;
  asm volatile("fmv.s.x fa0, a0\n"
               "fmv.x.s a1, fa0\n"
               : "+r"(out)
               : "r"(in)
               : "fa0");
  XCMP(4, out, (uint32_t)0x3F800000);
}

// NaN-boxing of narrow GPR->FPR moves: read the FPR back *wider* than the
// move wrote it, so the box bits land in the GPR.

// fmv.h.x must set FPR bits [FLEN-1:16] to all 1s; fmv.x.s exposes [31:16]
void TEST_CASE5(void) {
  register uint32_t in asm("a0") = 0x00003C00; // 1.0 (fp16), sign bit 0
  register uint32_t out asm("a1") = 0;
  asm volatile("fmv.h.x fa0, a0\n"
               "fmv.x.s a1, fa0\n"
               : "+r"(out)
               : "r"(in)
               : "fa0");
  XCMP(5, out, (uint32_t)0xFFFF3C00);
}

// fmv.b.x must set FPR bits [FLEN-1:8] to all 1s; fmv.x.s exposes [31:8]
void TEST_CASE6(void) {
  register uint32_t in asm("a0") = 0x00000038; // 1.0 (fp8), sign bit 0
  register uint32_t out asm("a1") = 0;
  asm volatile("fmv.b.x fa0, a0\n"
               "fmv.x.s a1, fa0\n"
               : "+r"(out)
               : "r"(in)
               : "fa0");
  XCMP(6, out, (uint32_t)0xFFFFFF38);
}

// fmv.s.x must set FPR bits [63:32] to all 1s; fsd exposes them.
// Only meaningful (and fsd only legal) on RVD configs.
#if ELEN == 64
void TEST_CASE7(void) {
  volatile uint32_t buf[2] __attribute__((aligned(8)));
  buf[0] = 0;
  buf[1] = 0;
  uint32_t in = 0x3F800000; // 1.0 (fp32), positive: sign extension would be 0s
  asm volatile("fmv.s.x fa0, %[in]\n"
               "fsd fa0, 0(%[buf])\n"
               "fence\n"
               :
               : [in] "r"(in), [buf] "r"(buf)
               : "fa0", "memory");
  XCMP(7, buf[0], (uint32_t)0x3F800000);
  XCMP(8, buf[1], (uint32_t)0xFFFFFFFF);
}
#endif

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();
  TEST_CASE6();
#if ELEN == 64
  TEST_CASE7();
#endif

  EXIT_CHECK();
}
