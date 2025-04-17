// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata

#include "widening4x-bp-matmul.h"

// Output tile size: 8x8
void __attribute__ ((noinline)) matrixMul_8x8(int8_t* addrA,int8_t* addrB,int32_t* addrC, int K, int N, int M, int shift)
{

    asm volatile("addi	sp, sp, -0x30         "                            );   // 
    asm volatile("sw	s0 , 0x2c(sp)           "                            );   // 
    asm volatile("sw	s1 , 0x28(sp)           "                            );   // 
    asm volatile("sw	s2 , 0x24(sp)           "                            );   // 
    asm volatile("sw	s3 , 0x20(sp)           "                            );   // 
    asm volatile("sw	s4 , 0x1c(sp)           "                            );   // 
    asm volatile("sw	s5 , 0x18(sp)           "                            );   // 
    asm volatile("sw	s6 , 0x14(sp)           "                            );   // 
    asm volatile("sw	s7 , 0x10(sp)           "                            );   // 
    asm volatile("sw	s8 , 0x0c(sp)           "                            );   // 
    asm volatile("sw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("sw	s10, 0x04(sp)           "                            );   // 
    asm volatile("sw	s11, 0x00(sp)           "                            );   // 

    //--------------------------------------------------------------------------------
    asm volatile("sll     a6,%0,%1              " :: "r" (N),"r" (shift)     );   // a6  = N* 2**SIMD_SHIFT
    asm volatile("addi    t0,x0, 0              "                            );   // t0  = m0 =0;
    asm volatile("slli    s3,%0, 2              " :: "r" (K)                 );   // s3  = K*4;
    asm volatile("slli    s4,%0, 2              " :: "r" (N)                 );   // s4  = N*4;

    asm volatile("loopM_start8x8:               "                            );   // while(m0<M) {
    asm volatile("addi    t1,x0, 0              "                            );   // t1   = n0 =0
    asm volatile("addi    t3,t0,4               "                            );   // t3   = m0+WIDTH
    asm volatile("mul     s1,s3,t0              "                            );   // s1   = K*4*m0
    asm volatile("mul     s2,s3,t3              "                            );   // s2   = K*4*(m0+WIDTH)
    asm volatile("mul     s0,s4,t0              "                            );   // s0   = N*4*m0;
    asm volatile("mul     s10,s4,t3             "                            );   // s10  = N*4*(m0+WIDTH)
    asm volatile("add     s1,%0,s1              " :: "r" (addrA)             );   // s1   = startAddrA0  = addrA + K*4*m0 
    asm volatile("add     s2,%0,s2              " :: "r" (addrA)             );   // s2   = startAddrA1  = addrA + K*4*(m0+WIDTH)
    asm volatile("add     s0,%0,s0              " :: "r" (addrC)             );   // s0   = startAddrC0x = addrC + N*4*m0
    asm volatile("add     s10,%0,s10            " :: "r" (addrC)             );   // s10  = startAddrC1x = addrC + N*4*(m0+WIDTH)

    asm volatile("loopN_start8x8:               "                            );   // while(n0<N) {
    asm volatile("addi    t4,t1,4               "                            );   // t4  = n0+WIDTH;
    asm volatile("addi    t2,x0,16              "                            );   // t2  = k0 = 16;
    asm volatile("slli    t5,t1, 2              "                            );   // t5  = n0*4;
    asm volatile("mld.w   m0, (s1) , s3         "                            );   // m0  = A[s1] 
    asm volatile("mzero   m4                    "                            );   // m4  = 0;
    asm volatile("mul     s9,s3,t1              "                            );   // s9  = K*4*n0;
    asm volatile("add     s9 ,%0,s9             " :: "r" (addrB)             );   // s9  = startAddrB0 = addrB + K*4*n0 
    asm volatile("mld.w   m1, (s9) , a6         "                            );   // m1  = B[s9]
    asm volatile("mzero   m6                    "                            );   // m6  = 0
    asm volatile("mul     s11,s3,t4             "                            );   // s11 = K*4*(n0+WIDTH);
    asm volatile("mmaqa.b m4,m1,m0              "                            );   // m4 += m1 * m0
    asm volatile("mld.w   m2, (s2) , s3         "                            );   // m2  = A[s2]
    asm volatile("mzero   m5                    "                            );   // m5  = 0
    asm volatile("add     s11,%0,s11            " :: "r" (addrB)             );   // s11 = startAddrB1 = addrB + K*4*(n0+WIDTH)
    asm volatile("mmaqa.b m6,m1,m2              "                            );   // m6 +=  m1 * m2
    asm volatile("mld.w   m3, (s11), a6         "                            );   // m3  = B[s11]
    asm volatile("mzero   m7                    "                            );   // m7  = 0

    asm volatile("loopK_start8x8:               "                            );   // while(k0*4<K*4) { 
    asm volatile("add     s6 ,s1 ,t2            "                            );   // s6  = startAddrA0 += k0*4
    asm volatile("mmaqa.b m5,m3,m0              "                            );   // m5 +=  m3 * m0
    asm volatile("mld.w   m0, (s6) , s3         "                            );   // m0  = A[s1]
    asm volatile("add     s7 ,s9 ,t2            "                            );   // s7  = startAddrB0 += k0*4
    asm volatile("add     s5 ,s2 ,t2            "                            );   // s5  = startAddrA1 += k0*4
    asm volatile("mmaqa.b m7,m3,m2              "                            );   // m7 += m3 * m2
    asm volatile("mld.w   m1, (s7) , a6         "                            );   // m1  = B[s7]
    asm volatile("add     s8,s11,t2             "                            );   // s8  = startAddrB1 += k0*4
    asm volatile("addi    t2,t2,16              "                            );   // t2  = k0*4 += 4*4;
    asm volatile("mmaqa.b m4,m1,m0              "                            );   // m4 += m1 * m0 
    asm volatile("mld.w   m2, (s5) , s3         "                            );   // m2  = A[s5]
    asm volatile("mld.w   m3, (s8) , a6         "                            );   // m3  = B[s8]
    asm volatile("mmaqa.b m6,m1,m2              "                            );   // m6 += m1 * m2
    asm volatile("blt     t2, s3, loopK_start8x8"                            );   // endwhile(k0*4<K*4)

    asm volatile("add     s6,t5,s0              "                            );   // s6  = startAddrC00 += n0*4
    asm volatile("mst.w   m4, (s6) , s4         "                            );   // m4  -> (s6) 
    asm volatile("mmaqa.b m5,m3,m0              "                            );   // m5 += m3 * m0
    asm volatile("add     s7,t5,s10             "                            );   // s7  = startAddrC10 += n0*4
    asm volatile("mst.w   m6, (s7) , s4         "                            );   // m6  -> (s7)
    asm volatile("mmaqa.b m7,m3,m2              "                            );   // m7 += m3 * m2
    asm volatile("slli    t6,t4, 2              "                            );   // t6  = (n0+WIDTH)*4;
    asm volatile("add     s5,t6,s0              "                            );   // s5  = startAddrC01 += (n0+WIDTH)*4
    asm volatile("mst.w   m5, (s5) , s4         "                            );   // m5  -> (s5)
    asm volatile("addi    t1,t1, 8              "                            );   // t1 = n0+=2*WIDTH;
    asm volatile("add     s8,t6,s10             "                            );   // s8  = startAddrC11 += (n0+WIDTH)*4
    asm volatile("mst.w   m7, (s8) , s4         "                            );   // m7  -> (s8) 
    asm volatile("blt     t1, %0, loopN_start8x8" :: "r" (N)                 );   // endwhile(n0<N)
    
    asm volatile("add     t0,t0, 8              "                            );   // t0 = m0 +=2*WIDTH;
    asm volatile("blt     t0, %0, loopM_start8x8" :: "r" (M)                 );   // endwhile(m0<M)
  //--------------------------------------------------------------------------------

    asm volatile("lw	s0 , 0x2c(sp)           "                            );   // 
    asm volatile("lw	s1 , 0x28(sp)           "                            );   // 
    asm volatile("lw	s2 , 0x24(sp)           "                            );   // 
    asm volatile("lw	s3 , 0x20(sp)           "                            );   // 
    asm volatile("lw	s4 , 0x1c(sp)           "                            );   // 
    asm volatile("lw	s5 , 0x18(sp)           "                            );   // 
    asm volatile("lw	s6 , 0x14(sp)           "                            );   // 
    asm volatile("lw	s7 , 0x10(sp)           "                            );   // 
    asm volatile("lw	s8 , 0x0c(sp)           "                            );   // 
    asm volatile("lw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("lw	s10, 0x04(sp)           "                            );   // 
    asm volatile("lw	s11, 0x00(sp)           "                            );   // 
    asm volatile("addi	sp, sp, 0x30            "                            );   //
}