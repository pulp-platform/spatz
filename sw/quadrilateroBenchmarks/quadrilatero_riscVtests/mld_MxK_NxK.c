// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//

#include "quadrilatero_macros.h"

// Expected registers: 1, 2 or 4 words of each memory row loaded (K = 4 elements of 1, 2, 4 bytes)
static const uint32_t SRCT_1ROW[16] = {
    0x9fe41920, 0xa11a9384, 0x7c3a5e21, 0xe6527d49,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0};
static const uint32_t SRCT_2ROWS[16] = {
    0x9fe41920, 0xa11a9384, 0x7c3a5e21, 0xe6527d49,
    0x8f2e05e0, 0xa7163840, 0x1b04d6f8, 0x0f83a9c4,
    0, 0, 0, 0,
    0, 0, 0, 0};
static const uint32_t SRCT_4ROWS[16] = {
    0x9fe41920, 0xa11a9384, 0x7c3a5e21, 0xe6527d49,
    0x8f2e05e0, 0xa7163840, 0x1b04d6f8, 0x0f83a9c4,
    0xf9aa71f0, 0x99991348, 0x5e9d3a07, 0x4d16b03e,
    0xc394bbd3, 0xa9f38cd1, 0x30c8f1b2, 0x82fa6c15};

// Element k of memory row x in a transposed buffer of P words per row
static inline void qput_t(uint32_t *buf, unsigned P, unsigned d, unsigned x, unsigned k,
                          uint32_t e) {
  uint8_t *p = (uint8_t *)buf + x * P * 4 + k * QDT_SIZE(d);
  for (unsigned i = 0; i < QDT_SIZE(d); i++)
    p[i] = (uint8_t)(e >> (8 * i));
}

// mld.cw.rhs, sweep over the shapes
static void rhs_shape(unsigned s) {
  QSET(s, QDT_INT32, 4, 4, 4);
  QDATA_32(src, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
                0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
                0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15);
  asm volatile("mld.cw.rhs m0, (%0), %1" ::"r"(src), "r"(16) : "memory");
  MCMP_32(1 + (int)s, m0, 0x9fe41920, 0xa11a9384, 0x7c3a5e21, 0xe6527d49,
                          0x8f2e05e0, 0xa7163840, 0x1b04d6f8, 0x0f83a9c4,
                          0xf9aa71f0, 0x99991348, 0x5e9d3a07, 0x4d16b03e,
                          0xc394bbd3, 0xa9f38cd1, 0x30c8f1b2, 0x82fa6c15);
}

void TEST_CASE1(void) {
  rhs_shape(QSHAPE_1X1);
  rhs_shape(QSHAPE_1X2);
  rhs_shape(QSHAPE_2X1);
  rhs_shape(QSHAPE_1X4);
  rhs_shape(QSHAPE_4X1);
  rhs_shape(QSHAPE_2X2);
}

// mld.rw.lhs, sweep over the shapes
static void lhs_shape(unsigned s) {
  QSET(s, QDT_INT32, 4, 4, 4);
  QDATA_32(src, 0x00000001, 0x00000002, 0x00000003, 0x00000004,
                0x10000000, 0x20000000, 0x30000000, 0x40000000,
                0xffffffff, 0x80000000, 0x7fffffff, 0x00000000,
                0x0000ffff, 0xffff0000, 0x00ff00ff, 0xff00ff00);
  asm volatile("mld.rw.lhs m0, (%0), %1" ::"r"(src), "r"(16) : "memory");
  MCMP_32(7 + (int)s, m0, 0x00000001, 0x10000000, 0xffffffff, 0x0000ffff,
                          0x00000002, 0x20000000, 0x80000000, 0xffff0000,
                          0x00000003, 0x30000000, 0x7fffffff, 0x00ff00ff,
                          0x00000004, 0x40000000, 0x00000000, 0xff00ff00);
}

void TEST_CASE2(void) {
  lhs_shape(QSHAPE_1X1);
  lhs_shape(QSHAPE_1X2);
  lhs_shape(QSHAPE_2X1);
  lhs_shape(QSHAPE_1X4);
  lhs_shape(QSHAPE_4X1);
  lhs_shape(QSHAPE_2X2);
}

// Stride larger than the row: pitch P words (P mod 4 selects the rotation)
static void stride_case(int casenum, unsigned P) {
  QSET(QSHAPE_1X1, QDT_INT32, 4, 4, 4);
  for (unsigned x = 0; x < 4; x++)
    for (unsigned w = 0; w < P; w++)
      qa[x * P + w] = w < 4 ? SRCT_4ROWS[w * 4 + x] : 0xbad00000 + x * P + w;
  asm volatile("fence" ::: "memory");
  asm volatile("mld.cw.rhs m0, (%0), %1" ::"r"(qa), "r"(P * 4) : "memory");
  MCMP_PTR(casenum, m0, SRCT_4ROWS);
  asm volatile("mld.rw.lhs m1, (%0), %1" ::"r"(qa), "r"(P * 4) : "memory");
  MCMP_PTR(casenum + 1, m1, SRCT_4ROWS);
}

void TEST_CASE3(void) {
  stride_case(13, 5);
  stride_case(15, 6);
  stride_case(17, 7);
}

// mld.cw.rhs, sweep over the data types
static void rhs_dt(unsigned d) {
  QSET(QSHAPE_1X1, d, 4, 4, 4);
  QDATA_32(src, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
                0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
                0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15);
  asm volatile("mld.cw.rhs m0, (%0), %1" ::"r"(src), "r"(16) : "memory");
  MCMP_PTR(19 + (int)d, m0,
           QDT_SIZE(d) == 1 ? SRCT_1ROW : QDT_SIZE(d) == 2 ? SRCT_2ROWS : SRCT_4ROWS);
}

