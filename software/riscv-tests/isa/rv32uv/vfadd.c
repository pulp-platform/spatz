// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "float_macros.h"
#include "vector_macros.h"

// Notes: hard to check if FS is Dirtied by the first vector FP instruction
// since it is not accessible in U mode and it is dirtied before the first vfp
// operation

// Simple random test with similar values + 1 subnormal
void TEST_CASE1(void) {
  VSET(16, e16, m2);
  //             -0.8896, -0.3406,  0.7324, -0.6846, -0.2969, -0.7739,  0.5737,
  //             0.4331,  0.8940, -0.4900,  0.4219,  0.4639,  0.6694,  0.4382,
  //             0.1356,  0.5337
  VLOAD_16(v4, 0xbb1e, 0xb573, 0x39dc, 0xb97a, 0xb4c0, 0xba31, 0x3897, 0x36ee,
           0x3b27, 0xb7d7, 0x36c0, 0x376c, 0x395b, 0x3703, 0x3057, 0x0001);
  //             -0.8164,  0.6533, -0.4685,  0.6284,  0.1666,  0.9438,  0.0445,
  //             -0.1342, -0.8071, -0.3167, -0.8350,  0.2178, -0.0896, -0.3057,
  //             -0.3064,  0.2073
  VLOAD_16(v6, 0xba88, 0x393a, 0xb77f, 0x3907, 0x3155, 0x3b8d, 0x29b3, 0xb04b,
           0xba75, 0xb511, 0xbaae, 0x32f8, 0xadbc, 0xb4e4, 0xb4e7, 0x8010);
  asm volatile("vfadd.vv v2, v4, v6");
  //             -1.7061,  0.3127,  0.2639, -0.0562, -0.1302,  0.1699,  0.6182,
  //             0.2988,  0.0869, -0.8066, -0.4131,  0.6816,  0.5801,  0.1326,
  //             -0.1708,  0.7412
  VCMP_U16(1, v2, 0xbed3, 0x3501, 0x3439, 0xab30, 0xb02b, 0x3170, 0x38f2,
           0x34c8, 0x2d90, 0xba74, 0xb69c, 0x3974, 0x38a4, 0x303e, 0xb177,
           0x800f);

  VSET(16, e32, m2);
  //             -0.28968573,  0.40292332,  0.33936000,  0.53889370, 0.39942014,
  //             -0.27004066,  0.78120714, -0.15632398, -0.49984047,
  //             -0.69259918, -0.03384063, -0.62385744,  0.00338853, 0.33711585,
  //             -0.34673852,  0.11450682
  VLOAD_32(v4, 0xbe9451b0, 0x3ece4bf7, 0x3eadc098, 0x3f09f4f0, 0x3ecc80cc,
           0xbe8a42c5, 0x3f47fd31, 0xbe201365, 0xbeffeb17, 0xbf314e2e,
           0xbd0a9c78, 0xbf1fb51f, 0x3b5e1209, 0x3eac9a73, 0xbeb187b6,
           0x3dea828d);
  //             -0.62142891,  0.63306540,  0.26511025,  0.85738784,
  //             -0.78492641, -0.44331804, -0.84668529,  0.13981950, 0.84909225,
  //             0.23569171,  0.34283128,  0.56619811,  0.22596644,  0.55843508,
  //             0.53194439,  0.02510819
  VLOAD_32(v6, 0xbf1f15f7, 0x3f221093, 0x3e87bc88, 0x3f5b7dc5, 0xbf48f0f0,
           0xbee2fa95, 0xbf58c05e, 0x3e0f2cd8, 0x3f595e1c, 0x3e71592b,
           0x3eaf8795, 0x3f10f25c, 0x3e6763bf, 0x3f0ef59a, 0x3f082d82,
           0x3ccdafb0);
  asm volatile("vfadd.vv v2, v4, v6");
  //             -0.91111463,  1.03598869,  0.60447025,  1.39628148,
  //             -0.38550627, -0.71335870, -0.06547815, -0.01650448, 0.34925178,
  //             -0.45690745,  0.30899066, -0.05765933,  0.22935496, 0.89555097,
  //             0.18520588,  0.13961500
  VCMP_U32(2, v2, 0xbf693ecf, 0x3f849b47, 0x3f1abe90, 0x3fb2b95a, 0xbec56114,
           0xbf369ead, 0xbd861968, 0xbc873468, 0x3eb2d121, 0xbee9efc6,
           0x3e9e3406, 0xbd6c2c30, 0x3e6adc07, 0x3f6542d4, 0x3e3da69c,
           0x3e0ef73c);
};

