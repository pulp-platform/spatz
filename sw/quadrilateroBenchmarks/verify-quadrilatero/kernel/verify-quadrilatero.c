// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata
#include <stdio.h>

#define MUL_1 0
#define MUL_2 1
#define MUL_4 3
#define FP32  0b00110

void __attribute__ ((noinline)) test_cfg (){
  asm volatile("mmac.dt %0, %1, %2":: "i"(FP32), "i"(FP32), "i"(FP32));
  // --------------------------------------------------------------------------
  asm volatile("li t1,  74");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("li t1,  75");
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("li t1,  76");
  asm volatile("mcfgk t0, t1");
  // --------------------------------------------------------------------------
  asm volatile("li t1,  4");
  asm volatile("li t2,  8");
  asm volatile("li t3, 16");
  asm volatile("li t4, 32");
  asm volatile("li t5, 17");
  asm volatile("li t6,  3");
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t2, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t2, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t2"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t3, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t3, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t3"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t4, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t4, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t4"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t5, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t5, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t5"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t6, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t6, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t6"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t2, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t2, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t2"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t3, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t3, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t3"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t4, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t4, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t4"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t5, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t5, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t5"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t6, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t6, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t6"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t2, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t2, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t2"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t3, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t3, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t3"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t4, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t4, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t4"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t5, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t5, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t5"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t6, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t6, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t6"   );
  // --------------------------------------------------------------------------
  asm volatile("mcfgm t0, t2, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t2, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t2"   );
}

void __attribute__ ((noinline)) test_load_1x1 (float* addrA, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  4");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m1, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m2, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m3, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m5, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m7, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m9, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m10, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m11, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m13, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m15, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m1            "   );
  asm volatile("mzero.m     m2            "   );
  asm volatile("mzero.m     m3            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m5            "   );
  asm volatile("mzero.m     m6            "   );
  asm volatile("mzero.m     m7            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m9            "   );
  asm volatile("mzero.m     m10           "   );
  asm volatile("mzero.m     m11           "   );
  asm volatile("mzero.m     m12           "   );
  asm volatile("mzero.m     m13           "   );
  asm volatile("mzero.m     m14           "   );
  asm volatile("mzero.m     m15           "   );
}
void __attribute__ ((noinline)) test_load_1x2 (float* addrA, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m2, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m3, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m7, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m10, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m11, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m15, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m2            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m6            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m10           "   );
  asm volatile("mzero.m     m12           "   );
  asm volatile("mzero.m     m14           "   );
}
void __attribute__ ((noinline)) test_load_1x4 (float* addrA, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m5, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m7, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m13, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m15, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m12           "   );
}
void __attribute__ ((noinline)) test_load_2x1 (float* addrA, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m2, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m3, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m7, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m10, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m11, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m15, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m2            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m6            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m10           "   );
  asm volatile("mzero.m     m12           "   );
  asm volatile("mzero.m     m14           "   );
}
void __attribute__ ((noinline)) test_load_4x1 (float* addrA, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("mld.rhs     m0, (t1) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m5, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m7, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m13, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m15, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m12           "   );
}
void __attribute__ ((noinline)) test_load_2x2 (float* addrA, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m2, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m10, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.rhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m12           "   );
}

