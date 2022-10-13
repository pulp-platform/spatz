// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "float_macros.h"
#include "vector_macros.h"

// Simple random test with similar values
void TEST_CASE1(void) {
  VSET(16, e16, m2);
  //               0.3784,  0.9043, -0.4600, -0.6748,  0.4448,  0.8804,  0.1497,
  //               0.7285,  0.9927,  0.9922,  0.8965,  0.8672, -0.1860,  0.9336,
  //               -0.2959,  0.9668
  VLOAD_16(v4, 0x360e, 0x3b3c, 0xb75c, 0xb966, 0x371e, 0x3b0b, 0x30ca, 0x39d4,
           0x3bf1, 0x3bf0, 0x3b2c, 0x3af0, 0xb1f4, 0x3b78, 0xb4bc, 0x3bbc);
  //              -0.7988, -0.5054, -0.9380, -0.7383, -0.7168,  0.2181, -0.1597,
  //              0.1833,  0.0045, -0.2152,  0.1919, -0.6914,  0.1748, -0.8604,
  //              0.6084,  0.1591
  VLOAD_16(v6, 0xba64, 0xb80b, 0xbb81, 0xb9e8, 0xb9bc, 0x32fb, 0xb11c, 0x31de,
           0x1c8f, 0xb2e3, 0x3224, 0xb988, 0x3198, 0xbae2, 0x38de, 0x3117);
  asm volatile("vfsgnj.vv v2, v4, v6");
  //              -0.3784, -0.9043, -0.4600, -0.6748, -0.4448,  0.8804, -0.1497,
  //              0.7285,  0.9927, -0.9922,  0.8965, -0.8672,  0.1860, -0.9336,
  //              0.2959,  0.9668
  VCMP_U16(1, v2, 0xb60e, 0xbb3c, 0xb75c, 0xb966, 0xb71e, 0x3b0b, 0xb0ca,
           0x39d4, 0x3bf1, 0xbbf0, 0x3b2c, 0xbaf0, 0x31f4, 0xbb78, 0x34bc,
           0x3bbc);

  VSET(16, e32, m2);
  //               0.30226409,  0.06318295, -0.82590002, -0.17829193,
  //               0.45379546,  0.85831785, -0.43186289, -0.32250872,
  //               0.35404092, -0.55081791,  0.09124859, -0.13254598,
  //               0.95786512,  0.95395225,  0.19890578,  0.76956910
  VLOAD_32(v4, 0x3e9ac25c, 0x3d816610, 0xbf536e2f, 0xbe369229, 0x3ee857e1,
           0x3f5bbab8, 0xbedd1d22, 0xbea51fdd, 0x3eb544da, 0xbf0d0267,
           0x3dbae08b, 0xbe07ba22, 0x3f7536a6, 0x3f743637, 0x3e4badf5,
           0x3f45027b);
  //               0.06560040,  0.31805936,  0.14663234, -0.85004497,
  //               -0.49171701,  0.32139263, -0.09995110, -0.34368968,
  //               0.33917251,  0.07372360,  0.70147520,  0.82915747,
  //               -0.14581841, -0.19974701, -0.58837658,  0.95794803
  VLOAD_32(v6, 0x3d865981, 0x3ea2d8ad, 0x3e1626ca, 0xbf599c8c, 0xbefbc255,
           0x3ea48d93, 0xbdccb329, 0xbeaff818, 0x3eada805, 0x3d96fc66,
           0x3f3393e1, 0x3f5443aa, 0xbe15516c, 0xbe4c8a7b, 0xbf169fd9,
           0x3f753c15);
  asm volatile("vfsgnj.vv v2, v4, v6");
  //               0.30226409,  0.06318295,  0.82590002, -0.17829193,
  //               -0.45379546,  0.85831785, -0.43186289, -0.32250872,
  //               0.35404092,  0.55081791,  0.09124859,  0.13254598,
  //               -0.95786512, -0.95395225, -0.19890578,  0.76956910
  VCMP_U32(2, v2, 0x3e9ac25c, 0x3d816610, 0x3f536e2f, 0xbe369229, 0xbee857e1,
           0x3f5bbab8, 0xbedd1d22, 0xbea51fdd, 0x3eb544da, 0x3f0d0267,
           0x3dbae08b, 0x3e07ba22, 0xbf7536a6, 0xbf743637, 0xbe4badf5,
           0x3f45027b);
};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE2(void) {
  VSET(16, e16, m2);
  //               0.3784,  0.9043, -0.4600, -0.6748,  0.4448,  0.8804,  0.1497,
  //               0.7285,  0.9927,  0.9922,  0.8965,  0.8672, -0.1860,  0.9336,
  //               -0.2959,  0.9668
  VLOAD_16(v4, 0x360e, 0x3b3c, 0xb75c, 0xb966, 0x371e, 0x3b0b, 0x30ca, 0x39d4,
           0x3bf1, 0x3bf0, 0x3b2c, 0x3af0, 0xb1f4, 0x3b78, 0xb4bc, 0x3bbc);
  //              -0.7988, -0.5054, -0.9380, -0.7383, -0.7168,  0.2181, -0.1597,
  //              0.1833,  0.0045, -0.2152,  0.1919, -0.6914,  0.1748, -0.8604,
  //              0.6084,  0.1591
  VLOAD_16(v6, 0xba64, 0xb80b, 0xbb81, 0xb9e8, 0xb9bc, 0x32fb, 0xb11c, 0x31de,
           0x1c8f, 0xb2e3, 0x3224, 0xb988, 0x3198, 0xbae2, 0x38de, 0x3117);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfsgnj.vv v2, v4, v6, v0.t");
  //               0.0000, -0.9043,  0.0000, -0.6748,  0.0000,  0.8804,  0.0000,
  //               0.7285,  0.0000, -0.9922,  0.0000, -0.8672,  0.0000, -0.9336,
  //               0.0000,  0.9668
  VCMP_U16(3, v2, 0x0, 0xbb3c, 0x0, 0xb966, 0x0, 0x3b0b, 0x0, 0x39d4, 0x0,
           0xbbf0, 0x0, 0xbaf0, 0x0, 0xbb78, 0x0, 0x3bbc);

  VSET(16, e32, m2);
  //               0.30226409,  0.06318295, -0.82590002, -0.17829193,
  //               0.45379546,  0.85831785, -0.43186289, -0.32250872,
  //               0.35404092, -0.55081791,  0.09124859, -0.13254598,
  //               0.95786512,  0.95395225,  0.19890578,  0.76956910
  VLOAD_32(v4, 0x3e9ac25c, 0x3d816610, 0xbf536e2f, 0xbe369229, 0x3ee857e1,
           0x3f5bbab8, 0xbedd1d22, 0xbea51fdd, 0x3eb544da, 0xbf0d0267,
           0x3dbae08b, 0xbe07ba22, 0x3f7536a6, 0x3f743637, 0x3e4badf5,
           0x3f45027b);
  //               0.06560040,  0.31805936,  0.14663234, -0.85004497,
  //               -0.49171701,  0.32139263, -0.09995110, -0.34368968,
  //               0.33917251,  0.07372360,  0.70147520,  0.82915747,
  //               -0.14581841, -0.19974701, -0.58837658,  0.95794803
  VLOAD_32(v6, 0x3d865981, 0x3ea2d8ad, 0x3e1626ca, 0xbf599c8c, 0xbefbc255,
           0x3ea48d93, 0xbdccb329, 0xbeaff818, 0x3eada805, 0x3d96fc66,
           0x3f3393e1, 0x3f5443aa, 0xbe15516c, 0xbe4c8a7b, 0xbf169fd9,
           0x3f753c15);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfsgnj.vv v2, v4, v6, v0.t");
  //               0.00000000,  0.06318295,  0.00000000, -0.17829193,
  //               0.00000000,  0.85831785,  0.00000000, -0.32250872,
  //               0.00000000,  0.55081791,  0.00000000,  0.13254598,
  //               0.00000000, -0.95395225,  0.00000000,  0.76956910
  VCMP_U32(4, v2, 0x0, 0x3d816610, 0x0, 0xbe369229, 0x0, 0x3f5bbab8, 0x0,
           0xbea51fdd, 0x0, 0x3f0d0267, 0x0, 0x3e07ba22, 0x0, 0xbf743637, 0x0,
           0x3f45027b);
};

