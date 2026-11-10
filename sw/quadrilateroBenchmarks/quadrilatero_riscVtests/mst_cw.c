// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "quadrilatero_macros.h"

// Register content of the small cases (row-major 4 x 4)
static const uint32_t SRC[16] = {
    0x3d7ca152, 0xf30b86c9, 0x4e17da63, 0x952f71b8,
    0x08e53a6d, 0xc4905b27, 0xfe441379, 0xabd6611c,
    0x823fee55, 0x09bc7430, 0xcb6af721, 0x9e48d38c,
    0x1570a74b, 0xe02d935f, 0xb439670c, 0xdf841a46};

// Simple test, sweep over the shapes
static void stt_shape(unsigned s) {
  QSET(s, QDT_INT32, 4, 4, 4);
  MLOAD_RHS_32(m0, 0x3d7ca152, 0xf30b86c9, 0x4e17da63, 0x952f71b8,
                   0x08e53a6d, 0xc4905b27, 0xfe441379, 0xabd6611c,
                   0x823fee55, 0x09bc7430, 0xcb6af721, 0x9e48d38c,
                   0x1570a74b, 0xe02d935f, 0xb439670c, 0xdf841a46);
  qfill(qc, 16, QSENTINEL);
  asm volatile("fence" ::: "memory");
  asm volatile("mst.cw m0, (%0), %1" ::"r"(qc), "r"(16) : "memory");
  QFENCE();
  QCMP_32(1 + (int)s, qc, 0x3d7ca152, 0x08e53a6d, 0x823fee55, 0x1570a74b,
                          0xf30b86c9, 0xc4905b27, 0x09bc7430, 0xe02d935f,
                          0x4e17da63, 0xfe441379, 0xcb6af721, 0xb439670c,
                          0x952f71b8, 0xabd6611c, 0x9e48d38c, 0xdf841a46);
}

void TEST_CASE1(void) {
  stt_shape(QSHAPE_1X1);
  stt_shape(QSHAPE_1X2);
  stt_shape(QSHAPE_2X1);
  stt_shape(QSHAPE_1X4);
  stt_shape(QSHAPE_4X1);
  stt_shape(QSHAPE_2X2);
}

// SRC stored as m x n with pitch P words (P mod 4 selects the rotation)
static void stt_case(int casenum, unsigned m, unsigned n, unsigned P) {
  QSET(QSHAPE_1X1, QDT_INT32, 4, 4, 4);
  for (unsigned i = 0; i < 16; i++)
    qa[i] = SRC[i];
  asm volatile("fence" ::: "memory");
  MLD_RW_RHS(m0, qa, 16);
  QSET(QSHAPE_1X1, QDT_INT32, m, n, 4);
  for (unsigned c = 0; c < 4; c++)
    for (unsigned r = 0; r < P; r++)
      qe[c * P + r] = (c < QN && r < QM) ? SRC[r * 4 + c] : QSENTINEL;
  qfill(qc, 4 * P, QSENTINEL);
  asm volatile("fence" ::: "memory");
  asm volatile("mst.cw m0, (%0), %1" ::"r"(qc), "r"(P * 4) : "memory");
  QFENCE();
  QCMP_INT32(casenum, qc, qe, 4 * P);
}

// Stride larger than the row
void TEST_CASE2(void) {
  stt_case(7, 4, 4, 5);
  stt_case(8, 4, 4, 6);
  stt_case(9, 4, 4, 7);
}

// Partial M and N
void TEST_CASE3(void) {
  stt_case(10, 2, 3, 4);
  stt_case(11, 3, 2, 5);
  stt_case(12, 1, 4, 6);
  stt_case(13, 4, 1, 7);
}

// Large matrix (full-size 2x2), pitch M + off words
static void large_case(unsigned off) {
  qfill_seq(qa, 4 * QROWS * QROWS, 0x40000000);
  qload_regs(4, qa);
  QSET(QSHAPE_2X2, QDT_INT32, QMAX, QMAX, QMAX);
  unsigned P = QM + off;
  for (unsigned c = 0; c < QN; c++)
    for (unsigned r = 0; r < P; r++)
      qe[c * P + r] = r < QM ? QGROUP_WORD(qa, 0, 2, r, c) : QSENTINEL;
  qfill(qc, QN * P, QSENTINEL);
  asm volatile("fence" ::: "memory");
  asm volatile("mst.cw m0, (%0), %1" ::"r"(qc), "r"(P * 4) : "memory");
  QFENCE();
  QCMP_INT32(14 + (int)off, qc, qe, QN * P);
}

void TEST_CASE4(void) {
  large_case(0);
  large_case(1);
  large_case(2);
  large_case(3);
}

int main(void) {
  INIT_CHECK();

  TEST_CASE1(); // Simple test, sweep over the shapes
  TEST_CASE2(); // Stride larger than the row
  TEST_CASE3(); // Partial M and N
#ifndef QUAD_NO_LARGE
  TEST_CASE4(); // Large matrix (full-size 2x2)
#endif

  EXIT_CHECK();
}