void __attribute__ ((noinline)) test_store_1x1 (float* addrA, float* addrC, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  4");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi        t4,  %0 ,  0 " :: "r" (addrC));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  asm volatile("mst         m0, (t4) , t3 "   );
  asm volatile("mzero.m     m0            "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));

  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1,  4");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  asm volatile("addi        t4,  %0  ,  0 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m1, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m2, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m3, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m5, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m7, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m9, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m10, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m11, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m13, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m15, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m1            "   );
  asm volatile("mzero.m     m2            "   );
  asm volatile("mzero.m     m3            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m5            "   );
  asm volatile("mzero.m     m6            "   );
  asm volatile("mzero.m     m7            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m9            "   );
  asm volatile("mzero.m     m10           "   );
  asm volatile("mzero.m     m11           "   );
  asm volatile("mzero.m     m12           "   );
  asm volatile("mzero.m     m13           "   );
  asm volatile("mzero.m     m14           "   );
  asm volatile("mzero.m     m15           "   );
}
void __attribute__ ((noinline)) test_store_1x2 (float* addrA, float* addrC, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi        t4,  %0 ,  0 " :: "r" (addrC));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  asm volatile("mld.lhs     m1, (t1) , t3 "   );
  asm volatile("mst         m0, (t4) , t3 "   );
  asm volatile("mzero.m     m0            "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));

  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  asm volatile("addi        t4,  %0  ,  0 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m2, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m8, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m10, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m2            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m6            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m10           "   );
  asm volatile("mzero.m     m12           "   );
  asm volatile("mzero.m     m14           "   );
}
void __attribute__ ((noinline)) test_store_1x4 (float* addrA, float* addrC, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi        t4,  %0 ,  0 " :: "r" (addrC));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  asm volatile("mld.lhs     m1, (t1) , t3 "   );
  asm volatile("mld.lhs     m2, (t1) , t3 "   );
  asm volatile("mld.lhs     m3, (t1) , t3 "   );
  asm volatile("mst         m0, (t4) , t3 "   );
  asm volatile("mzero.m     m0            "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));

  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t1"   );
  asm volatile("addi        t4,  %0  ,  0 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m8, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m12           "   );
}
void __attribute__ ((noinline)) test_store_2x1 (float* addrA, float* addrC, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi        t4,  %0 ,  0 " :: "r" (addrC));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  asm volatile("mst         m0, (t4) , t3 "   );
  asm volatile("mzero.m     m0            "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));

  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  asm volatile("addi        t4,  %0  ,  0 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m2, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m6, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m8, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m10, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m14, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m2            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m6            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m10           "   );
  asm volatile("mzero.m     m12           "   );
  asm volatile("mzero.m     m14           "   );
}
void __attribute__ ((noinline)) test_store_4x1 (float* addrA, float* addrC, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi        t4,  %0 ,  0 " :: "r" (addrC));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  asm volatile("mst         m0, (t4) , t3 "   );
  asm volatile("mzero.m     m0            "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));

  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  asm volatile("addi        t4,  %0  ,  0 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m8, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m12           "   );
}
void __attribute__ ((noinline)) test_store_2x2 (float* addrA, float* addrC, int K)
{
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi        t4,  %0 ,  0 " :: "r" (addrC));
  asm volatile("mld.lhs     m0, (t1) , t3 "   );
  asm volatile("mld.rhs     m2, (t1) , t3 "   );
  asm volatile("mst         m0, (t4) , t3 "   );
  asm volatile("mzero.m     m0            "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("slli        t3,  %0  ,  2 " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi        t1,  %0  ,  0 " :: "r" (addrA));

  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs     m8, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t1  , t2 "   );
  asm volatile("mld.lhs    m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1,  16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  asm volatile("addi        t4,  %0  ,  0 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m0, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m4, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi        t2,  zero,  0 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst         m8, (t0) , t3 "   );
  // -------------------------------------------------------------------------- 
  asm volatile("addi        t2,  t2  , 16 "   );
  asm volatile("add         t0,  t4  , t2 "   );
  asm volatile("mst        m12, (t0) , t3 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.m     m0            "   );
  asm volatile("mzero.m     m4            "   );
  asm volatile("mzero.m     m8            "   );
  asm volatile("mzero.m     m12           "   );
}

void __attribute__ ((noinline)) test_load_store_1x1 (float* addrA, float* addrC, int K)
{
  asm volatile("addi	sp, sp, -0x30    "   );  // 
  asm volatile("sw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("sw	s1 , 0x28(sp)      "   );  // 
  asm volatile("sw	s2 , 0x24(sp)      "   );  // 
  asm volatile("sw	s3 , 0x20(sp)      "   );  // 
  asm volatile("sw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("sw	s5 , 0x18(sp)      "   );  // 
  asm volatile("sw	s6 , 0x14(sp)      "   );  // 
  asm volatile("sw	s7 , 0x10(sp)      "   );  // 
  asm volatile("sw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("sw	s9 , 0x08(sp)      "   );  // 
  asm volatile("sw	s10, 0x04(sp)      "   );  // 
  asm volatile("sw	s11, 0x00(sp)      "   );  // 
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli    t3,  %0  ,  2   " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi    t1,  %0  ,  512 " :: "r" (addrA));
  asm volatile("addi    s4,  %0  ,  512 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t1  ,  0 "   );
  asm volatile("addi    s0,  s4  ,  0 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t0) , t3 "   );
  asm volatile("mst       m0, (s0) , t3 "   );
  asm volatile("mld.lhs   m1, (t2) , t3 "   );
  asm volatile("mst       m1, (s2) , t3 "   );
  asm volatile("mld.lhs   m2, (t5) , t3 "   );
  asm volatile("mst       m2, (s5) , t3 "   );
  asm volatile("mld.lhs   m3, (t6) , t3 "   );
  asm volatile("mst       m3, (s6) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m4, (t0) , t3 "   );
  asm volatile("mld.lhs   m5, (t2) , t3 "   );
  asm volatile("mld.lhs   m6, (t5) , t3 "   );
  asm volatile("mld.lhs   m7, (t6) , t3 "   );
  asm volatile("mst   m4, (s0) , t3 "   );
  asm volatile("mst   m5, (s2) , t3 "   );
  asm volatile("mst   m6, (s5) , t3 "   );
  asm volatile("mst   m7, (s6) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m8, (t0) , t3 "   );
  asm volatile("mld.lhs   m9, (t2) , t3 "   );
  asm volatile("mst   m8, (s0) , t3 "   );
  asm volatile("mst   m9, (s2) , t3 "   );
  asm volatile("mld.lhs  m10, (t5) , t3 "   );
  asm volatile("mld.lhs  m11, (t6) , t3 "   );
  asm volatile("mst  m10, (s5) , t3 "   );
  asm volatile("mst  m11, (s6) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs  m12, (t0) , t3 "   );
  asm volatile("mst  m12, (s0) , t3 "   );
  asm volatile("mld.lhs  m13, (t2) , t3 "   );
  asm volatile("mld.lhs  m14, (t5) , t3 "   );
  asm volatile("mst  m13, (s2) , t3 "   );
  asm volatile("mld.lhs  m15, (t6) , t3 "   );
  asm volatile("mst  m14, (s5) , t3 "   );
  asm volatile("mst  m15, (s6) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m8            "   );
  asm volatile("mzero.m  m12           "   );
  // --------------------------------------------------------------------------
  asm volatile("lw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("lw	s1 , 0x28(sp)      "   );  // 
  asm volatile("lw	s2 , 0x24(sp)      "   );  // 
  asm volatile("lw	s3 , 0x20(sp)      "   );  // 
  asm volatile("lw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("lw	s5 , 0x18(sp)      "   );  // 
  asm volatile("lw	s6 , 0x14(sp)      "   );  // 
  asm volatile("lw	s7 , 0x10(sp)      "   );  // 
  asm volatile("lw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("lw	s9 , 0x08(sp)      "   );  // 
  asm volatile("lw	s10, 0x04(sp)      "   );  // 
  asm volatile("lw	s11, 0x00(sp)      "   );  // 
  asm volatile("addi	sp, sp, 0x30     "   );  //
}
void __attribute__ ((noinline)) test_load_store_1x2 (float* addrA, float* addrC, int K)
{
  asm volatile("addi	sp, sp, -0x30    "   );  // 
  asm volatile("sw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("sw	s1 , 0x28(sp)      "   );  // 
  asm volatile("sw	s2 , 0x24(sp)      "   );  // 
  asm volatile("sw	s3 , 0x20(sp)      "   );  // 
  asm volatile("sw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("sw	s5 , 0x18(sp)      "   );  // 
  asm volatile("sw	s6 , 0x14(sp)      "   );  // 
  asm volatile("sw	s7 , 0x10(sp)      "   );  // 
  asm volatile("sw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("sw	s9 , 0x08(sp)      "   );  // 
  asm volatile("sw	s10, 0x04(sp)      "   );  // 
  asm volatile("sw	s11, 0x00(sp)      "   );  // 
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli    t3,  %0  ,  2   " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi    t1,  %0  ,  512 " :: "r" (addrA));
  asm volatile("addi    s4,  %0  ,  512 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t1  ,  0 "   );
  asm volatile("addi    s0,  s4  ,  0 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.rhs   m0, (t0) , t3 "   );
  asm volatile("mst       m0, (s0) , t3 "   );
  asm volatile("mld.rhs   m2, (t5) , t3 "   );
  asm volatile("mst       m2, (s5) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.rhs   m4, (t0) , t3 "   );
  asm volatile("mld.rhs   m6, (t5) , t3 "   );
  asm volatile("mst   m4, (s0) , t3 "   );
  asm volatile("mst   m6, (s5) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.rhs   m8, (t0) , t3 "   );
  asm volatile("mst   m8, (s0) , t3 "   );
  asm volatile("mld.rhs  m10, (t5) , t3 "   );
  asm volatile("mst  m10, (s5) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.rhs  m12, (t0) , t3 "   );
  asm volatile("mld.rhs  m14, (t5) , t3 "   );
  asm volatile("mst  m12, (s0) , t3 "   );
  asm volatile("mst  m14, (s5) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m8            "   );
  asm volatile("mzero.m  m12           "   );
  // --------------------------------------------------------------------------
  asm volatile("lw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("lw	s1 , 0x28(sp)      "   );  // 
  asm volatile("lw	s2 , 0x24(sp)      "   );  // 
  asm volatile("lw	s3 , 0x20(sp)      "   );  // 
  asm volatile("lw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("lw	s5 , 0x18(sp)      "   );  // 
  asm volatile("lw	s6 , 0x14(sp)      "   );  // 
  asm volatile("lw	s7 , 0x10(sp)      "   );  // 
  asm volatile("lw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("lw	s9 , 0x08(sp)      "   );  // 
  asm volatile("lw	s10, 0x04(sp)      "   );  // 
  asm volatile("lw	s11, 0x00(sp)      "   );  // 
  asm volatile("addi	sp, sp, 0x30     "   );  //
}
void __attribute__ ((noinline)) test_load_store_1x4 (float* addrA, float* addrC, int K)
{
  asm volatile("addi	sp, sp, -0x30    "   );  // 
  asm volatile("sw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("sw	s1 , 0x28(sp)      "   );  // 
  asm volatile("sw	s2 , 0x24(sp)      "   );  // 
  asm volatile("sw	s3 , 0x20(sp)      "   );  // 
  asm volatile("sw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("sw	s5 , 0x18(sp)      "   );  // 
  asm volatile("sw	s6 , 0x14(sp)      "   );  // 
  asm volatile("sw	s7 , 0x10(sp)      "   );  // 
  asm volatile("sw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("sw	s9 , 0x08(sp)      "   );  // 
  asm volatile("sw	s10, 0x04(sp)      "   );  // 
  asm volatile("sw	s11, 0x00(sp)      "   );  // 
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli    t3,  %0  ,  2   " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi    t1,  %0  ,  512 " :: "r" (addrA));
  asm volatile("addi    s4,  %0  ,  512 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t1  ,  0 "   );
  asm volatile("addi    s0,  s4  ,  0 "   );
  asm volatile("addi    t2,  t0  , 64 "   );
  asm volatile("addi    s2,  s0  , 64 "   );
  asm volatile("addi    t5,  t2  , 64 "   );
  asm volatile("addi    s5,  s2  , 64 "   );
  asm volatile("addi    t6,  t5  , 64 "   );
  asm volatile("addi    s6,  s5  , 64 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.rhs   m0, (t0) , t3 "   );
  asm volatile("mld.rhs   m4, (t2) , t3 "   );
  asm volatile("mst       m0, (s0) , t3 "   );
  asm volatile("mld.rhs   m8, (t5) , t3 "   );
  asm volatile("mst       m4, (s2) , t3 "   );
  asm volatile("mst       m8, (s5) , t3 "   );
  asm volatile("mld.rhs  m12, (t6) , t3 "   );
  asm volatile("mst      m12, (s6) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m8            "   );
  asm volatile("mzero.m  m12           "   );
  // --------------------------------------------------------------------------
  asm volatile("lw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("lw	s1 , 0x28(sp)      "   );  // 
  asm volatile("lw	s2 , 0x24(sp)      "   );  // 
  asm volatile("lw	s3 , 0x20(sp)      "   );  // 
  asm volatile("lw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("lw	s5 , 0x18(sp)      "   );  // 
  asm volatile("lw	s6 , 0x14(sp)      "   );  // 
  asm volatile("lw	s7 , 0x10(sp)      "   );  // 
  asm volatile("lw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("lw	s9 , 0x08(sp)      "   );  // 
  asm volatile("lw	s10, 0x04(sp)      "   );  // 
  asm volatile("lw	s11, 0x00(sp)      "   );  // 
  asm volatile("addi	sp, sp, 0x30     "   );  //
}
void __attribute__ ((noinline)) test_load_store_2x1 (float* addrA, float* addrC, int K)
{
  asm volatile("addi	sp, sp, -0x30    "   );  // 
  asm volatile("sw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("sw	s1 , 0x28(sp)      "   );  // 
  asm volatile("sw	s2 , 0x24(sp)      "   );  // 
  asm volatile("sw	s3 , 0x20(sp)      "   );  // 
  asm volatile("sw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("sw	s5 , 0x18(sp)      "   );  // 
  asm volatile("sw	s6 , 0x14(sp)      "   );  // 
  asm volatile("sw	s7 , 0x10(sp)      "   );  // 
  asm volatile("sw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("sw	s9 , 0x08(sp)      "   );  // 
  asm volatile("sw	s10, 0x04(sp)      "   );  // 
  asm volatile("sw	s11, 0x00(sp)      "   );  // 
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli    t3,  %0  ,  2   " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi    t1,  %0  ,  512 " :: "r" (addrA));
  asm volatile("addi    s4,  %0  ,  512 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t1  ,  0 "   );
  asm volatile("addi    s0,  s4  ,  0 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t0) , t3 "   );
  asm volatile("mst       m0, (s0) , t3 "   );
  asm volatile("mld.lhs   m2, (t5) , t3 "   );
  asm volatile("mst       m2, (s5) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m4, (t0) , t3 "   );
  asm volatile("mld.lhs   m6, (t5) , t3 "   );
  asm volatile("mst   m4, (s0) , t3 "   );
  asm volatile("mst   m6, (s5) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m8, (t0) , t3 "   );
  asm volatile("mst   m8, (s0) , t3 "   );
  asm volatile("mld.lhs  m10, (t5) , t3 "   );
  asm volatile("mst  m10, (s5) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t6  , 16 "   );
  asm volatile("addi    s0,  s6  , 16 "   );
  asm volatile("addi    t2,  t0  , 16 "   );
  asm volatile("addi    s2,  s0  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs  m12, (t0) , t3 "   );
  asm volatile("mld.lhs  m14, (t5) , t3 "   );
  asm volatile("mst  m12, (s0) , t3 "   );
  asm volatile("mst  m14, (s5) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m8            "   );
  asm volatile("mzero.m  m12           "   );
  // --------------------------------------------------------------------------
  asm volatile("lw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("lw	s1 , 0x28(sp)      "   );  // 
  asm volatile("lw	s2 , 0x24(sp)      "   );  // 
  asm volatile("lw	s3 , 0x20(sp)      "   );  // 
  asm volatile("lw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("lw	s5 , 0x18(sp)      "   );  // 
  asm volatile("lw	s6 , 0x14(sp)      "   );  // 
  asm volatile("lw	s7 , 0x10(sp)      "   );  // 
  asm volatile("lw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("lw	s9 , 0x08(sp)      "   );  // 
  asm volatile("lw	s10, 0x04(sp)      "   );  // 
  asm volatile("lw	s11, 0x00(sp)      "   );  // 
  asm volatile("addi	sp, sp, 0x30     "   );  //
}
void __attribute__ ((noinline)) test_load_store_4x1 (float* addrA, float* addrC, int K)
{
  asm volatile("addi	sp, sp, -0x30    "   );  // 
  asm volatile("sw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("sw	s1 , 0x28(sp)      "   );  // 
  asm volatile("sw	s2 , 0x24(sp)      "   );  // 
  asm volatile("sw	s3 , 0x20(sp)      "   );  // 
  asm volatile("sw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("sw	s5 , 0x18(sp)      "   );  // 
  asm volatile("sw	s6 , 0x14(sp)      "   );  // 
  asm volatile("sw	s7 , 0x10(sp)      "   );  // 
  asm volatile("sw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("sw	s9 , 0x08(sp)      "   );  // 
  asm volatile("sw	s10, 0x04(sp)      "   );  // 
  asm volatile("sw	s11, 0x00(sp)      "   );  // 
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli    t3,  %0  ,  2   " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi    t1,  %0  ,  512 " :: "r" (addrA));
  asm volatile("addi    s4,  %0  ,  512 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t1  ,  0 "   );
  asm volatile("addi    s0,  s4  ,  0 "   );
  asm volatile("addi    t2,  t0  , 64 "   );
  asm volatile("addi    s2,  s0  , 64 "   );
  asm volatile("addi    t5,  t2  , 64 "   );
  asm volatile("addi    s5,  s2  , 64 "   );
  asm volatile("addi    t6,  t5  , 64 "   );
  asm volatile("addi    s6,  s5  , 64 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t0) , t3 "   );
  asm volatile("mld.lhs   m4, (t2) , t3 "   );
  asm volatile("mst       m0, (s0) , t3 "   );
  asm volatile("mld.lhs   m8, (t5) , t3 "   );
  asm volatile("mst       m4, (s2) , t3 "   );
  asm volatile("mst       m8, (s5) , t3 "   );
  asm volatile("mld.lhs  m12, (t6) , t3 "   );
  asm volatile("mst      m12, (s6) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m8            "   );
  asm volatile("mzero.m  m12           "   );
  // --------------------------------------------------------------------------
  asm volatile("lw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("lw	s1 , 0x28(sp)      "   );  // 
  asm volatile("lw	s2 , 0x24(sp)      "   );  // 
  asm volatile("lw	s3 , 0x20(sp)      "   );  // 
  asm volatile("lw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("lw	s5 , 0x18(sp)      "   );  // 
  asm volatile("lw	s6 , 0x14(sp)      "   );  // 
  asm volatile("lw	s7 , 0x10(sp)      "   );  // 
  asm volatile("lw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("lw	s9 , 0x08(sp)      "   );  // 
  asm volatile("lw	s10, 0x04(sp)      "   );  // 
  asm volatile("lw	s11, 0x00(sp)      "   );  // 
  asm volatile("addi	sp, sp, 0x30     "   );  //
}
void __attribute__ ((noinline)) test_load_store_2x2 (float* addrA, float* addrC, int K)
{
  asm volatile("addi	sp, sp, -0x30    "   );  // 
  asm volatile("sw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("sw	s1 , 0x28(sp)      "   );  // 
  asm volatile("sw	s2 , 0x24(sp)      "   );  // 
  asm volatile("sw	s3 , 0x20(sp)      "   );  // 
  asm volatile("sw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("sw	s5 , 0x18(sp)      "   );  // 
  asm volatile("sw	s6 , 0x14(sp)      "   );  // 
  asm volatile("sw	s7 , 0x10(sp)      "   );  // 
  asm volatile("sw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("sw	s9 , 0x08(sp)      "   );  // 
  asm volatile("sw	s10, 0x04(sp)      "   );  // 
  asm volatile("sw	s11, 0x00(sp)      "   );  // 
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_2));
  asm volatile("mcfgk t0, t1"   );
  // --------------------------------------------------------------------------
  asm volatile("slli    t3,  %0  ,  2   " :: "r" (K)    );  // t3  = K*4;
  asm volatile("addi    t1,  %0  ,  512 " :: "r" (addrA));
  asm volatile("addi    s4,  %0  ,  512 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("addi    t0,  t1  ,  0 "   );
  asm volatile("addi    s0,  s4  ,  0 "   );
  asm volatile("addi    t2,  t0  , 64 "   );
  asm volatile("addi    s2,  s0  , 64 "   );
  asm volatile("addi    t5,  t2  , 64 "   );
  asm volatile("addi    s5,  s2  , 64 "   );
  asm volatile("addi    t6,  t5  , 64 "   );
  asm volatile("addi    s6,  s5  , 64 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.rhs   m6, (t2) , t3 "   );
  asm volatile("mld.lhs   m0, (t0) , t3 "   );
  asm volatile("mld.lhs   m4, (t2) , t3 "   );
  asm volatile("mld.rhs   m2, (t0) , t3 "   );
  asm volatile("mst       m0, (s0) , t3 "   );
  asm volatile("mld.lhs   m8, (t5) , t3 "   );
  asm volatile("mld.rhs  m10, (t5) , t3 "   );
  asm volatile("mld.lhs  m12, (t6) , t3 "   );
  asm volatile("mst       m4, (s2) , t3 "   );
  asm volatile("mst       m8, (s5) , t3 "   );
  asm volatile("mld.rhs  m14, (t6) , t3 "   );
  asm volatile("mst      m12, (s6) , t3 "   );
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_4));
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m8            "   );
  asm volatile("mzero.m  m12           "   );
  // --------------------------------------------------------------------------
  asm volatile("lw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("lw	s1 , 0x28(sp)      "   );  // 
  asm volatile("lw	s2 , 0x24(sp)      "   );  // 
  asm volatile("lw	s3 , 0x20(sp)      "   );  // 
  asm volatile("lw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("lw	s5 , 0x18(sp)      "   );  // 
  asm volatile("lw	s6 , 0x14(sp)      "   );  // 
  asm volatile("lw	s7 , 0x10(sp)      "   );  // 
  asm volatile("lw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("lw	s9 , 0x08(sp)      "   );  // 
  asm volatile("lw	s10, 0x04(sp)      "   );  // 
  asm volatile("lw	s11, 0x00(sp)      "   );  // 
  asm volatile("addi	sp, sp, 0x30     "   );  //
}

void __attribute__ ((noinline)) test_macc(float* addrA,float* addrB,float* addrC, int K)
{   
  // --------------------------------------------------------------------------
  asm volatile("addi	sp, sp, -0x30    "   );  // 
  asm volatile("sw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("sw	s1 , 0x28(sp)      "   );  // 
  asm volatile("sw	s2 , 0x24(sp)      "   );  // 
  asm volatile("sw	s3 , 0x20(sp)      "   );  // 
  asm volatile("sw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("sw	s5 , 0x18(sp)      "   );  // 
  asm volatile("sw	s6 , 0x14(sp)      "   );  // 
  asm volatile("sw	s7 , 0x10(sp)      "   );  // 
  asm volatile("sw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("sw	s9 , 0x08(sp)      "   );  // 
  asm volatile("sw	s10, 0x04(sp)      "   );  // 
  asm volatile("sw	s11, 0x00(sp)      "   );  // 
  // --------------------------------------------------------------------------
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));  // t1  = startAddrA0  = addrA 
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));  // t6  = startAddrB0  = addrB 
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));  // t6  = startAddrB0  = addrB 
  asm volatile("slli    t2,  %0  ,  4 " :: "r" (K)    );
  asm volatile("add     s2,  s1  , t2 "   );
  asm volatile("add     s9,  s8  , t2 "   );
  asm volatile("add     t2,  t1  , t2 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi    t3,  t1  ,  0 "   );
  asm volatile("addi    s3,  s1  ,  0 "   );
  asm volatile("addi    t4,  t3  , 16 "   );
  asm volatile("addi    s4,  s3  , 16 "   );
  asm volatile("addi    t5,  t2  ,  0 "   );
  asm volatile("addi    s5,  s2  ,  0 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("li t1, 16");
  asm volatile("mcfgm t0, t1, %0" :: "i" (MUL_1));
  asm volatile("mcfgn t0, t1, %0" :: "i" (MUL_1));
  // --------------------------------------------------------------------------
  asm volatile("mzero.a acc0          "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t3) , s7 "   );
  asm volatile("mld.lhs   m1, (t4) , s7 "   );
  asm volatile("mld.lhs   m2, (t5) , s7 "   );
  asm volatile("mld.lhs   m3, (t6) , s7 "   );
  asm volatile("mld.rhs   m4, (s3) , s7 "   );
  asm volatile("mld.rhs   m5, (s4) , s7 "   );
  asm volatile("mld.rhs   m6, (s5) , s7 "   );
  asm volatile("mld.rhs   m7, (s6) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi    t3,  t1  , 16 "   );
  asm volatile("addi    s3,  s1  , 16 "   );
  asm volatile("addi    t4,  t3  , 16 "   );
  asm volatile("addi    s4,  s3  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t3) , s7 "   );
  asm volatile("mld.lhs   m1, (t4) , s7 "   );
  asm volatile("mld.lhs   m2, (t5) , s7 "   );
  asm volatile("mld.lhs   m3, (t6) , s7 "   );
  asm volatile("mld.rhs   m4, (s3) , s7 "   );
  asm volatile("mld.rhs   m5, (s4) , s7 "   );
  asm volatile("mld.rhs   m6, (s5) , s7 "   );
  asm volatile("mld.rhs   m7, (s6) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mmov.am   m4 ,  acc0     " );
  asm volatile("mst     m4 , (s8) , s7 " :: "r" (addrC));
  asm volatile("addi      s8 ,  s8  , 16 " );
  asm volatile("mst     m6 , (s9) , s7 " :: "r" (addrC));
  asm volatile("addi      s9 ,  s9  , 16 " );
  asm volatile("mst     m5 , (s8) , s7 " :: "r" (addrC));
  asm volatile("mst     m7 , (s9) , s7 " :: "r" (addrC));
  // --------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m1            "   );
  asm volatile("mzero.m  m2            "   );
  asm volatile("mzero.m  m3            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m5            "   );
  asm volatile("mzero.m  m6            "   );
  asm volatile("mzero.m  m7            "   );
  asm volatile("mzero.a  acc0          "   );
  // --------------------------------------------------------------------------
  asm volatile("lw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("lw	s1 , 0x28(sp)      "   );  // 
  asm volatile("lw	s2 , 0x24(sp)      "   );  // 
  asm volatile("lw	s3 , 0x20(sp)      "   );  // 
  asm volatile("lw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("lw	s5 , 0x18(sp)      "   );  // 
  asm volatile("lw	s6 , 0x14(sp)      "   );  // 
  asm volatile("lw	s7 , 0x10(sp)      "   );  // 
  asm volatile("lw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("lw	s9 , 0x08(sp)      "   );  // 
  asm volatile("lw	s10, 0x04(sp)      "   );  // 
  asm volatile("lw	s11, 0x00(sp)      "   );  // 
  asm volatile("addi	sp, sp, 0x30     "   );  //
  // --------------------------------------------------------------------------
  // asm volatile("mcfgk t2, t1"   );
  // asm volatile("mcfgm t2, t1, 3");
  // asm volatile("mcfgn t2, t1, 3");
  // asm volatile("fmmacc.b acc0, m2, m0");
  // asm volatile("fmmacc.h acc0, m2, m0");
  // asm volatile("mmacc acc0, m2, m0");
  // asm volatile("mmaqa.b acc0, m2, m0");
  // asm volatile("mmada.h acc0, m2, m0");
  // asm volatile("mmasa.w acc0, m2, m0");
  // asm volatile("mmov.mm m1, m0");
  // asm volatile("mmov.ma acc0, m0");
  // asm volatile("mmov.am m1, acc0");
  // asm volatile("mmov.aa acc0, acc0");
  // asm volatile("mzero.m m1");
  // asm volatile("mzero.a acc0");
  // asm volatile("mld.b m1, (t0), t4");
  // asm volatile("mld.h m1, (t0), t4");
  // asm volatile("mld.lhs m1, (t0), t4");
  // asm volatile("mst.b m1, (t0), t4");
  // asm volatile("mst.h m1, (t0), t4");
  // asm volatile("mst m1, (t0), t4");
}
void __attribute__ ((noinline)) test_macc_adv(float* addrA,float* addrB,float* addrC, int K)
{   
  // --------------------------------------------------------------------------
  asm volatile("addi	sp, sp, -0x30    "   );  // 
  asm volatile("sw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("sw	s1 , 0x28(sp)      "   );  // 
  asm volatile("sw	s2 , 0x24(sp)      "   );  // 
  asm volatile("sw	s3 , 0x20(sp)      "   );  // 
  asm volatile("sw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("sw	s5 , 0x18(sp)      "   );  // 
  asm volatile("sw	s6 , 0x14(sp)      "   );  // 
  asm volatile("sw	s7 , 0x10(sp)      "   );  // 
  asm volatile("sw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("sw	s9 , 0x08(sp)      "   );  // 
  asm volatile("sw	s10, 0x04(sp)      "   );  // 
  asm volatile("sw	s11, 0x00(sp)      "   );  // 
  // --------------------------------------------------------------------------
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));  // t1  = startAddrA0  = addrA 
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));  // t6  = startAddrB0  = addrB 
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));  // t6  = startAddrB0  = addrB 
  asm volatile("slli    t2,  %0  ,  4 " :: "r" (K)    );
  asm volatile("add     s2,  s1  , t2 "   );
  asm volatile("add     s9,  s8  , t2 "   );
  asm volatile("add     t2,  t1  , t2 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi    t3,  t1  ,  0 "   );
  asm volatile("addi    s3,  s1  ,  0 "   );
  asm volatile("addi    t4,  t3  , 16 "   );
  asm volatile("addi    s4,  s3  , 16 "   );
  asm volatile("addi    t5,  t2  ,  0 "   );
  asm volatile("addi    s5,  s2  ,  0 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t3) , s7 "   );
  asm volatile("mld.lhs   m1, (t4) , s7 "   );
  asm volatile("mld.lhs   m2, (t5) , s7 "   );
  asm volatile("mld.lhs   m3, (t6) , s7 "   );
  asm volatile("mld.rhs   m4, (s3) , s7 "   );
  asm volatile("mld.rhs   m5, (s4) , s7 "   );
  asm volatile("mld.rhs   m6, (s5) , s7 "   );
  asm volatile("mld.rhs   m7, (s6) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("addi    t3,  t1  , 16 "   );
  asm volatile("addi    s3,  s1  , 16 "   );
  asm volatile("addi    t4,  t3  , 16 "   );
  asm volatile("addi    s4,  s3  , 16 "   );
  asm volatile("addi    t5,  t2  , 16 "   );
  asm volatile("addi    s5,  s2  , 16 "   );
  asm volatile("addi    t6,  t5  , 16 "   );
  asm volatile("addi    s6,  s5  , 16 "   );
  // --------------------------------------------------------------------------
  asm volatile("mzero.a acc0          "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t3) , s7 "   );
  asm volatile("mld.lhs   m1, (t4) , s7 "   );
  asm volatile("mld.rhs   m4, (s3) , s7 "   );
  asm volatile("mld.rhs   m5, (s4) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m2, (t5) , s7 "   );
  asm volatile("mld.lhs   m3, (t6) , s7 "   );
  asm volatile("mld.rhs   m6, (s5) , s7 "   );
  asm volatile("mld.rhs   m7, (s6) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t3) , s7 "   );
  asm volatile("mld.lhs   m1, (t4) , s7 "   );
  asm volatile("mld.rhs   m4, (s3) , s7 "   );
  asm volatile("mld.rhs   m5, (s4) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m2, (t5) , s7 "   );
  asm volatile("mld.lhs   m3, (t6) , s7 "   );
  asm volatile("mld.rhs   m6, (s5) , s7 "   );
  asm volatile("mld.rhs   m7, (s6) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m0, (t3) , s7 "   );
  asm volatile("mld.lhs   m1, (t4) , s7 "   );
  asm volatile("mld.rhs   m4, (s3) , s7 "   );
  asm volatile("mld.rhs   m5, (s4) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mld.lhs   m2, (t5) , s7 "   );
  asm volatile("mld.lhs   m3, (t6) , s7 "   );
  asm volatile("mld.rhs   m6, (s5) , s7 "   );
  asm volatile("mld.rhs   m7, (s6) , s7 "   );
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  asm volatile("mmov.am   m4 ,  acc0     " );
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mmov.am   m4 ,  acc0     " );
  asm volatile("mzero.a  acc0          "   );
  asm volatile("mst     m4 , (s8) , s7 " :: "r" (addrC));
  asm volatile("addi      s8 ,  s8  , 16 " );
  asm volatile("mst     m6 , (s9) , s7 " :: "r" (addrC));
  asm volatile("addi      s9 ,  s9  , 16 " );
  asm volatile("mst     m5 , (s8) , s7 " :: "r" (addrC));
  asm volatile("mst     m7 , (s9) , s7 " :: "r" (addrC));
  asm volatile("mmacc acc0,  m4  , m0 " ); // CMUL=2, RMUL=2
  asm volatile("mmacc acc0,  m6  , m2 " ); // CMUL=2, RMUL=2
  // --------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m1            "   );
  asm volatile("mzero.m  m2            "   );
  asm volatile("mzero.m  m3            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m5            "   );
  asm volatile("mzero.m  m6            "   );
  asm volatile("mzero.m  m7            "   );
  asm volatile("mzero.a  acc0          "   );
  // --------------------------------------------------------------------------
  asm volatile("lw	s0 , 0x2c(sp)      "   );  // 
  asm volatile("lw	s1 , 0x28(sp)      "   );  // 
  asm volatile("lw	s2 , 0x24(sp)      "   );  // 
  asm volatile("lw	s3 , 0x20(sp)      "   );  // 
  asm volatile("lw	s4 , 0x1c(sp)      "   );  // 
  asm volatile("lw	s5 , 0x18(sp)      "   );  // 
  asm volatile("lw	s6 , 0x14(sp)      "   );  // 
  asm volatile("lw	s7 , 0x10(sp)      "   );  // 
  asm volatile("lw	s8 , 0x0c(sp)      "   );  // 
  asm volatile("lw	s9 , 0x08(sp)      "   );  // 
  asm volatile("lw	s10, 0x04(sp)      "   );  // 
  asm volatile("lw	s11, 0x00(sp)      "   );  // 
  asm volatile("addi	sp, sp, 0x30     "   );  //
  // --------------------------------------------------------------------------
}

void verify_quadrilatero (float* addrA,float* addrB,float* addrC, int K)
{
  printf("Start!\n");
  
  printf("Matrix Cfg   Test   started: ");
  test_cfg();
  printf("[SUCCESS]\n");

  printf("Matrix Load  Test 1 started: ");
  test_load_1x1 (addrA, K);
  printf("[SUCCESS]\n");
  printf("Matrix Load  Test 2 started: ");
  test_load_1x2 (addrA, K);
  printf("[SUCCESS]\n");
  printf("Matrix Load  Test 3 started: ");
  test_load_1x4 (addrA, K);
  printf("[SUCCESS]\n");
  printf("Matrix Load  Test 4 started: ");
  test_load_2x1 (addrA, K);
  printf("[SUCCESS]\n");
  printf("Matrix Load  Test 5 started: ");
  test_load_4x1 (addrA, K);
  printf("[SUCCESS]\n");
  printf("Matrix Load  Test 6 started: ");
  test_load_2x2 (addrA, K);
  printf("[SUCCESS]\n");

  printf("Matrix Store Test 1 started: ");
  test_store_1x1 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Store Test 2 started: ");
  test_store_1x2 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Store Test 3 started: ");
  test_store_1x4 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Store Test 4 started: ");
  test_store_2x1 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Store Test 5 started: ");
  test_store_4x1 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Store Test 6 started: ");
  test_store_2x2 (addrA, addrC, K);
  printf("[SUCCESS]\n");

  printf("Matrix Mem   Test 1 started: ");
  test_load_store_1x1 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Mem   Test 2 started: ");
  test_load_store_1x2 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Mem   Test 3 started: ");
  test_load_store_1x4 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Mem   Test 4 started: ");
  test_load_store_2x1 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Mem   Test 5 started: ");
  test_load_store_4x1 (addrA, addrC, K);
  printf("[SUCCESS]\n");
  printf("Matrix Mem   Test 6 started: ");
  test_load_store_2x2 (addrA, addrC, K);
  printf("[SUCCESS]\n");

  printf("Matrix MACC  Test 1 started: ");
  test_macc (addrA,addrB,addrC, K);
  printf("[SUCCESS]\n");

  printf("Matrix MACC  Test 2 started: ");
  test_macc (addrA,addrB,addrC, K);
  printf("[SUCCESS]\n");

  // printf("Verify Dependencies Test started\n");
  // verify_dependencies (addrA,addrB,addrC, K);
  // printf("MVerify Dependencies Test completed successfully\n");
}

// void __attribute__ ((noinline)) test_cfg (){
//   // --- Original tests (commented out) ---
//   asm volatile("mcfgk t2, t1"   );
//   asm volatile("mcfgm t2, t1, 3");
//   asm volatile("mcfgn t2, t1, 3");
//   asm volatile("fmmacc.b acc0, m2, m0");
//   asm volatile("fmmacc.h acc0, m2, m0");
//   asm volatile("mmacc acc0, m2, m0");
//   asm volatile("mmaqa.b acc0, m2, m0");
//   asm volatile("mmada.h acc0, m2, m0");
//   asm volatile("mmasa.w acc0, m2, m0");
//   asm volatile("mmov.mm m1, m0");
//   asm volatile("mmov.ma acc0, m0");
//   asm volatile("mmov.am m1, acc0");
//   asm volatile("mmov.aa acc0, acc0");
//   asm volatile("mzero.m m1");
//   asm volatile("mzero.a acc0");
//   asm volatile("mld.b m1, (t0), t4");
//   asm volatile("mld.h m1, (t0), t4");
//   asm volatile("mld.lhs m1, (t0), t4");
//   asm volatile("mst.b m1, (t0), t4");
//   asm volatile("mst.h m1, (t0), t4");
//   asm volatile("mst m1, (t0), t4");

//   // ==========================================
//   // Zvame: Attached Matrix Extension
//   // ==========================================

//   // --- Datatype Management ---
//   asm volatile("agettyp t2, acc0");
//   asm volatile("mgettyp t2, m1");
//   asm volatile("asettyp acc0, t1");
//   asm volatile("msettyp m1, t1");

//   // --- Unary Elementwise ---
//   asm volatile("mabs.ew m1, m2");
//   asm volatile("mexp2.ew m1, m2");
//   asm volatile("mlog2.ew m1, m2");
//   asm volatile("mbcast.x m1, t1");
//   asm volatile("mconv.ew m1, m2");
//   asm volatile("mcolunzip.ew m1, m2");
//   asm volatile("mcolzip.ew m1, m2");
//   asm volatile("mrowunzip.ew m1, m2");

//   // --- Binary Elementwise ---
//   asm volatile("madd.ew m1, m2, m3");
//   asm volatile("msub.ew m1, m2, m3");
//   asm volatile("mmul.ew m1, m2, m3");
//   asm volatile("mabsdiff.ew m1, m2, m3");
//   asm volatile("mand.ew m1, m2, m3");
//   asm volatile("mor.ew m1, m2, m3");
//   asm volatile("mxor.ew m1, m2, m3");
//   asm volatile("mandnot.ew m1, m2, m3");
//   asm volatile("mornot.ew m1, m2, m3");
//   asm volatile("mmax.ew m1, m2, m3");
//   asm volatile("mmin.ew m1, m2, m3");
//   asm volatile("mmean.ew m1, m2, m3");
//   asm volatile("mhdiff.ew m1, m2, m3");
//   asm volatile("mgather.ew m1, m2, m3");
//   asm volatile("mrowzip.ew m1, m2, m3");

//   // --- Scalar Binary (.x) ---
//   asm volatile("madd.ew.x m1, t1, m2");
//   asm volatile("msub.ew.x m1, t1, m2");
//   asm volatile("mmul.ew.x m1, t1, m2");
//   asm volatile("mabsdiff.ew.x m1, t1, m2");
//   asm volatile("mand.ew.x m1, t1, m2");
//   asm volatile("mor.ew.x m1, t1, m2");
//   asm volatile("mxor.ew.x m1, t1, m2");
//   asm volatile("mandnot.ew.x m1, t1, m2");
//   asm volatile("mornot.ew.x m1, t1, m2");
//   asm volatile("mmax.ew.x m1, t1, m2");
//   asm volatile("mmin.ew.x m1, t1, m2");
//   asm volatile("mmean.ew.x m1, t1, m2");
//   asm volatile("mhdiff.ew.x m1, t1, m2");

//   // --- Compare & Select ---
//   asm volatile("mcmpge.ew m1, m2, m3");
//   asm volatile("mcmplt.ew m1, m2, m3");
//   asm volatile("mcmpge.ew.x m1, t1, m2");
//   asm volatile("mcmplt.ew.x m1, t1, m2");
//   asm volatile("mcmovge.ew m1, m2, m3");
//   asm volatile("mcmovlt.ew m1, m2, m3");
//   asm volatile("mselge.ew m1, m2, m3");
//   asm volatile("msellt.ew m1, m2, m3");

//   // --- Fused Multiply-Add Variants ---
//   asm volatile("mmulacc.ew m1, m2, m3");
//   asm volatile("mmulaccneg.ew m1, m2, m3");
//   asm volatile("mmuladd.ew m1, m2, m3");
//   asm volatile("mmulsub.ew m1, m2, m3");
//   asm volatile("mmulacc.ew.x m1, t1, m2");
//   asm volatile("mmulaccneg.ew.x m1, t1, m2");
//   asm volatile("mmuladd.ew.x m1, t1, m2");
//   asm volatile("mmulsub.ew.x m1, t1, m2");
//   asm volatile("mmulneg.ew m1, m2, m3");
//   asm volatile("mmulneg.ew.x m1, t1, m2");

//   // --- Log/Exp with Bias ---
//   asm volatile("mlog2sub.ew m1, m2, m3");
//   asm volatile("msublog2.ew m1, m2, m3");
//   asm volatile("mlog2sub.ew.x m1, t1, m2");
//   asm volatile("msublog2.ew.x m1, t1, m2");

//   // --- Shift/Scale ---
//   asm volatile("mldexp.ew m1, m2, m3");
//   asm volatile("mrdexp.ew m1, m2, m3");
//   asm volatile("mldexp.ew.x m1, t1, m2");
//   asm volatile("mldexpacc.ew m1, m2, m3");
//   asm volatile("mrdexpacc.ew m1, m2, m3");
//   asm volatile("mldexpacc.ew.x m1, t1, m2");
//   asm volatile("mshift.ew m1, m2, 5");

//   // --- Prefix & Reduce ---
//   asm volatile("mprefixadd.row m1, m2");
//   asm volatile("mprefixadd.col m1, m2");
//   asm volatile("mprefixmax.row m1, m2");
//   asm volatile("mprefixmax.col m1, m2");
//   asm volatile("mreduceadd.row m1, m2");
//   asm volatile("mreduceadd.col m1, m2");
//   asm volatile("mreducemax.row m1, m2");
//   asm volatile("mreducemax.col m1, m2");

//   // --- Scatter ---
//   asm volatile("mscatadd.col m1, m2, m3");
//   asm volatile("mscatadd.row m1, m2, m3");
//   asm volatile("mscatmax.col m1, m2, m3");
//   asm volatile("mscatmax.row m1, m2, m3");

//   // --- Matrix Multiply (2D) ---
//   asm volatile("mmul.2d acc0, m2, m3");
//   asm volatile("mmulacc.2d acc0, m2, m3");
//   asm volatile("mmulaccneg.2d acc0, m2, m3");
//   asm volatile("mmulneg.2d acc0, m2, m3");
//   asm volatile("mmulat.2d acc0, m2, m3");
//   asm volatile("mmulatacc.2d acc0, m2, m3");
//   asm volatile("mmulbt.2d acc0, m2, m3");
//   asm volatile("mmulbtacc.2d acc0, m2, m3");

//   // --- Accumulator Management ---
//   asm volatile("mzero.2d acc0");
//   asm volatile("mmov.a.m m1, acc0");
//   asm volatile("mmov.m.m m1, m2");

//   // --- Load/Store (fixed: removed parentheses) ---
//   asm volatile("mls m1, t0");
//   asm volatile("mls.rm m1, t0");
//   asm volatile("mls.cm m1, t0");
//   asm volatile("mss m1, t0");
//   asm volatile("mss.rm m1, t0");
//   asm volatile("mss.cm m1, t0");

//   // ==========================================
//   // Zvt: Vector Matrix Extensions
//   // ==========================================

//   // --- Matrix Configuration ---
//   asm volatile("msetmtype t1, t2");
//   asm volatile("msettn t2, t1");
//   asm volatile("msettm t2, t1");
//   asm volatile("msettk t2, t1");
//   asm volatile("msetmtypei 5, 3"); // uimm5, uimm3

//   // --- Matrix Arithmetic (fixed: m1 → mt1) ---
//   asm volatile("vtfmm.tvv mt1, v2, v1");
//   asm volatile("vtfmm.alt.tvv mt1, v2, v1");
//   asm volatile("vtmmu.tvv mt1, v2, v1");
//   asm volatile("vtmms.tvv mt1, v2, v1");

//   // --- Load/Store Tile Subset to Memory ---
//   asm volatile("vtle8 t4, (t0)");
//   asm volatile("vtle16 t4, (t0)");
//   asm volatile("vtle32 t4, (t0)");
//   asm volatile("vtle64 t4, (t0)");
//   asm volatile("vtse8 t4, (t0)");
//   asm volatile("vtse16 t4, (t0)");
//   asm volatile("vtse32 t4, (t0)");
//   asm volatile("vtse64 t4, (t0)");

//   // --- Move Tile Subset ---
//   asm volatile("vtmv.v.t v1, t0");
//   asm volatile("vtmv.t.v t0, v2");

//   // --- Matrix Tile-Zero (fixed: m1 → mt1) ---
//   asm volatile("vtzero mt1");

//   // --- Context Discard ---
//   asm volatile("vtdiscard");
// }
