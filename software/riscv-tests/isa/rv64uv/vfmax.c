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
  //               0.0445, -0.4812,  0.5732,  0.0634,  0.2072, -0.6416,  0.7759,
  //               -0.0042,  0.6138,  0.7847, -0.5337,  0.3455,  0.0304, 0.7920,
  //               0.8179, -0.5659
  VLOAD_16(v4, 0x29b3, 0xb7b3, 0x3896, 0x2c0f, 0x32a1, 0xb922, 0x3a35, 0x9c4d,
           0x38e9, 0x3a47, 0xb845, 0x3587, 0x27ca, 0x3a56, 0x3a8b, 0xb887);
  //               0.6426, -0.4099, -0.1183,  0.2915,  0.5972, -0.1932, -0.0265,
  //               -0.5913, -0.8560,  0.5029, -0.8975, -0.7373,  0.3701, 0.9546,
  //               -0.2671, -0.6855
  VLOAD_16(v6, 0x3924, 0xb68f, 0xaf93, 0x34aa, 0x38c7, 0xb22f, 0xa6c7, 0xb8bb,
           0xbad9, 0x3806, 0xbb2e, 0xb9e6, 0x35ec, 0x3ba3, 0xb446, 0xb97c);
  asm volatile("vfmax.vv v2, v4, v6");
  //               0.6426, -0.4099,  0.5732,  0.2915,  0.5972, -0.1932,  0.7759,
  //               -0.0042,  0.6138,  0.7847, -0.5337,  0.3455,  0.3701, 0.9546,
  //               0.8179, -0.5659
  VCMP_U16(1, v2, 0x3924, 0xb68f, 0x3896, 0x34aa, 0x38c7, 0xb22f, 0x3a35,
           0x9c4d, 0x38e9, 0x3a47, 0xb845, 0x3587, 0x35ec, 0x3ba3, 0x3a8b,
           0xb887);

  VSET(16, e32, m2);
  //              -0.19589283,  0.64597517, -0.09556163,  0.96582597,
  //              0.93413597,  0.78331935, -0.18831402, -0.29520443, 0.09486515,
  //              0.96548969,  0.74523991,  0.81442171,  0.25644442,
  //              -0.92091519,  0.25139943, -0.77403748
  VLOAD_32(v4, 0xbe489821, 0x3f255ea1, 0xbdc3b5d1, 0x3f77405f, 0x3f6f2389,
           0x3f48879e, 0xbe40d564, 0xbe972509, 0x3dc248a9, 0x3f772a55,
           0x3f3ec80b, 0x3f507df1, 0x3e834caf, 0xbf6bc119, 0x3e80b76d,
           0xbf462752);
  //              -0.58921623,  0.69345474,  0.64817399, -0.00869324,
  //              0.15872470, -0.17028977, -0.99863762, -0.02739566,
  //              -0.08060763,  0.73060948,  0.62843031,  0.68798363,
  //              -0.35207590,  0.01353026,  0.25345275, -0.93635505
  VLOAD_32(v6, 0xbf16d6e0, 0x3f318640, 0x3f25eebb, 0xbc0e6e1c, 0x3e2288ba,
           0xbe2e6071, 0xbf7fa6b7, 0xbce06cdd, 0xbda5159d, 0x3f3b0939,
           0x3f20e0cf, 0x3f301fb2, 0xbeb4434b, 0x3c5dae02, 0x3e81c48f,
           0xbf6fb4f7);
  asm volatile("vfmax.vv v2, v4, v6");
  //              -0.19589283,  0.69345474,  0.64817399,  0.96582597,
  //              0.93413597,  0.78331935, -0.18831402, -0.02739566, 0.09486515,
  //              0.96548969,  0.74523991,  0.81442171,  0.25644442, 0.01353026,
  //              0.25345275, -0.77403748
  VCMP_U32(2, v2, 0xbe489821, 0x3f318640, 0x3f25eebb, 0x3f77405f, 0x3f6f2389,
           0x3f48879e, 0xbe40d564, 0xbce06cdd, 0x3dc248a9, 0x3f772a55,
           0x3f3ec80b, 0x3f507df1, 0x3e834caf, 0x3c5dae02, 0x3e81c48f,
           0xbf462752);