// Simple random test with similar values + 1 subnormal (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE2(void) {
  VSET(16, e16, m2);
  VLOAD_16(v4, 0xbb1e, 0xb573, 0x39dc, 0xb97a, 0xb4c0, 0xba31, 0x3897, 0x36ee,
           0x3b27, 0xb7d7, 0x36c0, 0x376c, 0x395b, 0x3703, 0x3057, 0x0001);
  VLOAD_16(v6, 0xba88, 0x393a, 0xb77f, 0x3907, 0x3155, 0x3b8d, 0x29b3, 0xb04b,
           0xba75, 0xb511, 0xbaae, 0x32f8, 0xadbc, 0xb4e4, 0xb4e7, 0x8010);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfadd.vv v2, v4, v6, v0.t");
  VCMP_U16(3, v2, 0, 0x3501, 0, 0xab30, 0, 0x3170, 0, 0x34c8, 0, 0xba74, 0,
           0x3974, 0, 0x303e, 0, 0x800f);

  VSET(16, e32, m2);
  VLOAD_32(v4, 0xbe9451b0, 0x3ece4bf7, 0x3eadc098, 0x3f09f4f0, 0x3ecc80cc,
           0xbe8a42c5, 0x3f47fd31, 0xbe201365, 0xbeffeb17, 0xbf314e2e,
           0xbd0a9c78, 0xbf1fb51f, 0x3b5e1209, 0x3eac9a73, 0xbeb187b6,
           0x3dea828d);
  VLOAD_32(v6, 0xbf1f15f7, 0x3f221093, 0x3e87bc88, 0x3f5b7dc5, 0xbf48f0f0,
           0xbee2fa95, 0xbf58c05e, 0x3e0f2cd8, 0x3f595e1c, 0x3e71592b,
           0x3eaf8795, 0x3f10f25c, 0x3e6763bf, 0x3f0ef59a, 0x3f082d82,
           0x3ccdafb0);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfadd.vv v2, v4, v6, v0.t");
  VCMP_U32(4, v2, 0, 0x3f849b47, 0, 0x3fb2b95a, 0, 0xbf369ead, 0, 0xbc873468, 0,
           0xbee9efc6, 0, 0xbd6c2c30, 0, 0x3f6542d4, 0, 0x3e0ef73c);
};

// Edge-case tests
void TEST_CASE3(void) {
  VSET(16, e16, m2);
  VLOAD_16(v4, pInfh, pInfh, mInfh, qNaNh, pMaxh, pMaxh, pZero, mZeroh, pZero,
           pMaxh, pZero, qNaNh, mInfh, pInfh, qNaNh, qNaNh);
  VLOAD_16(v6, mInfh, pInfh, mInfh, pZero, pMaxh, mMaxh, pZero, mZeroh, mZeroh,
           mZeroh, mMaxh, 0x1, 0xba88, pZero, qNaNh, 0xba88);
  asm volatile("vfadd.vv v2, v4, v6");
  VCMP_U16(5, v2, qNaNh, pInfh, mInfh, qNaNh, pInfh, pZero, pZero, mZeroh,
           pZero, pMaxh, mMaxh, qNaNh, mInfh, pInfh, qNaNh, qNaNh);

  VSET(16, e32, m2);
  VLOAD_32(v4, pInff, pInff, mInff, qNaNf, pMaxf, pMaxf, pZero, mZerof, pZero,
           pMaxf, pZero, qNaNf, mInff, pInff, qNaNf, qNaNf);
  VLOAD_32(v6, mInff, pInff, mInff, pZero, pMaxf, mMaxf, pZero, mZerof, mZerof,
           mZerof, mMaxf, 0x1, 0xbf48f0f0, pZero, qNaNf, 0xbf48f0f0);
  asm volatile("vfadd.vv v2, v4, v6");
  VCMP_U32(6, v2, qNaNf, pInff, mInff, qNaNf, pInff, pZero, pZero, mZerof,
           pZero, pMaxf, mMaxf, qNaNf, mInff, pInff, qNaNf, qNaNf);
};

