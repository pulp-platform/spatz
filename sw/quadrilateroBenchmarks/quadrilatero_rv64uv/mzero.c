// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// The shapes with fewer than 4 accumulators (1x1, 1x2, 2x1) run last (known issues).

#include "quadrilatero_macros.h"

// mzero.m, sweep over the shapes
static void zm_shape(unsigned s) {
  QSET(s, QDT_INT32, 4, 4, 4);
  MLOAD_RHS_32(m0, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                   0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
                   0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
                   0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15);
  asm volatile("mzero.m m0");
  MCMP_32(1 + (int)s, m0, 0, 0, 0, 0,
                          0, 0, 0, 0,
                          0, 0, 0, 0,
                          0, 0, 0, 0);
}

void TEST_CASE1(void) {
  zm_shape(QSHAPE_1X1);
  zm_shape(QSHAPE_1X2);
  zm_shape(QSHAPE_2X1);
  zm_shape(QSHAPE_1X4);
  zm_shape(QSHAPE_4X1);
  zm_shape(QSHAPE_2X2);
}

// Large matrix (full-size 2x2), mzero.m
void TEST_CASE2(void) {
  qfill_seq(qa, 4 * QROWS * QROWS, 0x50000000);
  qload_regs(4, qa);
  QSET(QSHAPE_2X2, QDT_INT32, QMAX, QMAX, QMAX);
  asm volatile("mzero.m m0");
  qfill(qe, QM * QN, 0);
  MCMP_PTR(7, m0, qe);
}

// Large matrix (full-size 2x2), mzero.a
void TEST_CASE3(void) {
  QSET(QSHAPE_2X2, QDT_INT16, QMAX, QMAX, QMAX);
  qfill(qa, (QK / 2) * QM, 0x00010001);
  qfill(qb, (QK / 2) * QN, 0x00010001);
  asm volatile("fence" ::: "memory");
  MLD_LHS(m0, qa, QM * 4);
  MLD_RHS(m4, qb, QN * 4);
  MCLEAR_ACC();
  MMACC(acc0, m4, m0);
  asm volatile("mzero.a acc0");
  MMOV_AM(m12, acc0);
  qfill(qe, QM * QN, 0);
  MCMP_PTR(8, m12, qe);
}

// mzero.a, sweep over the shapes
static void za_shape(unsigned s) {
  QSET(s, QDT_INT16, 4, 4, 4);
  MLOAD_A(m0, 1, 2, 3, 4,
              5, 6, 8, 0,
              2, 4, 6, 8,
              3, 1, 0, 5);
  MLOAD_B(m4, 1, 0, 2, 3,
              4, 5, 6, 8,
              0, 1, 3, 2,
              2, 8, 1, 0);
  MCLEAR_ACC();
  MMACC(acc0, m4, m0);
  asm volatile("mzero.a acc0");
  MCMP_ACC(9 + (int)s, 0, 0, 0, 0,
                       0, 0, 0, 0,
                       0, 0, 0, 0,
                       0, 0, 0, 0);
}

void TEST_CASE4(void) {
  za_shape(QSHAPE_1X4);
  za_shape(QSHAPE_4X1);
  za_shape(QSHAPE_2X2);
  za_shape(QSHAPE_1X1);
  za_shape(QSHAPE_1X2);
  za_shape(QSHAPE_2X1);
}

int main(void) {
  INIT_CHECK();

  TEST_CASE1(); // mzero.m, sweep over the shapes
#ifndef QUAD_NO_LARGE
  TEST_CASE2(); // Large matrix (full-size 2x2), mzero.m
  //TEST_CASE3(); // mzero.a, sweep over the shapes
#endif
  //TEST_CASE4(); Large matrix (full-size 2x2), mzero.a

  EXIT_CHECK();
}
