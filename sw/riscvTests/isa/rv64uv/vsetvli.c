// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>
//         Max Wipfli <mwipfli@ethz.ch>

#include "encoding.h"
#include "vector_macros.h"
#include <stdint.h>

void TEST_CASE1() {
  uint32_t vl, avl;
  uint32_t vlenb = read_csr(vlenb);

  avl = 314;
  __asm__ volatile("vsetvli %[vl], %[avl], e8, m2, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  XCMP(1, vl, snrt_min(avl, vlenb * 2));

  avl = 15;
  __asm__ volatile("vsetvli %[vl], %[avl], e16, m2, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  XCMP(2, vl, snrt_min(avl, vlenb * 2 / 2));

  avl = 255;
  __asm__ volatile("vsetvli %[vl], %[avl], e32, m2, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  XCMP(3, vl, snrt_min(avl, vlenb * 2 / 4));

  avl = 69;
  __asm__ volatile("vsetvli %[vl], %[avl], e64, m8, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  XCMP(4, vl, snrt_min(avl, vlenb * 8 / 8));

  // SEW=128 not supported
  // avl = 69;
  // __asm__ volatile("vsetvli %[vl], %[avl], e128, m8, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  // XCMP(5, vl, snrt_min(avl, vlenb * 2 / 16));

  avl = 15;
  __asm__ volatile("vsetvli %[vl], %[avl], e8, m8, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  XCMP(6, vl, snrt_min(avl, vlenb * 8));

  avl = 10000;
  __asm__ volatile("vsetvli %[vl], %[avl], e16, m2, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  XCMP(7, vl, snrt_min(avl, vlenb * 2 / 2));
}

void TEST_CASE2() {
  uint32_t vl, avl;
  uint32_t vlenb = read_csr(vlenb);

  avl = 0xFFFFFFFF;
  __asm__ volatile("vsetvli %[vl], %[avl], e8, m1, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  XCMP(8, vl, vlenb);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();

  EXIT_CHECK();
}
