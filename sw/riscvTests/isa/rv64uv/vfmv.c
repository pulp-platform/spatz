// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>
//         Matteo Perotti <mperotti@student.ethz.ch>

#include "float_macros.h"
#include "vector_macros.h"

void TEST_CASE1(void) {
  VSET(16, e16, m8);
  float fscalar_16;
  //                            -0.9380
  BOX_HALF_IN_FLOAT(fscalar_16, 0xbb81);
  VCLEAR(v8);
  asm volatile("vfmv.v.f v8, %[A]" ::[A] "f"(fscalar_16));
  //              -0.9380, -0.9380, -0.9380, -0.9380, -0.9380, -0.9380, -0.9380,
  //              -0.9380, -0.9380, -0.9380, -0.9380, -0.9380, -0.9380, -0.9380,
  //              -0.9380, -0.9380
  VCMP_U16(1, v8, 0xbb81, 0xbb81, 0xbb81, 0xbb81, 0xbb81, 0xbb81, 0xbb81,
           0xbb81, 0xbb81, 0xbb81, 0xbb81, 0xbb81, 0xbb81, 0xbb81, 0xbb81,
           0xbb81);

  VSET(16, e32, m8);
  float fscalar_32;
  //                             -0.96056187
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbf75e762);
  VCLEAR(v8);
  asm volatile("vfmv.v.f v8, %[A]" ::[A] "f"(fscalar_32));
  //               -0.96056187, -0.96056187, -0.96056187, -0.96056187,
  //               -0.96056187, -0.96056187, -0.96056187, -0.96056187,
  //               -0.96056187, -0.96056187, -0.96056187, -0.96056187,
  //               -0.96056187, -0.96056187, -0.96056187, -0.96056187
  VCMP_U32(2, v8, 0xbf75e762, 0xbf75e762, 0xbf75e762, 0xbf75e762, 0xbf75e762,
           0xbf75e762, 0xbf75e762, 0xbf75e762, 0xbf75e762, 0xbf75e762,
           0xbf75e762, 0xbf75e762, 0xbf75e762, 0xbf75e762, 0xbf75e762,
           0xbf75e762);

#if ELEN == 64
  VSET(16, e64, m8);
  double dscalar_64;
  //                               0.9108707261227378
  BOX_DOUBLE_IN_DOUBLE(dscalar_64, 0x3fed25da5d7296fe);
  VCLEAR(v8);
  asm volatile("vfmv.v.f v8, %[A]" ::[A] "f"(dscalar_64));
  //                0.9108707261227378,  0.9108707261227378, 0.9108707261227378,
  //                0.9108707261227378,  0.9108707261227378, 0.9108707261227378,
  //                0.9108707261227378,  0.9108707261227378, 0.9108707261227378,
  //                0.9108707261227378,  0.9108707261227378, 0.9108707261227378,
  //                0.9108707261227378,  0.9108707261227378, 0.9108707261227378,
  //                0.9108707261227378
  VCMP_U64(3, v8, 0x3fed25da5d7296fe, 0x3fed25da5d7296fe, 0x3fed25da5d7296fe,
           0x3fed25da5d7296fe, 0x3fed25da5d7296fe, 0x3fed25da5d7296fe,
           0x3fed25da5d7296fe, 0x3fed25da5d7296fe, 0x3fed25da5d7296fe,
           0x3fed25da5d7296fe, 0x3fed25da5d7296fe, 0x3fed25da5d7296fe,
           0x3fed25da5d7296fe, 0x3fed25da5d7296fe, 0x3fed25da5d7296fe,
           0x3fed25da5d7296fe);
#endif
};

void TEST_CASE2(void) {
  VSET(16, e16, m8);
  float fscalar_16;
  //                            -0.9380
  BOX_HALF_IN_FLOAT(fscalar_16, 0xbb81);
  VCLEAR(v8);
  asm volatile("vfmv.s.f v8, %[A]" ::[A] "f"(fscalar_16));
  VCMP_U16(4, v8, 0xbb81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  VSET(16, e32, m8);
  float fscalar_32;
  //                             -0.96056187
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbf75e762);
  VCLEAR(v8);
  asm volatile("vfmv.s.f v8, %[A]" ::[A] "f"(fscalar_32));
  VCMP_U32(5, v8, 0xbf75e762, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

#if ELEN == 64
  VSET(16, e64, m8);
  double dscalar_64;
  //                               0.9108707261227378
  BOX_DOUBLE_IN_DOUBLE(dscalar_64, 0x3fed25da5d7296fe);
  VCLEAR(v8);
  asm volatile("vfmv.s.f v8, %[A]" ::[A] "f"(dscalar_64));
  VCMP_U64(6, v8, 0x3fed25da5d7296fe, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
#endif
};

void TEST_CASE3(void) {
  VSET(16, e32, m8);
  float fscalar_32;
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0x00000000);
  // Element 0 holds the value to extract; the rest is filler to catch
  // accidental reads of the wrong element
  VLOAD_32(v8, 0xbf75e762, 0x12345678, 0x9abcdef0, 0x0f1e2d3c, 0x4b5a6978,
           0x8796a5b4, 0xc3d2e1f0, 0x01020304, 0x05060708, 0x090a0b0c,
           0x0d0e0f10, 0x11121314, 0x15161718, 0x191a1b1c, 0x1d1e1f20,
           0x21222324);
  asm volatile("vfmv.f.s %[A], v8" : [A] "=f"(fscalar_32));
  FCMP32(7, fscalar_32, 0xbf75e762);

#if ELEN == 64
  VSET(16, e64, m8);
  double dscalar_64;
  VLOAD_64(v8, 0x3fed25da5d7296fe, 0x1234567890abcdef, 0xfedcba0987654321,
           0x0f1e2d3c4b5a6978, 0x8796a5b4c3d2e1f0, 0x0102030405060708,
           0x090a0b0c0d0e0f10, 0x1112131415161718, 0x191a1b1c1d1e1f20,
           0x2122232425262728, 0x292a2b2c2d2e2f30, 0x3132333435363738,
           0x393a3b3c3d3e3f40, 0x4142434445464748, 0x494a4b4c4d4e4f50,
           0x5152535455565758);
  asm volatile("vfmv.f.s %[A], v8" : [A] "=f"(dscalar_64));
  FCMP64(8, dscalar_64, 0x3fed25da5d7296feULL);
#endif
};

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();

  EXIT_CHECK();
}
