// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "float_macros.h"
#include "vector_macros.h"

// Test all the different output possibilities
void TEST_CASE1(void) {
  CLEAR_FFLAGS;
  CHECK_FFLAGS(0);

  VSET(16, e16, m1);
  VLOAD_16(v4, mInfh, pInfh, qNaNh, sNaNh, 0x3b27, 0xc767, pZero, mZeroh,
           0x8075, 0x00c5, mInfh, pInfh, qNaNh, sNaNh, 0x3b27, 0xb767);
  asm volatile("vfclass.v v8, v4");
  VCMP_U16(1, v8, CLASS_mInf, CLASS_pInf, CLASS_qNAN, CLASS_sNAN, CLASS_pNorm,
           CLASS_mNorm, CLASS_pZero, CLASS_mZero, CLASS_mSub, CLASS_pSub,
           CLASS_mInf, CLASS_pInf, CLASS_qNAN, CLASS_sNAN, CLASS_pNorm,
           CLASS_mNorm);

  VSET(16, e32, m1);
  VLOAD_32(v4, mInff, pInff, qNaNf, sNaNf, 0x3f738772, 0xbdef32e4, pZero,
           mZerof, 0x80000075, 0x000000c5, mInff, pInff, qNaNf, sNaNf,
           0x3f738772, 0xbdef32e4);
  asm volatile("vfclass.v v8, v4");
  VCMP_U32(2, v8, CLASS_mInf, CLASS_pInf, CLASS_qNAN, CLASS_sNAN, CLASS_pNorm,
           CLASS_mNorm, CLASS_pZero, CLASS_mZero, CLASS_mSub, CLASS_pSub,
           CLASS_mInf, CLASS_pInf, CLASS_qNAN, CLASS_sNAN, CLASS_pNorm,
           CLASS_mNorm);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, mInfd, pInfd, qNaNd, sNaNd, 0x3def3136584672de,
           0xbdef3136584672de, pZero, mZerod, 0x8000000000000075,
           0x0000000000000c5, mInfd, pInfd, qNaNd, sNaNd, 0x3def313584672de4,
           0xbdef313654672de4);
  asm volatile("vfclass.v v8, v4");
  VCMP_U64(3, v8, CLASS_mInf, CLASS_pInf, CLASS_qNAN, CLASS_sNAN, CLASS_pNorm,
           CLASS_mNorm, CLASS_pZero, CLASS_mZero, CLASS_mSub, CLASS_pSub,
           CLASS_mInf, CLASS_pInf, CLASS_qNAN, CLASS_sNAN, CLASS_pNorm,
           CLASS_mNorm);
#endif
}

// Same, but masked (mask 0xAAAA: only odd elements active)
void TEST_CASE2(void) {
  VSET(16, e16, m1);
  VLOAD_16(v4, mInfh, pInfh, qNaNh, sNaNh, 0x3b27, 0xc767, pZero, mZeroh,
           0x8075, 0x00c5, mInfh, pInfh, qNaNh, sNaNh, 0x3b27, 0xb767);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfclass.v v8, v4, v0.t");
  VCMP_U16(4, v8, 0, CLASS_pInf, 0, CLASS_sNAN, 0, CLASS_mNorm, 0, CLASS_mZero,
           0, CLASS_pSub, 0, CLASS_pInf, 0, CLASS_sNAN, 0, CLASS_mNorm);

  VSET(16, e32, m1);
  VLOAD_32(v4, mInff, pInff, qNaNf, sNaNf, 0x3f738772, 0xbdef32e4, pZero,
           mZerof, 0x80000075, 0x000000c5, mInff, pInff, qNaNf, sNaNf,
           0x3f738772, 0xbdef32e4);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfclass.v v8, v4, v0.t");
  VCMP_U32(5, v8, 0, CLASS_pInf, 0, CLASS_sNAN, 0, CLASS_mNorm, 0, CLASS_mZero,
           0, CLASS_pSub, 0, CLASS_pInf, 0, CLASS_sNAN, 0, CLASS_mNorm);

#if ELEN == 64
  VSET(16, e64, m2);
  VLOAD_64(v4, mInfd, pInfd, qNaNd, sNaNd, 0x3def313584672de4,
           0xbdef313658467de4, pZero, mZerod, 0x8000000000000075,
           0x0000000000000c5, mInfd, pInfd, qNaNd, sNaNd, 0x3def313658672de4,
           0xbdef313654672de4);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfclass.v v8, v4, v0.t");
  VCMP_U64(6, v8, 0, CLASS_pInf, 0, CLASS_sNAN, 0, CLASS_mNorm, 0, CLASS_mZero,
           0, CLASS_pSub, 0, CLASS_pInf, 0, CLASS_sNAN, 0, CLASS_mNorm);
#endif
  // No exception should be raised by vfclass.v
  CHECK_FFLAGS(0);
}

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();

  TEST_CASE1();
  TEST_CASE2();

  EXIT_CHECK();
}