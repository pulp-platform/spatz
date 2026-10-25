// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "quadrilatero_macros.h"

// Expected registers: 1, 2 or 4 memory rows loaded (K = 4 elements of 1, 2, 4 bytes)
static const uint32_t SRC_1ROW[16] = {
    0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0};
static const uint32_t SRC_2ROWS[16] = {
    0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
    0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
    0, 0, 0, 0,
    0, 0, 0, 0};
static const uint32_t SRC_4ROWS[16] = {
    0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
    0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
    0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
    0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15};

// mld.rhs, sweep over the shapes
static void rhs_shape(unsigned s) {
  QSET(s, QDT_INT32, 4, 4, 4);
  QDATA_32(src, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
                0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
                0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15);
  asm volatile("mld.rhs m0, (%0), %1" ::"r"(src), "r"(16) : "memory");
  MCMP_32(1 + (int)s, m0, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                          0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
                          0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
                          0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15);
}

void TEST_CASE1(void) {
  rhs_shape(QSHAPE_1X1);
  rhs_shape(QSHAPE_1X2);
  rhs_shape(QSHAPE_2X1);
  rhs_shape(QSHAPE_1X4);
  rhs_shape(QSHAPE_4X1);
  rhs_shape(QSHAPE_2X2);
}

// mld.lhs, sweep over the shapes
static void lhs_shape(unsigned s) {
  QSET(s, QDT_INT32, 4, 4, 4);
  QDATA_32(src, 0x00000001, 0x00000002, 0x00000003, 0x00000004,
                0x10000000, 0x20000000, 0x30000000, 0x40000000,
                0xffffffff, 0x80000000, 0x7fffffff, 0x00000000,
                0x0000ffff, 0xffff0000, 0x00ff00ff, 0xff00ff00);
  asm volatile("mld.lhs m0, (%0), %1" ::"r"(src), "r"(16) : "memory");
  MCMP_32(7 + (int)s, m0, 0x00000001, 0x00000002, 0x00000003, 0x00000004,
                          0x10000000, 0x20000000, 0x30000000, 0x40000000,
                          0xffffffff, 0x80000000, 0x7fffffff, 0x00000000,
                          0x0000ffff, 0xffff0000, 0x00ff00ff, 0xff00ff00);
}

void TEST_CASE2(void) {
  lhs_shape(QSHAPE_1X1);
  lhs_shape(QSHAPE_1X2);
  lhs_shape(QSHAPE_2X1);
  lhs_shape(QSHAPE_1X4);
  lhs_shape(QSHAPE_4X1);
  lhs_shape(QSHAPE_2X2);
}

// Stride larger than the row
void TEST_CASE3(void) {
  QSET(QSHAPE_1X1, QDT_INT32, 4, 4, 4);
  QDATA_32(src, 0x11111111, 0x22222222, 0x33333333, 0x44444444, 0xbad00000, 0xbad00001,
                0x55555555, 0x66666666, 0x77777777, 0x88888888, 0xbad00002, 0xbad00003,
                0x99999999, 0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xbad00004, 0xbad00005,
                0xdddddddd, 0xeeeeeeee, 0xffffffff, 0x01234567, 0xbad00006, 0xbad00007);
  asm volatile("mld.rhs m0, (%0), %1" ::"r"(src), "r"(24) : "memory");
  MCMP_32(13, m0, 0x11111111, 0x22222222, 0x33333333, 0x44444444,
                  0x55555555, 0x66666666, 0x77777777, 0x88888888,
                  0x99999999, 0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc,
                  0xdddddddd, 0xeeeeeeee, 0xffffffff, 0x01234567);
  asm volatile("mld.lhs m1, (%0), %1" ::"r"(src), "r"(24) : "memory");
  MCMP_32(14, m1, 0x11111111, 0x22222222, 0x33333333, 0x44444444,
                  0x55555555, 0x66666666, 0x77777777, 0x88888888,
                  0x99999999, 0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc,
                  0xdddddddd, 0xeeeeeeee, 0xffffffff, 0x01234567);
}

