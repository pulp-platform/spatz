// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include "vector_macros.h"

// Naive test
void TEST_CASE1(void) {
  VSET(16, e8, m8);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 0, 1, 9, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1);
  asm volatile("vredmin.vs v24, v16, v8");
  VCMP_U8(1, v24, 0);

  VSET(16, e16, m8);
  VLOAD_16(v16, 1, 2, -3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 0);
  asm volatile("vredmin.vs v24, v16, v8");
  VCMP_U16(2, v24, -3);

  VSET(16, e32, m8);
  VLOAD_32(v16, 9, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, -1);
  asm volatile("vredmin.vs v24, v16, v8");
  VCMP_U32(3, v24, -1);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_64(v16, -1, 2, 3, 4, 5, -6, 7, -9, -1, -2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v8, -1);
  asm volatile("vredmin.vs v24, v16, v8");
  VCMP_U64(4, v24, -9);
#endif
}

// Masked naive test
void TEST_CASE2(void) {
  VSET(16, e8, m8);
  VLOAD_8(v0, 0x03, 0x00);
  VLOAD_8(v16, 1, -2, 3, 4, 5, 6, 7, 9, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1);
  VLOAD_8(v24, 1);
  asm volatile("vredmin.vs v24, v16, v8, v0.t");
  VCMP_U8(5, v24, -2);

  VSET(16, e16, m8);
  VLOAD_8(v0, 0x00, 0xc0);
  VLOAD_16(v16, -1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 3);
  VLOAD_16(v24, 1);
  asm volatile("vredmin.vs v24, v16, v8, v0.t");
  VCMP_U16(6, v24, 3);

  VSET(16, e32, m8);
  VLOAD_8(v0, 0x00, 0xc0);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 8);
  VLOAD_32(v24, 1);
  asm volatile("vredmin.vs v24, v16, v8, v0.t");
  VCMP_U32(7, v24, 7);

#if ELEN == 64
  VSET(16, e64, m8);
  VLOAD_8(v0, 0xaa, 0x55);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v8, 4);
  VLOAD_64(v24, 1);
  asm volatile("vredmin.vs v24, v16, v8, v0.t");
  VCMP_U64(8, v24, 1);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  // TEST_CASE2();

  EXIT_CHECK();
}