#if ELEN == 64
  VSET(16, e64, m2);
  //              -0.4061329687298849, -0.2985478109200665,  0.0070087316277823,
  //              -0.2169778494878496, -0.8530745559533048, -0.1247477743553222,
  //              0.5680045000966327,  0.9515829310663801, -0.9797693611753244,
  //              0.0055288881366042,  0.3717566019240965,  0.0982171502328268,
  //              -0.1563664923399100,  0.9555697921812856,  0.4810293698835877,
  //              -0.1835757691555060
  VLOAD_64(v4, 0xbfd9fe1522a16c7c, 0xbfd31b68470c6bc4, 0x3f7cb530120b5400,
           0xbfcbc5ee1fc0dc58, 0xbfeb4c6302dbd036, 0xbfbfef785b1ada80,
           0x3fe22d17c5fcaaf0, 0x3fee735e0c0b94e4, 0xbfef5a45467bddd8,
           0x3f76a5759bade800, 0x3fd7cadc33d5826c, 0x3fb924c2582803f0,
           0xbfc403d135652390, 0x3fee940719ceda38, 0x3fdec92f69043118,
           0xbfc77f692a6e3368);
  //              -0.5461826062085420, -0.4431702866722571, -0.7458438472286320,
  //              -0.8611805160192025,  0.5288841839862100,  0.4836992661145783,
  //              -0.5942889927274901,  0.5287333894552471,  0.3093279352228719,
  //              -0.5415645292681506,  0.0094485111801912, -0.2151605186231076,
  //              -0.0785069829906857,  0.6345480854408712,  0.4658290296396683,
  //              -0.5143497066150833
  VLOAD_64(v6, 0xbfe17a53f1e9e958, 0xbfdc5ce6e7f43e14, 0xbfe7ddf3ea78a228,
           0xbfeb8eca710827f8, 0x3fe0ec9e8632f518, 0x3fdef4edc443ec94,
           0xbfe3046a59846530, 0x3fe0eb6249006ebc, 0x3fd3cc0765615f4c,
           0xbfe1547f22bc2bc2, 0x3f8359bdb41e5580, 0xbfcb8a613f7035f0,
           0xbfb419089c73df20, 0x3fe44e37c956a792, 0x3fddd0248ff51b48,
           0xbfe0758d8413ceaa);
  asm volatile("vfmax.vv v2, v4, v6");
  //              -0.4061329687298849, -0.2985478109200665,  0.0070087316277823,
  //              -0.2169778494878496,  0.5288841839862100,  0.4836992661145783,
  //              0.5680045000966327,  0.9515829310663801,  0.3093279352228719,
  //              0.0055288881366042,  0.3717566019240965,  0.0982171502328268,
  //              -0.0785069829906857,  0.9555697921812856,  0.4810293698835877,
  //              -0.1835757691555060
  VCMP_U64(3, v2, 0xbfd9fe1522a16c7c, 0xbfd31b68470c6bc4, 0x3f7cb530120b5400,
           0xbfcbc5ee1fc0dc58, 0x3fe0ec9e8632f518, 0x3fdef4edc443ec94,
           0x3fe22d17c5fcaaf0, 0x3fee735e0c0b94e4, 0x3fd3cc0765615f4c,
           0x3f76a5759bade800, 0x3fd7cadc33d5826c, 0x3fb924c2582803f0,
           0xbfb419089c73df20, 0x3fee940719ceda38, 0x3fdec92f69043118,
           0xbfc77f692a6e3368);
#endif
};

