// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "float_macros.h"
#include "vector_macros.h"

// Simple random test with similar values + 1 subnormal
void TEST_CASE1(void) {
  VSET(16, e16, m2);
  //              -0.2161,  0.7432,  0.7871,  0.7583, -0.4546, -0.0478,  0.1260,
  //              -0.4824,  0.9282, -0.6221,  0.6543,  0.3025, -0.1420, -0.7236,
  //              0.2333, -0.0269
  VLOAD_16(v4, 0xb2ea, 0x39f2, 0x3a4c, 0x3a11, 0xb746, 0xaa1f, 0x3008, 0xb7b8,
           0x3b6d, 0xb8fa, 0x393c, 0x34d7, 0xb08b, 0xb9ca, 0x3377, 0xa6e5);
  //              -0.3289, -0.8408, -0.1754, -0.8472,  0.7739, -0.9111, -0.3152,
  //              0.4519, -0.2537,  0.9287, -0.7163, -0.2318,  0.0615, -0.2563,
  //              0.1448,  0.6606
  VLOAD_16(v6, 0xb543, 0xbaba, 0xb19d, 0xbac7, 0x3a31, 0xbb4a, 0xb50b, 0x373b,
           0xb40f, 0x3b6e, 0xb9bb, 0xb36b, 0x2bde, 0xb41a, 0x30a2, 0x3949);
  asm volatile("vfsub.vv v2, v4, v6");
  //               0.1128,  1.5840,  0.9624,  1.6055, -1.2285,  0.8633,  0.4412,
  //               -0.9346,  1.1816, -1.5508,  1.3711,  0.5342, -0.2034,
  //               -0.4673,  0.0885, -0.6875
  VCMP_U16(1, v2, 0x2f38, 0x3e56, 0x3bb3, 0x3e6c, 0xbcea, 0x3ae8, 0x370f,
           0xbb7a, 0x3cba, 0xbe34, 0x3d7c, 0x3846, 0xb282, 0xb77a, 0x2daa,
           0xb980);

  VSET(16, e32, m2);
  //              -0.12869358,  0.96847999, -0.85811919, -0.21122381,
  //              0.62089461, -0.65907389,  0.91886526, -0.57595438,
  //              -0.35377914, -0.26657876,  0.49153560,  0.42637765
  VLOAD_32(v4, 0xbe03c840, 0x3f77ee4e, 0xbf5badb3, 0xbe584b0e, 0xbd54d298,
           0x3ee0d1ec, 0x3f5e47b2, 0xbf6771a8, 0x3f1ef2f3, 0xbf28b911,
           0x3f6b3ac1, 0xbf1371bf, 0xbeb5228a, 0xbe887d03, 0x3efbaa8e,
           0x3eda4e2c);
  //              -0.50821143, -0.56901741, -0.88642830,  0.91128469,
  //              -0.00441748,  0.72763014,  0.81834352, -0.49977919,
  //              -0.94507313, -0.60766727,  0.21069343,  0.35644454,
  //              -0.51639801, -0.74812186, -0.97028691,  0.42650157
  VLOAD_32(v6, 0xbf021a25, 0xbf11ab20, 0xbf62ecf7, 0x3f6949f4, 0xbb90c083,
           0x3f3a45f8, 0x3f517ef6, 0xbeffe30f, 0xbf71f050, 0xbf1b9015,
           0x3e57c005, 0x3eb67fe6, 0xbf0432a9, 0xbf3f84ea, 0xbf7864b9,
           0x3eda5e6a);
  asm volatile("vfsub.vv v2, v4, v6");
  //               0.37951785,  1.53749740,  0.02830911, -1.12250853,
  //               -0.04754117, -0.28852856,  0.04993796,
  //               -0.40429881,  1.56596780, -0.05140662,  0.70817184,
  //               -0.93239892,  0.16261888,  0.48154309,  1.46182251,
  //               -0.00012392
  VCMP_U32(2, v2, 0x3ec2502a, 0x3fc4ccb7, 0x3ce7e880, 0xbf8fae5c, 0xbd42ba88,
           0xbe93ba04, 0x3d4c8bc0, 0xbecf0041, 0x3fc871a2, 0xbd528fc0,
           0x3f354ac0, 0xbf6eb1b2, 0x3e268590, 0x3ef68cd1, 0x3fbb1d00,
           0xb901f000);
};