// mld.rhs, sweep over the data types
static void rhs_dt(unsigned d) {
  QSET(QSHAPE_1X1, d, 4, 4, 4);
  QDATA_32(src, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
                0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
                0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15);
  asm volatile("mld.rhs m0, (%0), %1" ::"r"(src), "r"(16) : "memory");
  MCMP_PTR(15 + (int)d, m0,
           QDT_SIZE(d) == 1 ? SRC_1ROW : QDT_SIZE(d) == 2 ? SRC_2ROWS : SRC_4ROWS);
}

// mld.lhs, sweep over the data types
static void lhs_dt(unsigned d) {
  QSET(QSHAPE_1X1, d, 4, 4, 4);
  QDATA_32(src, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
                0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
                0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15);
  asm volatile("mld.lhs m0, (%0), %1" ::"r"(src), "r"(16) : "memory");
  MCMP_PTR(27 + (int)d, m0,
           QDT_SIZE(d) == 1 ? SRC_1ROW : QDT_SIZE(d) == 2 ? SRC_2ROWS : SRC_4ROWS);
}

void TEST_CASE4(void) {
  rhs_dt(QDT_INT8);
  rhs_dt(QDT_INT16);
  rhs_dt(QDT_INT32);
  rhs_dt(QDT_UINT8);
  rhs_dt(QDT_UINT16);
  rhs_dt(QDT_UINT32);
  rhs_dt(QDT_FP8);
  rhs_dt(QDT_FP8ALT);
  rhs_dt(QDT_FP16);
  rhs_dt(QDT_BF16);
  rhs_dt(QDT_FP32);
  rhs_dt(QDT_FP32ALT);
}

void TEST_CASE5(void) {
  lhs_dt(QDT_INT8);
  lhs_dt(QDT_INT16);
  lhs_dt(QDT_INT32);
  lhs_dt(QDT_UINT8);
  lhs_dt(QDT_UINT16);
  lhs_dt(QDT_UINT32);
  lhs_dt(QDT_FP8);
  lhs_dt(QDT_FP8ALT);
  lhs_dt(QDT_FP16);
  lhs_dt(QDT_BF16);
  lhs_dt(QDT_FP32);
  lhs_dt(QDT_FP32ALT);
}

// Large matrix (full-size 2x2)
void TEST_CASE6(void) {
  QSET(QSHAPE_2X2, QDT_INT32, QMAX, QMAX, QMAX);
  unsigned R = QROWS, W = QN; // M = N = 2R, K = R
  qfill_seq(qa, QK * W, 0x10000000);
  for (unsigned r = 0; r < QM; r++)
    for (unsigned c = 0; c < QN; c++)
      qe[r * QN + c] = r < R ? qa[r * W + c] : 0;
  asm volatile("fence" ::: "memory");

  MZERO_M(m0);
  asm volatile("mld.rhs m0, (%0), %1" ::"r"(qa), "r"(W * 4) : "memory");
  MCMP_PTR(39, m0, qe);

  MZERO_M(m4);
  asm volatile("mld.lhs m4, (%0), %1" ::"r"(qa), "r"(W * 4) : "memory");
  MCMP_PTR(40, m4, qe);
}

int main(void) {
  INIT_CHECK();

  TEST_CASE1(); // mld.rhs, sweep over the shapes
  TEST_CASE2(); // mld.lhs, sweep over the shapes
  TEST_CASE3(); // Stride larger than the row
  TEST_CASE4(); // mld.rhs, sweep over the data types
  TEST_CASE5(); // mld.lhs, sweep over the data types
#ifndef QUAD_NO_LARGE
  TEST_CASE6(); // Large matrix (full-size 2x2)
#endif

  EXIT_CHECK();
}