// Simple random test with similar values + 1 subnormal (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE2(void) {
  VSET(16, e16, m2);
  //               0.0445, -0.4812,  0.5732,  0.0634,  0.2072, -0.6416,  0.7759,
  //               -0.0042,  0.6138,  0.7847, -0.5337,  0.3455,  0.0304, 0.7920,
  //               0.8179, -0.5659
  VLOAD_16(v4, 0x29b3, 0xb7b3, 0x3896, 0x2c0f, 0x32a1, 0xb922, 0x3a35, 0x9c4d,
           0x38e9, 0x3a47, 0xb845, 0x3587, 0x27ca, 0x3a56, 0x3a8b, 0xb887);
  //               0.6426, -0.4099, -0.1183,  0.2915,  0.5972, -0.1932, -0.0265,
  //               -0.5913, -0.8560,  0.5029, -0.8975, -0.7373,  0.3701, 0.9546,
  //               -0.2671, -0.6855
  VLOAD_16(v6, 0x3924, 0xb68f, 0xaf93, 0x34aa, 0x38c7, 0xb22f, 0xa6c7, 0xb8bb,
           0xbad9, 0x3806, 0xbb2e, 0xb9e6, 0x35ec, 0x3ba3, 0xb446, 0xb97c);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfmax.vv v2, v4, v6, v0.t");
  //               0.0000, -0.4099,  0.0000,  0.2915,  0.0000, -0.1932,  0.0000,
  //               -0.0042,  0.0000,  0.7847,  0.0000,  0.3455,  0.0000, 0.9546,
  //               0.0000, -0.5659
  VCMP_U16(4, v2, 0x0, 0xb68f, 0x0, 0x34aa, 0x0, 0xb22f, 0x0, 0x9c4d, 0x0,
           0x3a47, 0x0, 0x3587, 0x0, 0x3ba3, 0x0, 0xb887);

  VSET(16, e32, m2);
  //              -0.19589283,  0.64597517, -0.09556163,  0.96582597,
  //              0.93413597,  0.78331935, -0.18831402, -0.29520443, 0.09486515,
  //              0.96548969,  0.74523991,  0.81442171,  0.25644442,
  //              -0.92091519,  0.25139943, -0.77403748
  VLOAD_32(v4, 0xbe489821, 0x3f255ea1, 0xbdc3b5d1, 0x3f77405f, 0x3f6f2389,
           0x3f48879e, 0xbe40d564, 0xbe972509, 0x3dc248a9, 0x3f772a55,
           0x3f3ec80b, 0x3f507df1, 0x3e834caf, 0xbf6bc119, 0x3e80b76d,
           0xbf462752);
  //              -0.58921623,  0.69345474,  0.64817399, -0.00869324,
  //              0.15872470, -0.17028977, -0.99863762, -0.02739566,
  //              -0.08060763,  0.73060948,  0.62843031,  0.68798363,
  //              -0.35207590,  0.01353026,  0.25345275, -0.93635505
  VLOAD_32(v6, 0xbf16d6e0, 0x3f318640, 0x3f25eebb, 0xbc0e6e1c, 0x3e2288ba,
           0xbe2e6071, 0xbf7fa6b7, 0xbce06cdd, 0xbda5159d, 0x3f3b0939,
           0x3f20e0cf, 0x3f301fb2, 0xbeb4434b, 0x3c5dae02, 0x3e81c48f,
           0xbf6fb4f7);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfmax.vv v2, v4, v6, v0.t");
  //               0.00000000,  0.69345474,  0.00000000,  0.96582597,
  //               0.00000000,  0.78331935,  0.00000000, -0.02739566,
  //               0.00000000,  0.96548969,  0.00000000,  0.81442171,
  //               0.00000000,  0.01353026,  0.00000000, -0.77403748
  VCMP_U32(5, v2, 0x0, 0x3f318640, 0x0, 0x3f77405f, 0x0, 0x3f48879e, 0x0,
           0xbce06cdd, 0x0, 0x3f772a55, 0x0, 0x3f507df1, 0x0, 0x3c5dae02, 0x0,
           0xbf462752);