// Simple random test with similar values (vector-scalar)
void TEST_CASE3(void) {
  VSET(16, e16, m2);
  float fscalar_16;
  //                              0.9023
  BOX_HALF_IN_FLOAT(fscalar_16, 0x3b38);
  //               0.5586,  0.0221,  0.7397,  0.9844, -0.1426,  0.6958,  0.0319,
  //               0.3943, -0.5425,  0.9814,  0.7852, -0.7271, -0.1810, -0.7485,
  //               -0.3499, -0.2178
  VLOAD_16(v4, 0x3878, 0x25a7, 0x39eb, 0x3be0, 0xb090, 0x3991, 0x2816, 0x364f,
           0xb857, 0x3bda, 0x3a48, 0xb9d1, 0xb1cb, 0xb9fd, 0xb599, 0xb2f8);
  asm volatile("vfsgnj.vf v2, v4, %[A]" ::[A] "f"(fscalar_16));
  //               0.5586,  0.0221,  0.7397,  0.9844,  0.1426,  0.6958,  0.0319,
  //               0.3943,  0.5425,  0.9814,  0.7852,  0.7271,  0.1810,  0.7485,
  //               0.3499,  0.2178
  VCMP_U16(5, v2, 0x3878, 0x25a7, 0x39eb, 0x3be0, 0x3090, 0x3991, 0x2816,
           0x364f, 0x3857, 0x3bda, 0x3a48, 0x39d1, 0x31cb, 0x39fd, 0x3599,
           0x32f8);

  VSET(16, e32, m2);
  float fscalar_32;
  //                               0.64529878
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0x3f25324d);
  //               0.27794743,  0.64720273,  0.88201439, -0.27750894,
  //               -0.02381280, -0.27677080, -0.58998328,  0.15329099,
  //               0.52908343, -0.63265759,  0.48432603,  0.70191479,
  //               -0.55785930,  0.34719029, -0.06872076, -0.69960916
  VLOAD_32(v4, 0x3e8e4f20, 0x3f25af14, 0x3f61cbb2, 0xbe8e15a7, 0xbcc31310,
           0xbe8db4e7, 0xbf170925, 0x3e1cf850, 0x3f077203, 0xbf21f5d9,
           0x3ef7f995, 0x3f33b0b0, 0xbf0ecfde, 0x3eb1c2ed, 0xbd8cbd78,
           0xbf331996);
  asm volatile("vfsgnj.vf v2, v4, %[A]" ::[A] "f"(fscalar_32));
  //               0.27794743,  0.64720273,  0.88201439,  0.27750894,
  //               0.02381280,  0.27677080,  0.58998328,  0.15329099,
  //               0.52908343,  0.63265759,  0.48432603,  0.70191479,
  //               0.55785930,  0.34719029,  0.06872076,  0.69960916
  VCMP_U32(6, v2, 0x3e8e4f20, 0x3f25af14, 0x3f61cbb2, 0x3e8e15a7, 0x3cc31310,
           0x3e8db4e7, 0x3f170925, 0x3e1cf850, 0x3f077203, 0x3f21f5d9,
           0x3ef7f995, 0x3f33b0b0, 0x3f0ecfde, 0x3eb1c2ed, 0x3d8cbd78,
           0x3f331996);
};

