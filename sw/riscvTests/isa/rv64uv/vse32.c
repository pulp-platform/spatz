// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

#include "vector_macros.h"


//**********Checking functionality of vse32********//
void TEST_CASE1(void) {
  volatile uint32_t ALIGNED_I32[16];
  VSET(16, e32, m2);
  VLOAD_32(v2, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  asm volatile("vse32.v v2, (%0)" ::"r"(ALIGNED_I32));
  VVCMP_U32(1, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}

//*******Checking functionality of vse32 with different values of masking
// register******//
void TEST_CASE2(void) {
  volatile uint32_t ALIGNED_I32[16]={0};
  VSET(16, e32, m2);
  VLOAD_32(v2, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  VLOAD_8(v0, 0xFF, 0xFF);
  asm volatile("vse32.v v2, (%0), v0.t" ::"r"(ALIGNED_I32));
  VCLEAR(v2);
  VVCMP_U32(2, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}

void TEST_CASE3(void) {
  volatile uint32_t ALIGNED_I32[16];
  VSET(16, e32, m4);
  VLOAD_32(v4, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse32.v v4, (%0)" ::"r"(ALIGNED_I32));
  VCLEAR(v4);
  VLOAD_32(v4, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  VLOAD_8(v0, 0x00, 0x00);
  asm volatile("vse32.v v4, (%0), v0.t" ::"r"(ALIGNED_I32));
  VCLEAR(v4);
  VVCMP_U32(3, ALIGNED_I32, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16);
}

void TEST_CASE4(void) {
  volatile uint32_t ALIGNED_I32[16];
  VSET(16, e32, m8);
  VLOAD_32(v8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse32.v v8, (%0)" ::"r"(ALIGNED_I32));
  VCLEAR(v8);
  VLOAD_32(v8,  0x11111111, 0x22222222, 0x33333333, 0x44444444,
           0x55555555, 0x66666666, 0x77777777, 0x88888888, 0x99999999,
           0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd, 0xeeeeeeee,
           0xffffffff,0x00000000);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vse32.v v8, (%0), v0.t" ::"r"(ALIGNED_I32));
  VCLEAR(v8);
  VVCMP_U32(4, ALIGNED_I32, 1, 0x22222222, 3, 0x44444444, 5, 0x66666666, 7,
            0x88888888, 9, 0xaaaaaaaa, 11, 0xcccccccc, 13, 0xeeeeeeee, 15,
            0x00000000);
}

// change LMUL and EW
void TEST_CASE5(void) {
  volatile uint32_t ALIGNED_I32[16] = {0};
  VSET(16, e32, m2);
  VLOAD_32(v8, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  VSET(16, e8, m8); // ? uncertain
  asm volatile("vse32.v v8, (%0)" ::"r"(ALIGNED_I32));
  VCLEAR(v8);
  VVCMP_U32(5, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}


void TEST_CASE6(void) {
  VSET(64, e32, m8);
  volatile uint32_t ALIGNED_I32[64];

  //init mem
  for (int i = 0; i < 64; i++) ALIGNED_I32[i] = 0xFFFFFFFF;

  VLOAD_32(v8,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
               17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
               33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
               49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);

  VCLEAR(v0);
  VLOAD_32(v0, 0xAAAAAAAA, 0xAAAAAAAA);

  asm volatile("vse32.v v8, (%0), v0.t" ::"r"(ALIGNED_I32));

  VVCMP_U32(6, ALIGNED_I32,
    0xFFFFFFFF,  2, 0xFFFFFFFF,  4, 0xFFFFFFFF,  6, 0xFFFFFFFF,  8, 0xFFFFFFFF, 10, 0xFFFFFFFF, 12, 0xFFFFFFFF, 14, 0xFFFFFFFF, 16,
    0xFFFFFFFF, 18, 0xFFFFFFFF, 20, 0xFFFFFFFF, 22, 0xFFFFFFFF, 24, 0xFFFFFFFF, 26, 0xFFFFFFFF, 28, 0xFFFFFFFF, 30, 0xFFFFFFFF, 32,
    0xFFFFFFFF, 34, 0xFFFFFFFF, 36, 0xFFFFFFFF, 38, 0xFFFFFFFF, 40, 0xFFFFFFFF, 42, 0xFFFFFFFF, 44, 0xFFFFFFFF, 46, 0xFFFFFFFF, 48,
    0xFFFFFFFF, 50, 0xFFFFFFFF, 52, 0xFFFFFFFF, 54, 0xFFFFFFFF, 56, 0xFFFFFFFF, 58, 0xFFFFFFFF, 60, 0xFFFFFFFF, 62, 0xFFFFFFFF, 64);
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