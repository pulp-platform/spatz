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

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();

  TEST_CASE1();
  TEST_CASE2();

  EXIT_CHECK();
}