#if ELEN == 64
  VSET(16, e64, m2);
  //              -0.4061329687298849, -0.2985478109200665,  0.0070087316277823,
  //              -0.2169778494878496, -0.8530745559533048, -0.1247477743553222,
  //              0.5680045000966327,  0.9515829310663801, -0.9797693611753244,
  //              0.0055288881366042,  0.3717566019240965,  0.0982171502328268,
  //              -0.1563664923399100,  0.9555697921812856,  0.4810293698835877,
  //              -0.1835757691555060
  VLOAD_64(v4, 0xbfd9fe1522a16c7c, 0xbfd31b68470c6bc4, 0x3f7cb530120b5400,
           0xbfcbc5ee1fc0dc58, 0xbfeb4c6302dbd036, 0xbfbfef785b1ada80,
           0x3fe22d17c5fcaaf0, 0x3fee735e0c0b94e4, 0xbfef5a45467bddd8,
           0x3f76a5759bade800, 0x3fd7cadc33d5826c, 0x3fb924c2582803f0,
           0xbfc403d135652390, 0x3fee940719ceda38, 0x3fdec92f69043118,
           0xbfc77f692a6e3368);
  //              -0.5461826062085420, -0.4431702866722571, -0.7458438472286320,
  //              -0.8611805160192025,  0.5288841839862100,  0.4836992661145783,
  //              -0.5942889927274901,  0.5287333894552471,  0.3093279352228719,
  //              -0.5415645292681506,  0.0094485111801912, -0.2151605186231076,
  //              -0.0785069829906857,  0.6345480854408712,  0.4658290296396683,
  //              -0.5143497066150833
  VLOAD_64(v6, 0xbfe17a53f1e9e958, 0xbfdc5ce6e7f43e14, 0xbfe7ddf3ea78a228,
           0xbfeb8eca710827f8, 0x3fe0ec9e8632f518, 0x3fdef4edc443ec94,
           0xbfe3046a59846530, 0x3fe0eb6249006ebc, 0x3fd3cc0765615f4c,
           0xbfe1547f22bc2bc2, 0x3f8359bdb41e5580, 0xbfcb8a613f7035f0,
           0xbfb419089c73df20, 0x3fe44e37c956a792, 0x3fddd0248ff51b48,
           0xbfe0758d8413ceaa);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfmax.vv v2, v4, v6, v0.t");
  //               0.0000000000000000, -0.2985478109200665,  0.0000000000000000,
  //               -0.2169778494878496,  0.0000000000000000, 0.4836992661145783,
  //               0.0000000000000000,  0.9515829310663801,  0.0000000000000000,
  //               0.0055288881366042,  0.0000000000000000,  0.0982171502328268,
  //               0.0000000000000000,  0.9555697921812856,  0.0000000000000000,
  //               -0.1835757691555060
  VCMP_U64(6, v2, 0x0, 0xbfd31b68470c6bc4, 0x0, 0xbfcbc5ee1fc0dc58, 0x0,
           0x3fdef4edc443ec94, 0x0, 0x3fee735e0c0b94e4, 0x0, 0x3f76a5759bade800,
           0x0, 0x3fb924c2582803f0, 0x0, 0x3fee940719ceda38, 0x0,
           0xbfc77f692a6e3368);
#endif
};

// Simple random test with similar values (vector-scalar)
void TEST_CASE3(void) {
  VSET(16, e16, m2);
  float fscalar_16;
  //                              0.0368
  BOX_HALF_IN_FLOAT(fscalar_16, 0x28b5);
  //              -0.5518,  0.6772,  0.2756,  0.4421,  0.2081,  0.6250,  0.4136,
  //              0.8203, -0.3535, -0.1597, -0.5244,  0.8696,  0.1744,  0.0793,
  //              -0.2445, -0.4031
  VLOAD_16(v4, 0xb86a, 0x396b, 0x3469, 0x3713, 0x32a9, 0x3900, 0x369e, 0x3a90,
           0xb5a8, 0xb11c, 0xb832, 0x3af5, 0x3195, 0x2d14, 0xb3d3, 0xb673);
  asm volatile("vfmax.vf v2, v4, %[A]" ::[A] "f"(fscalar_16));
  //               0.0368,  0.6772,  0.2756,  0.4421,  0.2081,  0.6250,  0.4136,
  //               0.8203,  0.0368,  0.0368,  0.0368,  0.8696,  0.1744,  0.0793,
  //               0.0368,  0.0368
  VCMP_U16(7, v2, 0x28b5, 0x396b, 0x3469, 0x3713, 0x32a9, 0x3900, 0x369e,
           0x3a90, 0x28b5, 0x28b5, 0x28b5, 0x3af5, 0x3195, 0x2d14, 0x28b5,
           0x28b5);

  VSET(16, e32, m2);
  float fscalar_32;
  //                              -0.94383347
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbf719f12);
  //               0.51733643,  0.31252080,  0.47358772,  0.13738893,
  //               0.11194360, -0.33637357,  0.83680850,  0.95792335,
  //               0.41251704,  0.27496886, -0.06774041, -0.19357064,
  //               -0.48802575, -0.53921199,  0.32722279,  0.28428423
  VLOAD_32(v4, 0x3f047029, 0x3ea002ba, 0x3ef27a17, 0x3e0cafaf, 0x3de542b0,
           0xbeac3928, 0x3f563915, 0x3f753a77, 0x3ed3356f, 0x3e8cc8b8,
           0xbd8abb7c, 0xbe463762, 0xbef9de83, 0xbf0a09cc, 0x3ea789bf,
           0x3e918db4);
  asm volatile("vfmax.vf v2, v4, %[A]" ::[A] "f"(fscalar_32));
  //               0.51733643,  0.31252080,  0.47358772,  0.13738893,
  //               0.11194360, -0.33637357,  0.83680850,  0.95792335,
  //               0.41251704,  0.27496886, -0.06774041, -0.19357064,
  //               -0.48802575, -0.53921199,  0.32722279,  0.28428423
  VCMP_U32(8, v2, 0x3f047029, 0x3ea002ba, 0x3ef27a17, 0x3e0cafaf, 0x3de542b0,
           0xbeac3928, 0x3f563915, 0x3f753a77, 0x3ed3356f, 0x3e8cc8b8,
           0xbd8abb7c, 0xbe463762, 0xbef9de83, 0xbf0a09cc, 0x3ea789bf,
           0x3e918db4);

