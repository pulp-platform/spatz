// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "quadrilatero_macros.h"

// Simple test, sweep over the shapes
static void st_shape(unsigned s) {
  QSET(s, QDT_INT32, 4, 4, 4);
  MLOAD_RHS_32(m0, 0x3d7ca152, 0xf30b86c9, 0x4e17da63, 0x952f71b8,
                   0x08e53a6d, 0xc4905b27, 0xfe441379, 0xabd6611c,
                   0x823fee55, 0x09bc7430, 0xcb6af721, 0x9e48d38c,
                   0x1570a74b, 0xe02d935f, 0xb439670c, 0xdf841a46);
  qfill(qc, 16, QSENTINEL);
  asm volatile("fence" ::: "memory");
  asm volatile("mst m0, (%0), %1" ::"r"(qc), "r"(16) : "memory");
  QFENCE();
  QCMP_32(1 + (int)s, qc, 0x3d7ca152, 0xf30b86c9, 0x4e17da63, 0x952f71b8,
                          0x08e53a6d, 0xc4905b27, 0xfe441379, 0xabd6611c,
                          0x823fee55, 0x09bc7430, 0xcb6af721, 0x9e48d38c,
                          0x1570a74b, 0xe02d935f, 0xb439670c, 0xdf841a46);
}

void TEST_CASE1(void) {
  st_shape(QSHAPE_1X1);
  st_shape(QSHAPE_1X2);
  st_shape(QSHAPE_2X1);
  st_shape(QSHAPE_1X4);
  st_shape(QSHAPE_4X1);
  st_shape(QSHAPE_2X2);
}

// Stride larger than the row
void TEST_CASE2(void) {
  QSET(QSHAPE_1X1, QDT_INT32, 4, 4, 4);
  MLOAD_RHS_32(m0, 0x3d7ca152, 0xf30b86c9, 0x4e17da63, 0x952f71b8,
                   0x08e53a6d, 0xc4905b27, 0xfe441379, 0xabd6611c,
                   0x823fee55, 0x09bc7430, 0xcb6af721, 0x9e48d38c,
                   0x1570a74b, 0xe02d935f, 0xb439670c, 0xdf841a46);
  qfill(qc, 24, QSENTINEL);
  asm volatile("fence" ::: "memory");
  asm volatile("mst m0, (%0), %1" ::"r"(qc), "r"(24) : "memory");
  QFENCE();
  QCMP_32(7, qc, 0x3d7ca152, 0xf30b86c9, 0x4e17da63, 0x952f71b8, QSENTINEL, QSENTINEL,
                 0x08e53a6d, 0xc4905b27, 0xfe441379, 0xabd6611c, QSENTINEL, QSENTINEL,
                 0x823fee55, 0x09bc7430, 0xcb6af721, 0x9e48d38c, QSENTINEL, QSENTINEL,
                 0x1570a74b, 0xe02d935f, 0xb439670c, 0xdf841a46, QSENTINEL, QSENTINEL);
}

// Partial M and N
void TEST_CASE3(void) {
  QSET(QSHAPE_1X1, QDT_INT32, 4, 4, 4);
  MLOAD_RHS_32(m0, 0x3d7ca152, 0xf30b86c9, 0x4e17da63, 0x952f71b8,
                   0x08e53a6d, 0xc4905b27, 0xfe441379, 0xabd6611c,
                   0x823fee55, 0x09bc7430, 0xcb6af721, 0x9e48d38c,
                   0x1570a74b, 0xe02d935f, 0xb439670c, 0xdf841a46);
  QSET(QSHAPE_1X1, QDT_INT32, 2, 3, 4);
  qfill(qc, 16, QSENTINEL);
  asm volatile("fence" ::: "memory");
  asm volatile("mst m0, (%0), %1" ::"r"(qc), "r"(16) : "memory");
  QFENCE();
  QCMP_32(8, qc, 0x3d7ca152, 0xf30b86c9, 0x4e17da63, QSENTINEL,
                 0x08e53a6d, 0xc4905b27, 0xfe441379, QSENTINEL,
                 QSENTINEL,  QSENTINEL,  QSENTINEL,  QSENTINEL,
                 QSENTINEL,  QSENTINEL,  QSENTINEL,  QSENTINEL);
}

// Large matrix (full-size 2x2)
void TEST_CASE4(void) {
  qfill_seq(qa, 4 * QROWS * QROWS, 0x40000000);
  qload_regs(4, qa);
  QSET(QSHAPE_2X2, QDT_INT32, QMAX, QMAX, QMAX);
  unsigned P = QN + 2;
  for (unsigned r = 0; r < QM; r++)
    for (unsigned c = 0; c < P; c++)
      qe[r * P + c] = c < QN ? QGROUP_WORD(qa, 0, 2, r, c) : QSENTINEL;
  qfill(qc, QM * P, QSENTINEL);
  asm volatile("fence" ::: "memory");
  asm volatile("mst m0, (%0), %1" ::"r"(qc), "r"(P * 4) : "memory");
  QFENCE();
  QCMP_INT32(9, qc, qe, QM * P);
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
