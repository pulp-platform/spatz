// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Riccardo Giunti, Fondazione Chips-IT

#include "vector_macros.h"

void TEST_CASE1(void) {
  uint32_t avl, vl, vtypei;
  uint32_t golden_vtype;
  // golden_vtype = vta | vma | vsew | vlmul
  vtype(golden_vtype, 1, 0, 1, 1);
  asm volatile("vsetivli %[AVL], 16, e8, m2, ta, ma" : [AVL] "=r"(avl));
  read_vtype(vtypei);
  read_vl(vl);
  check_vtype_vl(1, vtypei, golden_vtype, avl, vl);
}

void TEST_CASE2(void) {
  uint32_t avl, vl, vtypei;
  uint32_t golden_vtype;
  // golden_vtype = vma | vta | vsew | vlmul
  vtype(golden_vtype, 1, 2, 0, 1);
  asm volatile("vsetivli %[AVL], 31, e32, m2, tu, ma" : [AVL] "=r"(avl));
  read_vtype(vtypei);
  read_vl(vl);
  check_vtype_vl(2, vtypei, golden_vtype, avl, vl);
}

// Zero avl
void TEST_CASE3(void) {
  uint32_t avl, vl, vtypei;
  uint32_t golden_vtype;
  // golden_vtype = vma | vta | vsew | vlmul
  vtype(golden_vtype, 3, 2, 0, 0);
  asm volatile("vsetivli %[AVL], 0, e32, m8, tu, mu" : [AVL] "=r"(avl));
  read_vtype(vtypei);
  read_vl(vl);
  check_vtype_vl(3, vtypei, golden_vtype, avl, vl);
}

// AVL is the 5-bit immediate
void TEST_CASE4(void) { // uimm = 0, rd != x0
  uint32_t vl;
  asm volatile("vsetivli %[vl], 0, e32, m1, ta, ma" : [vl] "=r"(vl));
  XCMP(4, vl, 0);
}

void TEST_CASE5(void) { // uimm = 0, rd = x0
  uint32_t vl, avl = 10;
  asm volatile("vsetvli %[vl], %[avl], e32, m1, ta, ma" : [vl] "=r"(vl) : [avl] "r"(avl));
  asm volatile("vsetivli zero, 0, e32, m1, ta, ma");
  read_vl(vl);
  XCMP(5, vl, 0);
}

// reserved bit zimm[8] in the vsetivli immediate
void TEST_CASE6(void) {
  uint32_t vl;
  // vsetivli rd, uimm = 4 (rs1 field = x4), zimm = 0x110: imm12 = 0b11 << 10 | 0x110 = -752
  asm volatile(".insn i 0x57, 7, %0, x4, -752" : "=r"(vl));
  XCMP(6, vl, 0);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();
  TEST_CASE6();

  EXIT_CHECK();
}