// Simple random test with similar values + 1 subnormal (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE2(void) {
  VSET(16, e16, m2);
  //              -0.2161,  0.7432,  0.7871,  0.7583, -0.4546, -0.0478,  0.1260,
  //              -0.4824,  0.9282, -0.6221,  0.6543,  0.3025, -0.1420, -0.7236,
  //              0.2333, -0.0269
  VLOAD_16(v4, 0xb2ea, 0x39f2, 0x3a4c, 0x3a11, 0xb746, 0xaa1f, 0x3008, 0xb7b8,
           0x3b6d, 0xb8fa, 0x393c, 0x34d7, 0xb08b, 0xb9ca, 0x3377, 0xa6e5);
  //              -0.3289, -0.8408, -0.1754, -0.8472,  0.7739, -0.9111, -0.3152,
  //              0.4519, -0.2537,  0.9287, -0.7163, -0.2318,  0.0615, -0.2563,
  //              0.1448,  0.6606
  VLOAD_16(v6, 0xb543, 0xbaba, 0xb19d, 0xbac7, 0x3a31, 0xbb4a, 0xb50b, 0x373b,
           0xb40f, 0x3b6e, 0xb9bb, 0xb36b, 0x2bde, 0xb41a, 0x30a2, 0x3949);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfsub.vv v2, v4, v6, v0.t");
  //               0.0000,  1.5840,  0.0000,  1.6055,  0.0000,  0.8633,  0.0000,
  //               -0.9346,  0.0000, -1.5508,  0.0000,  0.5342,  0.0000,
  //               -0.4673,  0.0000, -0.6875
  VCMP_U16(3, v2, 0x0, 0x3e56, 0x0, 0x3e6c, 0x0, 0x3ae8, 0x0, 0xbb7a, 0x0,
           0xbe34, 0x0, 0x3846, 0x0, 0xb77a, 0x0, 0xb980);

  VSET(16, e32, m2);
  //              -0.12869358,  0.96847999, -0.85811919, -0.21122381,
  //              -0.05195865,  0.43910158,  0.86828148, -0.90407801,
  //              0.62089461, -0.65907389,  0.91886526, -0.57595438,
  //              -0.35377914, -0.26657876,  0.49153560,  0.42637765
  VLOAD_32(v4, 0xbe03c840, 0x3f77ee4e, 0xbf5badb3, 0xbe584b0e, 0xbd54d298,
           0x3ee0d1ec, 0x3f5e47b2, 0xbf6771a8, 0x3f1ef2f3, 0xbf28b911,
           0x3f6b3ac1, 0xbf1371bf, 0xbeb5228a, 0xbe887d03, 0x3efbaa8e,
           0x3eda4e2c);
  //              -0.50821143, -0.56901741, -0.88642830,  0.91128469,
  //              -0.00441748,  0.72763014,  0.81834352, -0.49977919,
  //              -0.94507313, -0.60766727,  0.21069343,  0.35644454,
  //              -0.51639801, -0.74812186, -0.97028691,  0.42650157
  VLOAD_32(v6, 0xbf021a25, 0xbf11ab20, 0xbf62ecf7, 0x3f6949f4, 0xbb90c083,
           0x3f3a45f8, 0x3f517ef6, 0xbeffe30f, 0xbf71f050, 0xbf1b9015,
           0x3e57c005, 0x3eb67fe6, 0xbf0432a9, 0xbf3f84ea, 0xbf7864b9,
           0x3eda5e6a);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfsub.vv v2, v4, v6, v0.t");
  //               0.00000000,  1.53749740,  0.00000000, -1.12250853,
  //               0.00000000, -0.28852856,  0.00000000, -0.40429881,
  //               0.00000000, -0.05140662,  0.00000000, -0.93239892,
  //               0.00000000,  0.48154309,  0.00000000, -0.00012392
  VCMP_U32(4, v2, 0x0, 0x3fc4ccb7, 0x0, 0xbf8fae5c, 0x0, 0xbe93ba04, 0x0,
           0xbecf0041, 0x0, 0xbd528fc0, 0x0, 0xbf6eb1b2, 0x0, 0x3ef68cd1, 0x0,
           0xb901f000);
};