// Imprecise exceptions
// If the check is done immediately after the vector instruction, it fails as it
// is completed before the "faulty" operations are executed by Ara's FPU
void TEST_CASE4(void) {
  // Overflow + Inexact
  CLEAR_FFLAGS;
  VSET(16, e16, m2);
  CHECK_FFLAGS(0);
  VLOAD_16(v4, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh,
           pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh);
  VLOAD_16(v6, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh,
           pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh, pMaxh);
  asm volatile("vfadd.vv v2, v4, v6");
  VCMP_U16(7, v2, pInfh, pInfh, pInfh, pInfh, pInfh, pInfh, pInfh, pInfh,
           pInfh, pInfh, pInfh, pInfh, pInfh, pInfh, pInfh, pInfh);
  CHECK_FFLAGS(OF | NX);

  // Invalid operation, overflow
  CLEAR_FFLAGS;
  VSET(16, e32, m2);
  CHECK_FFLAGS(0);
  VLOAD_32(v4, pInff, pInff, pInff, pInff, pInff, pInff, pInff, pInff, pInff,
           pInff, pInff, pInff, pInff, pInff, pInff, pInff);
  VLOAD_32(v6, mInff, mInff, mInff, mInff, mInff, mInff, mInff, mInff, mInff,
           mInff, mInff, mInff, mInff, mInff, mInff, mInff);
  asm volatile("vfadd.vv v2, v4, v6");
  VCMP_U32(8, v2, qNaNf, qNaNf, qNaNf, qNaNf, qNaNf, qNaNf, qNaNf, qNaNf,
           qNaNf, qNaNf, qNaNf, qNaNf, qNaNf, qNaNf, qNaNf, qNaNf);
  CHECK_FFLAGS(NV);
};

// Different rounding-mode + Back-to-back rm change and vfp operation
// Index 12 (starting from 0) rounds differently for RNE and RTZ
void TEST_CASE5(void) {
  VSET(16, e16, m2);
  //             -0.8896, -0.3406,  0.7324, -0.6846, -0.2969, -0.7739,  0.5737,
  //             0.4331,  0.8940, -0.4900,  0.4219,  0.4639,  0.6694,  0.4382,
  //             0.1356,  0.5337
  VLOAD_16(v4, 0xbb1e, 0xb573, 0x39dc, 0xb97a, 0xb4c0, 0xba31, 0x3897, 0x36ee,
           0x3b27, 0xb7d7, 0x36c0, 0x376c, 0x395b, 0x3703, 0x3057, 0x0001);
  //             -0.8164,  0.6533, -0.4685,  0.6284,  0.1666,  0.9438,  0.0445,
  //             -0.1342, -0.8071, -0.3167, -0.8350,  0.2178, -0.0896, -0.3057,
  //             -0.3064,  0.2073
  VLOAD_16(v6, 0xba88, 0x393a, 0xb77f, 0x3907, 0x3155, 0x3b8d, 0x29b3, 0xb04b,
           0xba75, 0xb511, 0xbaae, 0x32f8, 0xadbc, 0xb4e4, 0xb4e7, 0x8010);
  CHANGE_RM(RM_RTZ);
  asm volatile("vfadd.vv v2, v4, v6");
  //              -1.7061,  0.3127,  0.2639, -0.0562, -0.1302,  0.1699,  0.6182,
  //              0.2988,  0.0869, -0.8066, -0.4131,  0.6816,  0.5801,  0.1326,
  //              -0.1708,  0.7412
  VCMP_U16(9, v2, 0xbed3, 0x3501, 0x3439, 0xab30, 0xb02b, 0x3170, 0x38f2,
           0x34c8, 0x2d90, 0xba74, 0xb69c, 0x3974, 0x38a3, 0x303e, 0xb177,
           0x800f);

  VSET(16, e16, m2);
  //             -0.8896, -0.3406,  0.7324, -0.6846, -0.2969, -0.7739,  0.5737,
  //             0.4331,  0.8940, -0.4900,  0.4219,  0.4639,  0.6694,  0.4382,
  //             0.1356,  0.5337
  VLOAD_16(v4, 0xbb1e, 0xb573, 0x39dc, 0xb97a, 0xb4c0, 0xba31, 0x3897, 0x36ee,
           0x3b27, 0xb7d7, 0x36c0, 0x376c, 0x395b, 0x3703, 0x3057, 0x0001);
  //             -0.8164,  0.6533, -0.4685,  0.6284,  0.1666,  0.9438,  0.0445,
  //             -0.1342, -0.8071, -0.3167, -0.8350,  0.2178, -0.0896, -0.3057,
  //             -0.3064,  0.2073
  VLOAD_16(v6, 0xba88, 0x393a, 0xb77f, 0x3907, 0x3155, 0x3b8d, 0x29b3, 0xb04b,
           0xba75, 0xb511, 0xbaae, 0x32f8, 0xadbc, 0xb4e4, 0xb4e7, 0x8010);
  CHANGE_RM(RM_RNE);
  asm volatile("vfadd.vv v2, v4, v6");
  //              -1.7061,  0.3127,  0.2639, -0.0562, -0.1302,  0.1699,  0.6182,
  //              0.2988,  0.0869, -0.8066, -0.4131,  0.6816,  0.5801,  0.1326,
  //              -0.1708,  0.7412
  VCMP_U16(10, v2, 0xbed3, 0x3501, 0x3439, 0xab30, 0xb02b, 0x3170, 0x38f2,
           0x34c8, 0x2d90, 0xba74, 0xb69c, 0x3974, 0x38a4, 0x303e, 0xb177,
           0x800f);
};

