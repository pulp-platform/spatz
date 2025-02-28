// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Max Wipfli <mwipfli@ethz.ch>

#include <stdint.h>
#include <snrt.h>
#include "vector_macros.h"

uint8_t buf[] = {
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
};

static inline void *memcpy_l1(void *src, size_t len) {
  void *dst = snrt_l1alloc(len);
  memcpy(dst, src, len);
  return dst;
}

// This test case checks whether the read-after-write dependencies between
// strided loads (vlse) and arithmetic instructions (e.g., vadd) are tracked.
void TEST_CASE1(void) {
  uint8_t *l1= memcpy_l1(buf, sizeof(buf));

  VSET(8, e8, m1);

  // prime v8
  asm volatile("vmv.v.i v8, -1");

  unsigned int stride = 8;
  asm volatile("vlse8.v v8, (%0), %1" :: "r"(l1), "r"(stride));

  // for (int i = 0; i < 32; i++)
  //   asm volatile("nop");

  // arithmetic operation to move to other register
  asm volatile("vadd.vi v24, v8, 0");

  VCMP_U8(1,  v8, 1, 1, 1, 1, 1, 1, 1, 1);
  VCMP_U8(2, v24, 1, 1, 1, 1, 1, 1, 1, 1);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();

  EXIT_CHECK();
}