// Simple random test with similar values (vector-scalar)
void TEST_CASE3(void) {
  VSET(16, e16, m2);
  //               0.9727,  0.7676,  0.0876, -0.4526, -0.1158,  0.6221,  0.7612,
  //               -0.7539,  0.3875, -0.2002,  0.2168, -0.1055, -0.4348, 0.9795,
  //               0.3650,  0.5171
  VLOAD_16(v4, 0x3bc8, 0x3a24, 0x2d9c, 0xb73e, 0xaf6a, 0x38fa, 0x3a17, 0xba08,
           0x3633, 0xb268, 0x32f0, 0xaec0, 0xb6f5, 0x3bd6, 0x35d7, 0x3823);
  float fscalar_16;
  //                            -0.8667
  BOX_HALF_IN_FLOAT(fscalar_16, 0xbaef);
  asm volatile("vfsub.vf v2, v4, %[A]" ::[A] "f"(fscalar_16));
  //               1.8398,  1.6348,  0.9541,  0.4141,  0.7510,  1.4883,  1.6279,
  //               0.1128,  1.2539,  0.6665,  1.0840,  0.7612,
  //               0.4319,  1.8457,  1.2314,  1.3838
  VCMP_U16(5, v2, 0x3f5c, 0x3e8a, 0x3ba2, 0x36a0, 0x3a02, 0x3df4, 0x3e83,
           0x2f38, 0x3d04, 0x3955, 0x3c56, 0x3a17, 0x36e9, 0x3f62, 0x3ced,
           0x3d89);

  VSET(16, e32, m2);
  //               0.85933530, -0.31821987,  0.18340160, -0.58902484,
  //               -0.83326858, -0.98716992, -0.74268776, -0.50486410,
  //               0.91496444, -0.46108878, -0.75265163, -0.17853038,
  //               0.09909800, -0.22828153,  0.31248060,  0.70940411
  VLOAD_32(v4, 0x3f5bfd66, 0xbea2edb7, 0x3e3bcda1, 0xbf16ca55, 0xbf555117,
           0xbf7cb72b, 0xbf3e20c9, 0xbf013ec6, 0x3f6a3b1c, 0xbeec13d4,
           0xbf40adc7, 0xbe36d0ab, 0x3dcaf3e5, 0xbe69c2a2, 0x3e9ffd75,
           0x3f359b82);
  float fscalar_32;
  //                             -0.16449618
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbe2871b0);
  asm volatile("vfsub.vf v2, v4, %[A]" ::[A] "f"(fscalar_32));
  //               1.02383149, -0.15372369,  0.34789777, -0.42452866,
  //               -0.66877240, -0.82267374, -0.57819158,
  //               -0.34036791,  1.07946062, -0.29659259, -0.58815545,
  //               -0.01403420,  0.26359418, -0.06378534,  0.47697678,
  //               0.87390029
  VCMP_U32(6, v2, 0x3f830ce9, 0xbe1d69be, 0x3eb21fa8, 0xbed95bd2, 0xbf2b34ab,
           0xbf529abf, 0xbf14045d, 0xbeae44b4, 0x3f8a2bc4, 0xbe97dafc,
           0xbf16915b, 0xbc65efb0, 0x3e86f5d1, 0xbd82a1e4, 0x3ef4364d,
           0x3f5fb7ee);
};

// Simple random test with similar values (vector-scalar) (masked)
void TEST_CASE4(void) {
  VSET(16, e16, m2);
  //                0.9727,  0.7676,  0.0876, -0.4526, -0.1158,  0.6221, 0.7612,
  //                -0.7539,  0.3875, -0.2002,  0.2168, -0.1055, -0.4348,
  //                0.9795,  0.3650,  0.5171
  VLOAD_16(v4, 0x3bc8, 0x3a24, 0x2d9c, 0xb73e, 0xaf6a, 0x38fa, 0x3a17, 0xba08,
           0x3633, 0xb268, 0x32f0, 0xaec0, 0xb6f5, 0x3bd6, 0x35d7, 0x3823);
  float fscalar_16;
  //                            -0.8667
  BOX_HALF_IN_FLOAT(fscalar_16, 0xbaef);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfsub.vf v2, v4, %[A], v0.t" ::[A] "f"(fscalar_16));
  //                0.0000,  1.6348,  0.0000,  0.4141,  0.0000,  1.4883, 0.0000,
  //                0.1128,  0.0000,  0.6665,  0.0000,  0.7612, 0.0000,  1.8457,
  //                0.0000,  1.3838
  VCMP_U16(7, v2, 0x0, 0x3e8a, 0x0, 0x36a0, 0x0, 0x3df4, 0x0, 0x2f38, 0x0,
           0x3955, 0x0, 0x3a17, 0x0, 0x3f62, 0x0, 0x3d89);

  VSET(16, e32, m2);
  //                0.85933530, -0.31821987,  0.18340160, -0.58902484,
  //                -0.83326858, -0.98716992, -0.74268776, -0.50486410,
  //                0.91496444, -0.46108878, -0.75265163, -0.17853038,
  //                0.09909800, -0.22828153,  0.31248060,  0.70940411
  VLOAD_32(v4, 0x3f5bfd66, 0xbea2edb7, 0x3e3bcda1, 0xbf16ca55, 0xbf555117,
           0xbf7cb72b, 0xbf3e20c9, 0xbf013ec6, 0x3f6a3b1c, 0xbeec13d4,
           0xbf40adc7, 0xbe36d0ab, 0x3dcaf3e5, 0xbe69c2a2, 0x3e9ffd75,
           0x3f359b82);
  float fscalar_32;
  //                             -0.16449618
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbe2871b0);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfsub.vf v2, v4, %[A], v0.t" ::[A] "f"(fscalar_32));
  //                0.00000000, -0.15372369,  0.00000000, -0.42452866,
  //                0.00000000, -0.82267374,  0.00000000, -0.34036791,
  //                0.00000000, -0.29659259,  0.00000000, -0.01403420,
  //                0.00000000, -0.06378534,  0.00000000,  0.87390029
  VCMP_U32(8, v2, 0x0, 0xbe1d69be, 0x0, 0xbed95bd2, 0x0, 0xbf529abf, 0x0,
           0xbeae44b4, 0x0, 0xbe97dafc, 0x0, 0xbc65efb0, 0x0, 0xbd82a1e4, 0x0,
           0x3f5fb7ee);
};

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();

  TEST_CASE1();
  //TEST_CASE2();
  TEST_CASE3();
  //TEST_CASE4();

  EXIT_CHECK();
}