// Simple random test with similar values (vector-scalar)
void TEST_CASE6(void) {
  VSET(16, e16, m2);
  //              -0.1481, -0.1797, -0.5454,  0.3228,  0.3237, -0.7212, -0.5195,
  //              -0.4500,  0.2681,  0.7300,  0.5059,  0.5830,  0.3198, -0.1713,
  //              -0.6431,  0.4841
  VLOAD_16(v4, 0xb0bd, 0xb1c0, 0xb85d, 0x352a, 0x352e, 0xb9c5, 0xb828, 0xb733,
           0x344a, 0x39d7, 0x380c, 0x38aa, 0x351e, 0xb17b, 0xb925, 0x37bf);
  float fscalar_16;
  //                           -0.9380
  BOX_HALF_IN_FLOAT(fscalar_16, 0xbb81);
  asm volatile("vfadd.vf v2, v4, %[A]" ::[A] "f"(fscalar_16));
  //               -1.0859, -1.1172, -1.4834, -0.6152, -0.6143, -1.6592,
  //               -1.4570, -1.3877, -0.6699, -0.2080, -0.4321, -0.3550,
  //               -0.6182, -1.1094, -1.5811, -0.4539
  VCMP_U16(11, v2, 0xbc58, 0xbc78, 0xbdef, 0xb8ec, 0xb8ea, 0xbea3, 0xbdd4,
           0xbd8d, 0xb95c, 0xb2a8, 0xb6ea, 0xb5ae, 0xb8f2, 0xbc70, 0xbe53,
           0xb743);

  VSET(16, e32, m2);
  //               0.86539453, -0.53925377, -0.47128764,  0.99265540,
  //               0.32128176, -0.47335613, -0.30028856,  0.44394016,
  //               -0.72540921, -0.26464799,  0.77351445, -0.21725702,
  //               -0.25191557, -0.53123665,  0.80404943,  0.81841671
  VLOAD_32(v4, 0x3f5d8a7f, 0xbf0a0c89, 0xbef14c9d, 0x3f7e1eaa, 0x3ea47f0b,
           0xbef25bbc, 0xbe99bf6c, 0x3ee34c20, 0xbf39b46b, 0xbe877ff1,
           0x3f46050b, 0xbe5e78a0, 0xbe80fb14, 0xbf07ff20, 0x3f4dd62f,
           0x3f5183c2);
  float fscalar_32;
  //                            -0.96056187
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbf75e762);
  asm volatile("vfadd.vf v2, v4, %[A]" ::[A] "f"(fscalar_32));
  //              -0.09516734, -1.49981570, -1.43184948,  0.03209352,
  //              -0.63928008, -1.43391800, -1.26085043, -0.51662171,
  //              -1.68597102, -1.22520983, -0.18704742, -1.17781889,
  //              -1.21247745, -1.49179852, -0.15651244, -0.14214516
  VCMP_U32(12, v2, 0xbdc2e718, 0xbfbff9f6, 0xbfb746d8, 0x3d037480, 0xbf23a7dc,
           0xbfb78aa0, 0xbfa1638c, 0xbf044152, 0xbfd7cde6, 0xbf9cd3ad,
           0xbe3f895c, 0xbf96c2c5, 0xbf9b3276, 0xbfbef341, 0xbe2044cc,
           0xbe118e80);
};

