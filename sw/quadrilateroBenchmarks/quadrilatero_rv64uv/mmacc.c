// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// The shapes with fewer than 4 accumulators (1x1, 1x2, 2x1) run last (known issues).

#include "quadrilatero_macros.h"

// Simple test, sweep over the data types
static void mac_dt(unsigned d) {
  QSET(QSHAPE_2X2, d, 4, 4, 4);
  MLOAD_A(m0, 1, 2, 3, 4,
              5, 6, 8, 0,
              2, 4, 6, 8,
              3, 1, 0, 5);
  MLOAD_B(m4, 1, 0, 2, 3,
              4, 5, 6, 8,
              0, 1, 3, 2,
              2, 8, 1, 0);
  MCLEAR_ACC();
  asm volatile("mmacc acc0, m4, m0");
  MCMP_ACC(1 + (int)d, 17, 45, 27, 25,
                       29, 38, 70, 79,
                       34, 90, 54, 50,
                       17, 45, 17, 17);
}

void TEST_CASE1(void) {
  mac_dt(QDT_INT8);
  mac_dt(QDT_INT16);
  mac_dt(QDT_INT32);
  mac_dt(QDT_UINT8);
  mac_dt(QDT_UINT16);
  mac_dt(QDT_UINT32);
  mac_dt(QDT_FP8);
  mac_dt(QDT_FP8ALT);
  mac_dt(QDT_FP16);
  mac_dt(QDT_BF16);
  mac_dt(QDT_FP32);
  mac_dt(QDT_FP32ALT);
}

// Min / max values of the data types
static void range_dt(unsigned d) {
  unsigned sz = QDT_SIZE(d);
  int32_t a, b;
  if (QDT_FLOAT(d)) {
    a = -3;
    b = 6;
  } else if (QDT_UNSIGNED(d)) {
    a = sz == 1 ? 200 : sz == 2 ? 40000 : (int32_t)0x80000000u;
    b = sz == 4 ? 1 : 3;
  } else {
    a = sz == 1 ? -128 : sz == 2 ? -32768 : (int32_t)0x80000000u;
    b = sz == 1 ? 127 : sz == 2 ? 32767 : 1;
  }
  QSET(QSHAPE_2X2, d, 4, 4, 4);
  unsigned words = QROWS_K(d, QK) * 4; // K = 4 elements of 4 columns
  uint32_t *pa = QALLOC(words), *pb = QALLOC(words);
  qfill(pa, words, 0);
  qfill(pb, words, 0);
  qput(pa, 4, d, 0, 0, a);
  qput(pb, 4, d, 0, 0, b);
  asm volatile("fence" ::: "memory");
  MLD_LHS(m0, pa, 16);
  MLD_RHS(m4, pb, 16);
  MCLEAR_ACC();
  asm volatile("mmacc acc0, m4, m0");
  qfill(qe, 16, 0);
  qe[0] = qres(d, a * b);
  MMOV_AM(m12, acc0);
  MCMP_PTR(13 + (int)d, m12, qe);
}

void TEST_CASE2(void) {
  range_dt(QDT_INT8);
  range_dt(QDT_INT16);
  range_dt(QDT_INT32);
  range_dt(QDT_UINT8);
  range_dt(QDT_UINT16);
  range_dt(QDT_UINT32);
  range_dt(QDT_FP8);
  range_dt(QDT_FP8ALT);
  range_dt(QDT_FP16);
  range_dt(QDT_BF16);
  range_dt(QDT_FP32);
  range_dt(QDT_FP32ALT);
}

// Accumulation on a non-zero accumulator
void TEST_CASE3(void) {
  QSET(QSHAPE_2X2, QDT_INT16, 4, 4, 4);
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
  asm volatile("mmacc acc0, m4, m0");
  MCMP_ACC(25, 34,  90,  54,  50,
               58,  76, 140, 158,
               68, 180, 108, 100,
               34,  90,  34,  34);
}

// Large matrix (full-size 2x2)
void TEST_CASE4(void) {
  QSET(QSHAPE_2X2, QDT_INT16, QMAX, QMAX, QMAX);
  for (unsigned k = 0; k < QK; k++) {
    for (unsigned m = 0; m < QM; m++)
      qput(qa, QM, QD, k, m, (int32_t)(m % 3) + 1);
    for (unsigned n = 0; n < QN; n++)
      qput(qb, QN, QD, k, n, (int32_t)(n % 5) - 2);
  }
  for (unsigned m = 0; m < QM; m++)
    for (unsigned n = 0; n < QN; n++)
      qe[m * QN + n] =
          (uint32_t)((int32_t)QK * ((int32_t)(m % 3) + 1) * ((int32_t)(n % 5) - 2));
  asm volatile("fence" ::: "memory");
  MLD_LHS(m0, qa, QM * 4);
  MLD_RHS(m4, qb, QN * 4);
  MCLEAR_ACC();
  asm volatile("mmacc acc0, m4, m0");
  MMOV_AM(m12, acc0);
  MCMP_PTR(26, m12, qe);
}

// Simple test, sweep over the shapes
static void mac_shape(unsigned s) {
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
  asm volatile("mmacc acc0, m4, m0");
  MCMP_ACC(27 + (int)s, 17, 45, 27, 25,
                        29, 38, 70, 79,
                        34, 90, 54, 50,
                        17, 45, 17, 17);
}

void TEST_CASE5(void) {
  mac_shape(QSHAPE_1X4);
  mac_shape(QSHAPE_4X1);
  mac_shape(QSHAPE_2X2);
  // mac_shape(QSHAPE_1X1);
  // mac_shape(QSHAPE_1X2);
  // mac_shape(QSHAPE_2X1);
}

int main(void) {
  INIT_CHECK();

  TEST_CASE1(); // Simple test, sweep over the data types
  TEST_CASE2(); // Min / max values of the data types
  TEST_CASE3(); // Accumulation on a non-zero accumulator
#ifndef QUAD_NO_LARGE
  TEST_CASE4(); // Large matrix (full-size 2x2)
#endif
  TEST_CASE5(); // Simple test, sweep over the shapes

  EXIT_CHECK();
}