// mld.rw.lhs, sweep over the data types
static void lhs_dt(unsigned d) {
  QSET(QSHAPE_1X1, d, 4, 4, 4);
  QDATA_32(src, 0x9fe41920, 0x8f2e05e0, 0xf9aa71f0, 0xc394bbd3,
                0xa11a9384, 0xa7163840, 0x99991348, 0xa9f38cd1,
                0x7c3a5e21, 0x1b04d6f8, 0x5e9d3a07, 0x30c8f1b2,
                0xe6527d49, 0x0f83a9c4, 0x4d16b03e, 0x82fa6c15);
  asm volatile("mld.rw.lhs m0, (%0), %1" ::"r"(src), "r"(16) : "memory");
  MCMP_PTR(31 + (int)d, m0,
           QDT_SIZE(d) == 1 ? SRCT_1ROW : QDT_SIZE(d) == 2 ? SRCT_2ROWS : SRCT_4ROWS);
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

// Partial K (bytes after K = 0), M = N = m, memory rows >= m != 0; full: check the whole register
static void part_k(int casenum, unsigned d, unsigned k, unsigned m, unsigned full) {
  const unsigned P = 3;
  QSET(QSHAPE_1X1, d, m, m, k);
  unsigned L = full ? QROWS : 4, W = QROWS_K(d, QK);
  for (unsigned i = 0; i < L * P; i++)
    qa[i] = i / P >= QM ? 0xc3c3c3c3 : (i % P) < W ? 0 : 0xbad00000 + i;
  for (unsigned x = 0; x < QM; x++)
    for (unsigned kk = 0; kk < QK; kk++)
      qput_t(qa, P, d, x, kk, 0x8100 + (x << 4) + kk + 1);
  for (unsigned r = 0; r < L; r++)
    for (unsigned x = 0; x < L; x++)
      qe[r * L + x] = (r < W && x < QM) ? qa[x * P + r] : 0;
  asm volatile("fence" ::: "memory");
  MZERO_M(m0);
  asm volatile("mld.cw.rhs m0, (%0), %1" ::"r"(qa), "r"(P * 4) : "memory");
  MZERO_M(m1);
  asm volatile("mld.rw.lhs m1, (%0), %1" ::"r"(qa), "r"(P * 4) : "memory");
  QSET(QSHAPE_1X1, QDT_INT32, L, L, 4);
  MCMP_PTR(casenum, m0, qe);
  MCMP_PTR(casenum + 1, m1, qe);
}

void TEST_CASE6(void) {
  part_k(43, QDT_INT8, 1, 4, 0);
  part_k(45, QDT_INT8, 2, 4, 0);
  part_k(47, QDT_INT8, 3, 4, 0);
  part_k(49, QDT_INT8, 5, 4, 0);
  part_k(51, QDT_INT8, 6, 4, 0);
  part_k(53, QDT_INT8, 7, 4, 0);
  part_k(55, QDT_INT16, 1, 4, 0);
  part_k(57, QDT_INT16, 3, 4, 0);
}

// Large matrix (full-size 2x2), pitch K + off words
static void large_case(unsigned off) {
  QSET(QSHAPE_2X2, QDT_INT32, QMAX, QMAX, QMAX);
  unsigned R = QROWS, W = QK + off; // M = N = 2R, K = R
  for (unsigned x = 0; x < QN; x++)
    for (unsigned w = 0; w < W; w++)
      qa[x * W + w] = w < QK ? 0x10000000 + x * QK + w : QSENTINEL;
  for (unsigned r = 0; r < QM; r++)
    for (unsigned c = 0; c < QN; c++)
      qe[r * QN + c] = r < R ? qa[c * W + r] : 0;
  asm volatile("fence" ::: "memory");

  MZERO_M(m0);
  asm volatile("mld.cw.rhs m0, (%0), %1" ::"r"(qa), "r"(W * 4) : "memory");
  MCMP_PTR(59 + (int)off, m0, qe);

  MZERO_M(m4);
  asm volatile("mld.rw.lhs m4, (%0), %1" ::"r"(qa), "r"(W * 4) : "memory");
  MCMP_PTR(63 + (int)off, m4, qe);
}

void TEST_CASE7(void) {
  large_case(0);
  large_case(1);
  large_case(2);
  large_case(3);
}

// Partial K with M, N smaller than the lanes, whole register (K = 4 int8: control case)
void TEST_CASE8(void) {
  part_k(67, QDT_INT8, 4, 2, 1);
  part_k(69, QDT_INT8, 5, 2, 1);
  part_k(71, QDT_INT8, 7, QROWS - 1, 1);
  part_k(73, QDT_INT16, 3, 2, 1);
}

int main(void) {
  INIT_CHECK();

  TEST_CASE1(); // mld.cw.rhs, sweep over the shapes
  TEST_CASE2(); // mld.rw.lhs, sweep over the shapes
  TEST_CASE3(); // Stride larger than the row
  TEST_CASE4(); // mld.cw.rhs, sweep over the data types
  TEST_CASE5(); // mld.rw.lhs, sweep over the data types
  TEST_CASE6(); // Partial K
#ifndef QUAD_NO_LARGE
  TEST_CASE7(); // Large matrix (full-size 2x2)
#endif
  //TEST_CASE8(); // Partial K with M, N smaller than the lanes (known issue)

  EXIT_CHECK();
}