// Simple random test with similar values (vector-scalar) (masked)
void TEST_CASE7(void) {
  VSET(16, e16, m2);
  VLOAD_16(v4, 0xb0bd, 0xb1c0, 0xb85d, 0x352a, 0x352e, 0xb9c5, 0xb828, 0xb733,
           0x344a, 0x39d7, 0x380c, 0x38aa, 0x351e, 0xb17b, 0xb925, 0x37bf);
  float fscalar_16;
  BOX_HALF_IN_FLOAT(fscalar_16, 0xbb81);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfadd.vf v2, v4, %[A], v0.t" ::[A] "f"(fscalar_16));
  VCMP_U16(13, v2, 0, 0xbc78, 0, 0xb8ec, 0, 0xbea3, 0, 0xbd8d, 0, 0xb2a8, 0,
           0xb5ae, 0, 0xbc70, 0, 0xb743);

  VSET(16, e32, m2);
  VLOAD_32(v4, 0x3f5d8a7f, 0xbf0a0c89, 0xbef14c9d, 0x3f7e1eaa, 0x3ea47f0b,
           0xbef25bbc, 0xbe99bf6c, 0x3ee34c20, 0xbf39b46b, 0xbe877ff1,
           0x3f46050b, 0xbe5e78a0, 0xbe80fb14, 0xbf07ff20, 0x3f4dd62f,
           0x3f5183c2);
  float fscalar_32;
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbf75e762);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfadd.vf v2, v4, %[A], v0.t" ::[A] "f"(fscalar_32));
  VCMP_U32(14, v2, 0, 0xbfbff9f6, 0, 0x3d037480, 0, 0xbfb78aa0, 0, 0xbf044152,
           0, 0xbf9cd3ad, 0, 0xbf96c2c5, 0, 0xbfbef341, 0, 0xbe118e80);
};

// Raise exceptions only on active elements!
void TEST_CASE8(void) {
  // Overflow and Inexact. Invalid operation should not be raised.
  CLEAR_FFLAGS;
  VSET(16, e16, m2);
  CHECK_FFLAGS(0);
  VLOAD_16(v4, pInfh, pMaxh, pInfh, pMaxh, pInfh, pMaxh, pInfh, pMaxh, pInfh,
           pMaxh, pInfh, pMaxh, pInfh, pMaxh, pInfh, pMaxh);
  VLOAD_16(v6, mInfh, pMaxh, mInfh, pMaxh, mInfh, pMaxh, mInfh, pMaxh, mInfh,
           pMaxh, mInfh, pMaxh, mInfh, pMaxh, mInfh, pMaxh);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfadd.vv v2, v4, v6, v0.t");
  VCMP_U16(15, v2, 0, pInfh, 0, pInfh, 0, pInfh, 0, pInfh, 0, pInfh, 0, pInfh,
           0, pInfh, 0, pInfh);
  CHECK_FFLAGS(OF | NX);

  // Invalid operation. Overflow and Inexact should not be raised.
  CLEAR_FFLAGS;
  VSET(16, e32, m2);
  CHECK_FFLAGS(0);
  VLOAD_32(v4, pMaxf, pInff, pMaxf, pInff, pMaxf, pInff, pMaxf, pInff, pMaxf,
           pInff, pMaxf, pInff, pMaxf, pInff, pMaxf, pInff);
  VLOAD_32(v6, pMaxf, mInff, pMaxf, mInff, pMaxf, mInff, pMaxf, mInff, pMaxf,
           mInff, pMaxf, mInff, pMaxf, mInff, pMaxf, mInff);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfadd.vv v2, v4, v6, v0.t");
  VCMP_U32(16, v2, 0, qNaNf, 0, qNaNf, 0, qNaNf, 0, qNaNf, 0, qNaNf, 0, qNaNf,
           0, qNaNf, 0, qNaNf);
  CHECK_FFLAGS(NV);
};

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();

  TEST_CASE1();
  //TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();
  TEST_CASE6();
  //TEST_CASE7();
  //TEST_CASE8();

  EXIT_CHECK();
}
