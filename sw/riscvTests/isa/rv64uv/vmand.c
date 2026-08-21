// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

void TEST_CASE1() {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0xCD, 0xEF);
  VLOAD_8(v3, 0x84, 0x21);
  asm volatile("vmand.mm v1, v2, v3");
  VSET(16, e8, m1);
  VCMP_U8(1, v1, 0x84, 0x21);
}

void TEST_CASE2() {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0xCD, 0xEF);
  VLOAD_8(v3, 0xFF, 0xFF);
  asm volatile("vmand.mm v1, v2, v3");
  VSET(16, e8, m1);
  VCMP_U8(2, v1, 0xCD, 0xEF);
}

void TEST_CASE3() {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0xCD, 0xEF);
  VLOAD_8(v3, 0x00, 0x00);
  asm volatile("vmand.mm v1, v2, v3");
  VSET(16, e8, m1);
  VCMP_U8(3, v1, 0x00, 0x00);
}

void TEST_CASE4() {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0xCD, 0xEF);
  VLOAD_8(v3, 0x0F, 0xF0);
  asm volatile("vmand.mm v1, v2, v3");
  VSET(16, e8, m1);
  VCMP_U8(4, v1, 0x0D, 0xE0);
}

void TEST_CASE5() {
  VSET(16, e8, m1);
  VLOAD_8(v1, 0xFF, 0xFF);
  VLOAD_8(v2, 0xCD, 0xEF);
  VLOAD_8(v3, 0x84, 0x21);
  VSET(16, e8, m1);
  asm volatile("vmand.mm v1, v2, v3");
  VSET(16, e8, m1);
  VCMP_U8(5, v1, 0x84, 0x21);
}

void TEST_CASE6() {
  VSET(16, e8, m1);
  VLOAD_8(v2, 0xCD, 0xEF, 0xCD, 0xEF, 0xCD, 0xEF, 0xCD, 0xEF);
  VLOAD_8(v3, 0x84, 0x21, 0x84, 0x21, 0x84, 0x21, 0x84, 0x21);
  asm volatile("vmand.mm v1, v2, v3");
  VSET(13, e8, m1);
  VCLEAR(v2);
  VCMP_U8(6, v2, 0, 0, 0, 0, 0, 0, 0, 0);
}

void TEST_CASE7() {
  uint8_t *buf  = (uint8_t *)snrt_l1alloc(64 * sizeof(uint8_t));
  uint8_t *gold = (uint8_t *)snrt_l1alloc(64 * sizeof(uint8_t));
  printf("[TC 7] \n");
  VSET(64, e8, m1);
  asm volatile("vmv.v.x v9, %0" :: "r"(0xBB));
  asm volatile("vmv.v.x v8, %0" :: "r"(0x77));
  VSET(128, e8, m4);
  asm volatile("vmv.v.x v4,  %0" :: "r"(0xCD));
  asm volatile("vmv.v.x v12, %0" :: "r"(0x84));
  asm volatile("vmand.mm v8, v4, v12");
  memset(gold, 0x84, 16);
  memset(gold + 16, 0x77, 48);
  VSET(64, e8, m1);
  asm volatile("vse8.v v8, (%0)" :: "r"(buf) : "memory");
  VMCMP(uint8_t, "%hhu", 7, buf, gold, 64);
  memset(gold, 0xBB, 64);
  VSET(64, e8, m1);
  asm volatile("vse8.v v9, (%0)" :: "r"(buf) : "memory");
  VMCMP(uint8_t, "%hhu", 7, buf, gold, 64);
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
  TEST_CASE7();

  EXIT_CHECK();
}
