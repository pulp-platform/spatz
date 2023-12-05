// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include "vector_macros.h"

// Naive test
void TEST_CASE1(void) {
  VSET(16, e8, m4);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U8(1, v24, 73);

  VSET(16, e16, m4);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U16(2, v24, 73);

  VSET(16, e32, m4);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U32(3, v24, 73);

#if ELEN == 64
  VSET(16, e64, m4);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v8, 1);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U64(4, v24, 73);
#endif
}

// Masked naive test
void TEST_CASE2(void) {
  VSET(16, e8, m4);
  VLOAD_8(v0, 0xaa, 0x55);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1);
  VLOAD_8(v24, 1);
  asm volatile("vredsum.vs v24, v16, v8, v0.t");
  VCMP_U8(5, v24, 37);

  VSET(16, e16, m4);
  VLOAD_8(v0, 0xaa, 0x55);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1);
  VLOAD_16(v24, 1);
  asm volatile("vredsum.vs v24, v16, v8, v0.t");
  VCMP_U16(6, v24, 37);

  VSET(16, e32, m4);
  VLOAD_8(v0, 0xaa, 0x55);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1);
  VLOAD_32(v24, 1);
  asm volatile("vredsum.vs v24, v16, v8, v0.t");
  VCMP_U32(7, v24, 37);

#if ELEN == 64
  VSET(16, e64, m4);
  VLOAD_8(v0, 0xaa, 0x55);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v8, 1);
  VLOAD_64(v24, 1);
  asm volatile("vredsum.vs v24, v16, v8, v0.t");
  VCMP_U64(8, v24, 37);
#endif
}

// Are we respecting the undisturbed tail policy?
void TEST_CASE3(void) {
  VSET(16, e8, m4);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U8(9, v24, 73, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(16, e16, m4);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U16(10, v24, 73, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(16, e32, m4);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U32(11, v24, 73, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

#if ELEN == 64
  VSET(16, e64, m4);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U64(12, v24, 73, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
#endif
}

// Odd number of elements, undisturbed policy
void TEST_CASE4(void) {
  VSET(15, e8, m4);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U8(13, v24, 65, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(1, e16, m4);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U16(14, v24, 2, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(3, e32, m4);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U32(15, v24, 7, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

#if ELEN == 64
  VSET(7, e64, m4);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U64(16, v24, 29, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(15, e64, m4);
  VLOAD_64(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_64(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8");
  VCMP_U64(17, v24, 65, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
#endif
}

// Odd number of elements, undisturbed policy, and mask
void TEST_CASE5(void) {
  VSET(15, e8, m4);
  VLOAD_8(v0, 0x00, 0x40);
  VLOAD_8(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v8, 100, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_8(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8, v0.t");
  VCMP_U8(18, v24, 107, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

  VSET(1, e16, m4);
  VLOAD_8(v0, 0xaa, 0x55);
  VLOAD_16(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_16(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8, v0.t");
  VCMP_U16(19, v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);

#if ELEN == 64
  VSET(3, e32, m4);
  VLOAD_8(v0, 0xaa, 0x55);
  VLOAD_32(v16, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  VLOAD_32(v24, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
  asm volatile("vredsum.vs v24, v16, v8, v0.t");
  VCMP_U32(20, v24, 3, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  // TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  // TEST_CASE5();

  EXIT_CHECK();
}