#if ELEN == 64
  VSET(16, e64, m2);
  double dscalar_64;
  //                               -0.8274885128397702
  BOX_DOUBLE_IN_DOUBLE(dscalar_64, 0xbfea7ac9308eccb6);
  //                0.9632225672084347,  0.4671677538923853,
  //                -0.1749283847947720, -0.0938698612480795,
  //                0.3438198935172891,  0.2938331380713377,
  //                -0.3607699326176230,  0.6841623039857032,
  //                -0.6959644979744999,  0.4712155929452235,
  //                0.1886883982201846,  0.9268486384654282,
  //                -0.9639662652720637, -0.2101071651393955,
  //                0.0859470276611187, -0.7001184217853196
  VLOAD_64(v4, 0x3feed2b8221dbd8e, 0x3fdde613942dab28, 0xbfc6640da5eaf690,
           0xbfb807daf023fbb0, 0x3fd601252797bdcc, 0x3fd2ce29819fd630,
           0xbfd716dac57e4298, 0x3fe5e4a85818c992, 0xbfe6455756bf47f8,
           0x3fde2865724428b0, 0x3fc826f101bec2b8, 0x3feda8be79d1a2f4,
           0xbfeed8cfc7f94e06, 0xbfcae4caa576e8a8, 0x3fb6009fd8fe2f80,
           0xbfe6675ebf9ca482);
  asm volatile("vfmax.vf v2, v4, %[A]" ::[A] "f"(dscalar_64));
  //               0.9632225672084347,  0.4671677538923853, -0.1749283847947720,
  //               -0.0938698612480795,  0.3438198935172891, 0.2938331380713377,
  //               -0.3607699326176230,  0.6841623039857032,
  //               -0.6959644979744999,  0.4712155929452235, 0.1886883982201846,
  //               0.9268486384654282, -0.8274885128397702, -0.2101071651393955,
  //               0.0859470276611187, -0.7001184217853196
  VCMP_U64(9, v2, 0x3feed2b8221dbd8e, 0x3fdde613942dab28, 0xbfc6640da5eaf690,
           0xbfb807daf023fbb0, 0x3fd601252797bdcc, 0x3fd2ce29819fd630,
           0xbfd716dac57e4298, 0x3fe5e4a85818c992, 0xbfe6455756bf47f8,
           0x3fde2865724428b0, 0x3fc826f101bec2b8, 0x3feda8be79d1a2f4,
           0xbfea7ac9308eccb6, 0xbfcae4caa576e8a8, 0x3fb6009fd8fe2f80,
           0xbfe6675ebf9ca482);
#endif
};

