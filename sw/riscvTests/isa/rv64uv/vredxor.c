// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include "vector_macros.h"

// Naive test
void TEST_CASE1(void) {
  VSET(4, e8, m8);
  VLOAD_8(v16, 0x00, 0x01, 0x01, 0x00);
  VLOAD_8(v8, 0x11);
  asm volatile("vredxor.vs v24, v16, v8");
  VCMP_U8(1, v24, 0x11);

  VSET(4, e16, m8);
  VLOAD_16(v16, 0x8000, 0x0301, 0x0101, 0x0001);
  VLOAD_16(v8, 0xe001);
  asm volatile("vredxor.vs v24, v16, v8");
  VCMP_U16(2, v24, 0x6200);

  VSET(4, e32, m8);
  VLOAD_32(v16, 0x00000001, 0x10000001, 0x00000000, 0x00000000);
  VLOAD_32(v8, 0x00001000);
  asm volatile("vredxor.vs v24, v16, v8");
  VCMP_U32(3, v24, 0x10001000);

#if ELEN == 64
  VSET(4, e64, m8);
  VLOAD_64(v16, 0x0000000000000000, 0x1000000000000001, 0x0000000000000000,
           0x0000000000000000);
  VLOAD_64(v8, 0x0000000000000007);
  asm volatile("vredxor.vs v24, v16, v8");
  VCMP_U64(4, v24, 0x1000000000000006);
#endif
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();

  EXIT_CHECK();
}
