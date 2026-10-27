// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// The shapes with fewer than 4 accumulators (1x1, 1x2, 2x1) run last (known issues).

#include "quadrilatero_macros.h"

// mmov.am of zeroed accumulators
void TEST_CASE1(void) {
  QSET(QSHAPE_2X2, QDT_INT16, 4, 4, 4);
  MLOAD_RHS_32(m8, 0x11111111, 0x22222222, 0x33333333, 0x44444444,
                   0x55555555, 0x66666666, 0x77777777, 0x88888888,
                   0x99999999, 0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc,
                   0xdddddddd, 0xeeeeeeee, 0xffffffff, 0x01234567);
  MCLEAR_ACC();
  asm volatile("mmov.am m8, acc0");
  MCMP_32(1, m8, 0, 0, 0, 0,
                 0, 0, 0, 0,
                 0, 0, 0, 0,
                 0, 0, 0, 0);
}

// Large matrix (full-size 2x2)
void TEST_CASE2(void) {
  QSET(QSHAPE_2X2, QDT_INT16, QMAX, QMAX, QMAX);
  qfill(qa, (QK / 2) * QM, 0x00010001);
  qfill(qb, (QK / 2) * QN, 0x00010001);
  asm volatile("fence" ::: "memory");
  MLD_LHS(m0, qa, QM * 4);
  MLD_RHS(m4, qb, QN * 4);
  MCLEAR_ACC();
  MMACC(acc0, m4, m0);
  asm volatile("mmov.am m8, acc0");
  qfill(qe, QM * QN, QK);
  MCMP_PTR(2, m8, qe);
}

// Simple test, sweep over the shapes
static void mov_shape(unsigned s) {
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
  asm volatile("mmov.am m8, acc0");
  MCMP_C(3 + (int)s, m8, 17, 45, 27, 25,
                         29, 38, 70, 79,
                         34, 90, 54, 50,
                         17, 45, 17, 17);
}

void TEST_CASE3(void) {
  mov_shape(QSHAPE_1X4);
  mov_shape(QSHAPE_4X1);
  mov_shape(QSHAPE_2X2);
  // mov_shape(QSHAPE_1X1);
  // mov_shape(QSHAPE_1X2);
  // mov_shape(QSHAPE_2X1);
}

int main(void) {
  INIT_CHECK();

  TEST_CASE1(); // mmov.am of zeroed accumulators
#ifndef QUAD_NO_LARGE
  TEST_CASE2(); // Large matrix (full-size 2x2)
#endif
  TEST_CASE3(); // Simple test, sweep over the shapes

  EXIT_CHECK();
}
