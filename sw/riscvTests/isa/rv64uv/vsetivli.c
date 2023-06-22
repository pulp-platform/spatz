// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include "vector_macros.h"

#ifndef MERGE_MODE
  void TEST_CASE1(void) {
    uint64_t avl, vtype, vl;
    uint64_t vlmul = 0;
    uint64_t vsew = 0;
    uint64_t vta = 1;
    uint64_t vma = 1;
    uint64_t golden_vtype;
    vtype(golden_vtype, vlmul, vsew, vta, vma);
    asm volatile("vsetivli %[AVL], 16, e8, m2, ta, ma" : [AVL] "=r"(avl));
    read_vtype(vtype);
    read_vl(vl);
    check_vtype_vl(1, vtype, golden_vtype, avl, vl);
  }

  void TEST_CASE2(void) {
    uint64_t avl, vtype, vl;
    uint64_t vlmul = 2;
    uint64_t vsew = 2;
    uint64_t vta = 0;
    uint64_t vma = 1;
    uint64_t golden_vtype;
    vtype(golden_vtype, vlmul, vsew, vta, vma);
    asm volatile("vsetivli %[AVL], 31, e32, m2, tu, ma" : [AVL] "=r"(avl));
    read_vtype(vtype);
    read_vl(vl);
    check_vtype_vl(2, vtype, golden_vtype, avl, vl);
  }

  // Zero avl
  void TEST_CASE3(void) {
    uint64_t avl, vtype, vl;
    uint64_t vlmul = 3;
    uint64_t vsew = 3;
    uint64_t vta = 0;
    uint64_t vma = 0;
    uint64_t golden_vtype;
    vtype(golden_vtype, vlmul, vsew, vta, vma);
    asm volatile("vsetivli %[AVL], 0, e64, m8, tu, mu" : [AVL] "=r"(avl));
    read_vtype(vtype);
    read_vl(vl);
    check_vtype_vl(3, vtype, golden_vtype, avl, vl);
  }
#endif

#ifdef MERGE_MODE
  // AVL < VL of a single Spatz
  void MERGE_TEST_CASE1(void) {
    uint64_t avl, vtype, vl;
    uint64_t vlmul = 0;
    uint64_t vsew = 0;
    uint64_t vta = 1;
    uint64_t vma = 1;
    uint64_t golden_vtype;
    vtype(golden_vtype, vlmul, vsew, vta, vma);
    asm volatile("vsetivli %[AVL], 16, e8, m2, ta, ma" : [AVL] "=r"(avl));
    read_vtype(vtype);
    vl = 16; // must match the uimm in the assembly
    //read_vl(vl); csrr-instruction not supported in merge mode
    check_vtype_vl(4, vtype, golden_vtype, avl, vl);
  }
  // AVL > VL of a single Spatz && AVL < 2*VL of both Spatzes
  void MERGE_TEST_CASE2(void) {
    uint64_t avl, vtype, vl;
    uint64_t vlmul = 1;
    uint64_t vsew = 2;
    uint64_t vta = 0;
    uint64_t vma = 1;
    uint64_t golden_vtype;
    vtype(golden_vtype, vlmul, vsew, vta, vma);
    asm volatile("vsetivli %[AVL], 24, e32, m2, tu, ma" : [AVL] "=r"(avl));
    read_vtype(vtype);
    vl = 24;
    //read_vl(vl);
    check_vtype_vl(5, vtype, golden_vtype, avl, vl);
  }

  // avl > 2*VL of both Spatzes combined
  void MERGE_TEST_CASE3(void) {
    uint64_t avl, vtype, vl;
    uint64_t vlmul = 1;
    uint64_t vsew = 3;
    uint64_t vta = 0;
    uint64_t vma = 0;
    uint64_t golden_vtype;
    vtype(golden_vtype, vlmul, vsew, vta, vma);
    asm volatile("vsetivli %[AVL], 25, e64, m8, tu, mu" : [AVL] "=r"(avl));
    read_vtype(vtype);
    vl = 25;
    //read_vl(vl);
    check_vtype_vl(6, vtype, golden_vtype, avl, vl);
  }

#endif

int main(void) {
  INIT_CHECK();
  enable_vec();

  #ifndef MERGE_MODE
    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
  #endif

  #ifdef MERGE_MODE
    MERGE_TEST_CASE1();
    MERGE_TEST_CASE2();
    MERGE_TEST_CASE3();
  #endif

  EXIT_CHECK();
}