// Simple random test with similar values (vector-scalar) (masked)
void TEST_CASE4(void) {
  VSET(16, e16, m2);
  float fscalar_16;
  //                              0.9023
  BOX_HALF_IN_FLOAT(fscalar_16, 0x3b38);
  //                0.5586,  0.0221,  0.7397,  0.9844, -0.1426,  0.6958, 0.0319,
  //                0.3943, -0.5425,  0.9814,  0.7852, -0.7271, -0.1810,
  //                -0.7485, -0.3499, -0.2178
  VLOAD_16(v4, 0x3878, 0x25a7, 0x39eb, 0x3be0, 0xb090, 0x3991, 0x2816, 0x364f,
           0xb857, 0x3bda, 0x3a48, 0xb9d1, 0xb1cb, 0xb9fd, 0xb599, 0xb2f8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfsgnj.vf v2, v4, %[A], v0.t" ::[A] "f"(fscalar_16));
  //                0.0000,  0.0221,  0.0000,  0.9844,  0.0000,  0.6958, 0.0000,
  //                0.3943,  0.0000,  0.9814,  0.0000,  0.7271,  0.0000, 0.7485,
  //                0.0000,  0.2178
  VCMP_U16(7, v2, 0x0, 0x25a7, 0x0, 0x3be0, 0x0, 0x3991, 0x0, 0x364f, 0x0,
           0x3bda, 0x0, 0x39d1, 0x0, 0x39fd, 0x0, 0x32f8);

  VSET(16, e32, m2);
  float fscalar_32;
  //                               0.64529878
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0x3f25324d);
  //                0.27794743,  0.64720273,  0.88201439, -0.27750894,
  //                -0.02381280, -0.27677080, -0.58998328,  0.15329099,
  //                0.52908343, -0.63265759,  0.48432603,  0.70191479,
  //                -0.55785930,  0.34719029, -0.06872076, -0.69960916
  VLOAD_32(v4, 0x3e8e4f20, 0x3f25af14, 0x3f61cbb2, 0xbe8e15a7, 0xbcc31310,
           0xbe8db4e7, 0xbf170925, 0x3e1cf850, 0x3f077203, 0xbf21f5d9,
           0x3ef7f995, 0x3f33b0b0, 0xbf0ecfde, 0x3eb1c2ed, 0xbd8cbd78,
           0xbf331996);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfsgnj.vf v2, v4, %[A], v0.t" ::[A] "f"(fscalar_32));
  //                0.00000000,  0.64720273,  0.00000000,  0.27750894,
  //                0.00000000,  0.27677080,  0.00000000,  0.15329099,
  //                0.00000000,  0.63265759,  0.00000000,  0.70191479,
  //                0.00000000,  0.34719029,  0.00000000,  0.69960916
  VCMP_U32(8, v2, 0x0, 0x3f25af14, 0x0, 0x3e8e15a7, 0x0, 0x3e8db4e7, 0x0,
           0x3e1cf850, 0x0, 0x3f21f5d9, 0x0, 0x3f33b0b0, 0x0, 0x3eb1c2ed, 0x0,
           0x3f331996);
};

