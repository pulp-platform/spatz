// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1(void) {
  uint32_t avl, vtype, vl;
  uint32_t vlmul = 0;
  uint32_t vsew = 0;
  uint32_t vta = 1;
  uint32_t vma = 1;
  uint32_t golden_vtype;
  vtype(golden_vtype, vlmul, vsew, vta, vma);
  asm volatile("vsetivli %[AVL], 16, e8, m1, ta, ma" : [AVL] "=r"(avl));
  read_vtype(vtype);
  read_vl(vl);
  check_vtype_vl(1, vtype, golden_vtype, avl, vl);
}

void TEST_CASE2(void) {
  uint32_t avl, vtype, vl;
  uint32_t vlmul = 2;
  uint32_t vsew = 2;
  uint32_t vta = 0;
  uint32_t vma = 1;
  uint32_t golden_vtype;
  vtype(golden_vtype, vlmul, vsew, vta, vma);
  asm volatile("vsetivli %[AVL], 31, e32, m4, tu, ma" : [AVL] "=r"(avl));
  read_vtype(vtype);
  read_vl(vl);
  check_vtype_vl(2, vtype, golden_vtype, avl, vl);
}

// Zero avl
void TEST_CASE3(void) {
  uint32_t avl, vtype, vl;
  uint32_t vlmul = 3;
  uint32_t vsew = 1;
  uint32_t vta = 0;
  uint32_t vma = 0;
  uint32_t golden_vtype;
  vtype(golden_vtype, vlmul, vsew, vta, vma);
  asm volatile("vsetivli %[AVL], 0, e16, m8, tu, mu" : [AVL] "=r"(avl));
  read_vtype(vtype);
  read_vl(vl);
  check_vtype_vl(3, vtype, golden_vtype, avl, vl);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();

  EXIT_CHECK();
}