// Simple random test with similar values (vector-scalar) (masked)
void TEST_CASE4(void) {
  VSET(16, e16, m2);
  float fscalar_16;
  //                              0.0368
  BOX_HALF_IN_FLOAT(fscalar_16, 0x28b5);
  //               -0.5518,  0.6772,  0.2756,  0.4421,  0.2081,  0.6250, 0.4136,
  //               0.8203, -0.3535, -0.1597, -0.5244,  0.8696,  0.1744,  0.0793,
  //               -0.2445, -0.4031
  VLOAD_16(v4, 0xb86a, 0x396b, 0x3469, 0x3713, 0x32a9, 0x3900, 0x369e, 0x3a90,
           0xb5a8, 0xb11c, 0xb832, 0x3af5, 0x3195, 0x2d14, 0xb3d3, 0xb673);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfmax.vf v2, v4, %[A], v0.t" ::[A] "f"(fscalar_16));
  //                0.0000,  0.6772,  0.0000,  0.4421,  0.0000,  0.6250, 0.0000,
  //                0.8203,  0.0000,  0.0368,  0.0000,  0.8696,  0.0000, 0.0793,
  //                0.0000,  0.0368
  VCMP_U16(10, v2, 0x0, 0x396b, 0x0, 0x3713, 0x0, 0x3900, 0x0, 0x3a90, 0x0,
           0x28b5, 0x0, 0x3af5, 0x0, 0x2d14, 0x0, 0x28b5);

  VSET(16, e32, m2);
  float fscalar_32;
  //                              -0.94383347
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbf719f12);
  //                0.51733643,  0.31252080,  0.47358772,  0.13738893,
  //                0.11194360, -0.33637357,  0.83680850,  0.95792335,
  //                0.41251704,  0.27496886, -0.06774041, -0.19357064,
  //                -0.48802575, -0.53921199,  0.32722279,  0.28428423
  VLOAD_32(v4, 0x3f047029, 0x3ea002ba, 0x3ef27a17, 0x3e0cafaf, 0x3de542b0,
           0xbeac3928, 0x3f563915, 0x3f753a77, 0x3ed3356f, 0x3e8cc8b8,
           0xbd8abb7c, 0xbe463762, 0xbef9de83, 0xbf0a09cc, 0x3ea789bf,
           0x3e918db4);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfmax.vf v2, v4, %[A], v0.t" ::[A] "f"(fscalar_32));
  //                0.00000000,  0.31252080,  0.00000000,  0.13738893,
  //                0.00000000, -0.33637357,  0.00000000,  0.95792335,
  //                0.00000000,  0.27496886,  0.00000000, -0.19357064,
  //                0.00000000, -0.53921199,  0.00000000,  0.28428423
  VCMP_U32(11, v2, 0x0, 0x3ea002ba, 0x0, 0x3e0cafaf, 0x0, 0xbeac3928, 0x0,
           0x3f753a77, 0x0, 0x3e8cc8b8, 0x0, 0xbe463762, 0x0, 0xbf0a09cc, 0x0,
           0x3e918db4);

#if ELEN == 64
  VSET(16, e64, m2);
  double dscalar_64;
  //                               -0.8274885128397702
  BOX_DOUBLE_IN_DOUBLE(dscalar_64, 0xbfea7ac9308eccb6);
  //                 0.9632225672084347,  0.4671677538923853,
  //                 -0.1749283847947720, -0.0938698612480795,
  //                 0.3438198935172891,  0.2938331380713377,
  //                 -0.3607699326176230,  0.6841623039857032,
  //                 -0.6959644979744999,  0.4712155929452235,
  //                 0.1886883982201846,  0.9268486384654282,
  //                 -0.9639662652720637, -0.2101071651393955,
  //                 0.0859470276611187, -0.7001184217853196
  VLOAD_64(v4, 0x3feed2b8221dbd8e, 0x3fdde613942dab28, 0xbfc6640da5eaf690,
           0xbfb807daf023fbb0, 0x3fd601252797bdcc, 0x3fd2ce29819fd630,
           0xbfd716dac57e4298, 0x3fe5e4a85818c992, 0xbfe6455756bf47f8,
           0x3fde2865724428b0, 0x3fc826f101bec2b8, 0x3feda8be79d1a2f4,
           0xbfeed8cfc7f94e06, 0xbfcae4caa576e8a8, 0x3fb6009fd8fe2f80,
           0xbfe6675ebf9ca482);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v2);
  asm volatile("vfmax.vf v2, v4, %[A], v0.t" ::[A] "f"(dscalar_64));
  //                0.0000000000000000,  0.4671677538923853, 0.0000000000000000,
  //                -0.0938698612480795,  0.0000000000000000,
  //                0.2938331380713377,  0.0000000000000000, 0.6841623039857032,
  //                0.0000000000000000,  0.4712155929452235, 0.0000000000000000,
  //                0.9268486384654282,  0.0000000000000000,
  //                -0.2101071651393955,  0.0000000000000000,
  //                -0.7001184217853196
  VCMP_U64(12, v2, 0x0, 0x3fdde613942dab28, 0x0, 0xbfb807daf023fbb0, 0x0,
           0x3fd2ce29819fd630, 0x0, 0x3fe5e4a85818c992, 0x0, 0x3fde2865724428b0,
           0x0, 0x3feda8be79d1a2f4, 0x0, 0xbfcae4caa576e8a8, 0x0,
           0xbfe6675ebf9ca482);
#endif
};

int main(void) {
  INIT_CHECK();
  enable_vec();
  enable_fp();

  TEST_CASE1();
  // TEST_CASE2();
  TEST_CASE3();
  // TEST_CASE4();

  EXIT_CHECK();
}
