// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "quadrilatero_macros.h"

// RLEN/32 and maxima of M and N in 1x1
void TEST_CASE1(void) {
  unsigned m, n;
#ifdef QUAD_RLEN
  QXCMP(1, QROWS, QUAD_RLEN / 32);
#else
  QXCMP(1, (QROWS & (QROWS - 1)) == 0 && QROWS >= 4, 1);
#endif
  asm volatile("mcfgm %0, %1, 0" : "=r"(m) : "r"(QMAX));
  QXCMP(2, m, QROWS);
  asm volatile("mcfgn %0, %1, 0" : "=r"(n) : "r"(QMAX));
  QXCMP(3, n, QROWS);
}

// Max K for every data type
void TEST_CASE2(void) {
  unsigned k;
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_INT32), "i"(DT_INT8), "i"(DT_INT8));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(4, k, 4 * QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_INT32), "i"(DT_INT16), "i"(DT_INT16));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(5, k, 2 * QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_INT32), "i"(DT_INT32), "i"(DT_INT32));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(6, k, QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_UINT32), "i"(DT_UINT8), "i"(DT_UINT8));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(7, k, 4 * QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_UINT32), "i"(DT_UINT16), "i"(DT_UINT16));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(8, k, 2 * QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_UINT32), "i"(DT_UINT32), "i"(DT_UINT32));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(9, k, QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_FP32), "i"(DT_FP8), "i"(DT_FP8));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(10, k, 4 * QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_FP32), "i"(DT_FP8ALT), "i"(DT_FP8ALT));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(11, k, 4 * QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_FP32), "i"(DT_FP16), "i"(DT_FP16));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(12, k, 2 * QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_FP32), "i"(DT_FP16ALT), "i"(DT_FP16ALT));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(13, k, 2 * QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_FP32), "i"(DT_FP32), "i"(DT_FP32));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(14, k, QROWS);
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_FP32), "i"(DT_FP32ALT), "i"(DT_FP32ALT));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QMAX));
  QXCMP(15, k, QROWS);
}

// Requests below the maximum
void TEST_CASE3(void) {
  unsigned m, n, k;
  asm volatile("mmac.dt %0, %1, %2" ::"i"(DT_INT32), "i"(DT_INT32), "i"(DT_INT32));
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(1));
  QXCMP(16, k, 1);
  asm volatile("mcfgk %0, %1" : "=r"(k) : "r"(QROWS - 1));
  QXCMP(17, k, QROWS - 1);
  asm volatile("mcfgm %0, %1, 0" : "=r"(m) : "r"(1));
  QXCMP(18, m, 1);
  asm volatile("mcfgn %0, %1, 0" : "=r"(n) : "r"(QROWS - 1));
  QXCMP(19, n, QROWS - 1);
  asm volatile("mcfgm %0, %1, 1" : "=r"(m) : "r"(QROWS + 1)); // valid only with rmul = 2
  QXCMP(20, m, QROWS + 1);
}

// rmul / cmul x2 and x4, saturation
void TEST_CASE4(void) {
  unsigned m, n;
  asm volatile("mcfgn %0, %1, 0" : "=r"(n) : "r"(QMAX));
  asm volatile("mcfgm %0, %1, 1" : "=r"(m) : "r"(QMAX));
  QXCMP(21, m, 2 * QROWS);
  asm volatile("mcfgm %0, %1, 1" : "=r"(m) : "r"(2 * QROWS + 1));
  QXCMP(22, m, 2 * QROWS);
  asm volatile("mcfgm %0, %1, 3" : "=r"(m) : "r"(QMAX)); // 4x1
  QXCMP(23, m, 4 * QROWS);

  asm volatile("mcfgm %0, %1, 0" : "=r"(m) : "r"(QMAX));
  asm volatile("mcfgn %0, %1, 1" : "=r"(n) : "r"(QMAX));
  QXCMP(24, n, 2 * QROWS);
  asm volatile("mcfgn %0, %1, 1" : "=r"(n) : "r"(2 * QROWS + 1));
  QXCMP(25, n, 2 * QROWS);
  asm volatile("mcfgn %0, %1, 3" : "=r"(n) : "r"(QMAX)); // 1x4
  QXCMP(26, n, 4 * QROWS);
}

int main(void) {
  INIT_CHECK();

  TEST_CASE1(); // RLEN/32 and maxima of M and N in 1x1
  TEST_CASE2(); // Max K for every data type
  TEST_CASE3(); // Requests below the maximum
  TEST_CASE4(); // rmul / cmul x2 and x4, saturation

  EXIT_CHECK();
}