// The sign injection should work with NaNs and special values, and should not
// raise any exceptions
void TEST_CASE5(void) {
  CLEAR_FFLAGS;
  VSET(16, e16, m2);
  CHECK_FFLAGS(0);
  VLOAD_16(v4, 0x0000, 0x3b3c, 0xb75c, 0x7fff, 0x371e, 0x3b0b, 0x30ca, 0x39d4,
           0x3bf1, 0x3bf0, 0x0000, 0x3af0, 0xb1f4, 0x3b78, 0xb4bc, 0x3bbc);
  VLOAD_16(v6, 0x8000, 0xffff, 0xffff, 0xb9e8, 0xb9bc, 0x7fff, 0xb11c, 0x31de,
           0x1c8f, 0xb2e3, 0x7fff, 0xb988, 0x3198, 0xbae2, 0x38de, 0x3117);
  asm volatile("vfsgnj.vv v2, v4, v6");
  VCMP_U16(9, v2, 0x8000, 0xbb3c, 0xb75c, 0xffff, 0xb71e, 0x3b0b, 0xb0ca,
           0x39d4, 0x3bf1, 0xbbf0, 0x0000, 0xbaf0, 0x31f4, 0xbb78, 0x34bc,
           0x3bbc);

  VSET(16, e32, m2);
  VLOAD_32(v4, 0x00000000, 0x3d816610, 0xbf536e2f, 0xbe369229, 0x3ee857e1,
           0x7fffffff, 0x80000000, 0xbea51fdd, 0x3eb544da, 0xbf0d0267,
           0x3dbae08b, 0xbe07ba22, 0x3f7536a6, 0x3f743637, 0x3e4badf5,
           0x3f45027b);
  VLOAD_32(v6, 0x80000000, 0x7fffffff, 0x3e1626ca, 0xffffffff, 0xbefbc255,
           0x7fffffff, 0xffffffff, 0xbeaff818, 0x3eada805, 0x3d96fc66,
           0x3f3393e1, 0x3f5443aa, 0xbe15516c, 0xbe4c8a7b, 0xbf169fd9,
           0x3f753c15);
  asm volatile("vfsgnj.vv v2, v4, v6");
  VCMP_U32(10, v2, 0x80000000, 0x3d816610, 0x3f536e2f, 0xbe369229, 0xbee857e1,
           0x7fffffff, 0x80000000, 0xbea51fdd, 0x3eb544da, 0x3f0d0267,
           0x3dbae08b, 0x3e07ba22, 0xbf7536a6, 0xbf743637, 0xbe4badf5,
           0x3f45027b);
};

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();

  TEST_CASE1();
  //TEST_CASE2();
  TEST_CASE3();
  //TEST_CASE4();

  TEST_CASE5();

  EXIT_CHECK();
}
