// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"
#include <stdint.h>

// vill bit set in rs2, other fields legal
void TEST_CASE1() {
  uint32_t vl, avl = 4, vt = 0x80000000 | (2 << 3); // vill | e32 | m1
  __asm__ volatile("vsetvl %0, %1, %2" : "=r"(vl) : "r"(avl), "r"(vt));
  XCMP(1, vl, 0);
}

// reserved bit 8 set in rs2
void TEST_CASE2() {
  uint32_t vl, avl = 4, vt = (1 << 8) | (2 << 3); // reserved | e32 | m1
  __asm__ volatile("vsetvl %0, %1, %2" : "=r"(vl) : "r"(avl), "r"(vt));
  XCMP(2, vl, 0);
}

int main(void) {
  INIT_CHECK();
  enable_vec();
  uint64_t scalar = 314;
  uint64_t scalar2 = 0;
  __asm__ volatile("vsetvl t0, %[A], %[B]" ::[A] "r"(scalar), [B] "r"(scalar2));
  scalar = 15;
  scalar2 = 5;
  __asm__ volatile("vsetvl t0, %[A], %[B]" ::[A] "r"(scalar), [B] "r"(scalar2));
  scalar = 255;
  scalar2 = 10;
  __asm__ volatile("vsetvl t0, %[A], %[B]" ::[A] "r"(scalar), [B] "r"(scalar2));
  scalar = 69;
  scalar2 = 15;
  __asm__ volatile("vsetvl t0, %[A], %[B]" ::[A] "r"(scalar), [B] "r"(scalar2));
  TEST_CASE1();
  TEST_CASE2();
  EXIT_CHECK();
}