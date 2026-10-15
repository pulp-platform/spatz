// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>
//         Max Wipfli <mwipfli@ethz.ch>
//         Riccardo Giunti, Fondazione Chips-IT

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

#if ELEN == 64
  avl = 69;
  __asm__ volatile("vsetvli %[vl], %[avl], e64, m8, ta, ma" : [vl]"=r"(vl) : [avl] "r"(avl));
  XCMP(4, vl, snrt_min(avl, vlenb * 8 / 8));
#endif

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

// vill must be at bit XLEN-1 of vtype (RVV par. 3.4.4)
void TEST_CASE3() {
  uint32_t vl, vtype, avl = 16;
  uint32_t illegal_vtype = 35; // 0x00000023,
                               // vill = 0, vsew = 4 (illegal), vlmul = 3 = LMUL_8,
                               // vta = 0, vma = 0
  __asm__ volatile("vsetvl %[vl], %[avl], %[vt]"
                   : [vl] "=r"(vl) : [avl] "r"(avl), [vt] "r"(illegal_vtype));
  read_vtype(vtype);
  XCMP(9, vl, 0);
  XCMP(10, vtype, 0x80000000);
}

#if ELEN == 64
// SEW > LMUL*ELEN sets vill (RVV par. 3.4.2)
void TEST_CASE4() {
  uint32_t vl, avl = 100;
  __asm__ volatile("vsetvli %[vl], %[avl], e64, mf2, ta, ma" : [vl] "=r"(vl) : [avl] "r"(avl));
  XCMP(11, vl, 0);
}

// LMUL = 1/8 must be supported with ELEN = 64 (RVV par. 3.4.2)
void TEST_CASE5() {
  uint32_t vl, vtype, avl = 100;
  uint32_t vlenb = read_csr(vlenb);
  __asm__ volatile("vsetvli %[vl], %[avl], e8, mf8, ta, ma" : [vl] "=r"(vl) : [avl] "r"(avl));
  read_vtype(vtype);
  XCMP(12, vl, snrt_min(avl, vlenb / 8)); // VLMAX = LMUL*VLEN/SEW = VLEN/64 = vlenb/8
  XCMP(13, vtype, 0xC5);
}

// e64/mf4 is illegal (vl = 0, vtype = vill), the keep-vl then changes the ratio -> vill
void TEST_CASE6() {
  uint32_t vl, avl = 100;
  __asm__ volatile("vsetvli %[vl], %[avl], e64, mf4, ta, ma" : [vl] "=r"(vl) : [avl] "r"(avl));
  __asm__ volatile("vsetvli zero, zero, e8, m8, ta, ma");
  read_vl(vl);
  XCMP(14, vl, 0);
}
#endif

// legal keep-vl, same SEW/LMUL ratio: vl and new vtype kept
void TEST_CASE7() {
  uint32_t vl, vtype, avl = 10;
  uint32_t vlenb = read_csr(vlenb);
  __asm__ volatile("vsetvli %[vl], %[avl], e32, m1, ta, ma" : [vl] "=r"(vl) : [avl] "r"(avl));  // VLMAX = vlenb/4
  __asm__ volatile("vsetvli zero, zero, e16, mf2, ta, ma"); // 32/1 = 16/(1/2)
  read_vl(vl);
  read_vtype(vtype);
  XCMP(15, vl, snrt_min(avl, vlenb / 4));
  XCMP(16, vtype, 0xCF); // vma | vta | e16 | mf2
}

// keep-vl that changes the ratio: Spatz sets vill (reserved in RVV par. 6.2)
void TEST_CASE8() {
  uint32_t vl, avl = 10;
  __asm__ volatile("vsetvli %[vl], %[avl], e32, m1, ta, ma" : [vl] "=r"(vl) : [avl] "r"(avl));
  __asm__ volatile("vsetvli zero, zero, e32, m2, ta, ma"); // ratio 32 -> 16
  read_vl(vl);
  XCMP(17, vl, 0);
}

// context restore of an illegal vtype
void TEST_CASE9() {
  uint32_t vl, saved, avl = 4;
  uint32_t illegal_vtype = 35; // 0x00000023,
                               // vill = 0, vsew = 4 (illegal), vlmul = 3 = LMUL_8,
                               // vta = 0, vma = 0
  __asm__ volatile("vsetvl %0, %1, %2" : "=r"(vl) : "r"(avl), "r"(illegal_vtype));
  read_vtype(saved); // 0x80000000
  __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(avl)); // legal in between
  __asm__ volatile("vsetvl %0, %1, %2" : "=r"(vl) : "r"(avl), "r"(saved)); // restore
  XCMP(18, vl, 0);
}

// reserved bit zimm[8] in the vsetvli immediate
void TEST_CASE10() {
  uint32_t vl, avl = 4;
  // vsetvli rd, rs1, zimm = 0x110 (bit 8 reserved | e32 | m1): opcode 0x57, funct3 7
  __asm__ volatile(".insn i 0x57, 7, %0, %1, 0x110" : "=r"(vl) : "r"(avl));
  XCMP(19, vl, 0);
}


int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
#if ELEN == 64
  TEST_CASE4();
  TEST_CASE5();
  TEST_CASE6();
#endif
  TEST_CASE7();
  TEST_CASE8();
  TEST_CASE9();
  TEST_CASE10();

  EXIT_CHECK();
}
