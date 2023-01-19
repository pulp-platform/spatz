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
  //              -0.6377, -0.2332,  0.9458, -0.2612, -0.6772,  0.4543,  0.1002,
  //              0.7764,  0.7979, -0.8599,  0.7837, -0.2461,  0.4221,  0.2251,
  //              0.7739,  0.1461
  VLOAD_16(v4, 0xb91a, 0xb376, 0x3b91, 0xb42e, 0xb96b, 0x3745, 0x2e69, 0x3a36,
           0x3a62, 0xbae1, 0x3a45, 0xb3e0, 0x36c1, 0x3334, 0x3a31, 0x30ad);
  //               0.9551, -0.6787,  0.5605, -0.7305, -0.7197, -0.1581,  0.7271,
  //               0.6113,  0.2971, -0.8062,  0.9668, -0.5278,  0.3972, -0.1084,
  //               -0.3015,  0.9556
  VLOAD_16(v6, 0x3ba4, 0xb96e, 0x387c, 0xb9d8, 0xb9c2, 0xb10f, 0x39d1, 0x38e4,
           0x34c1, 0xba73, 0x3bbc, 0xb839, 0x365b, 0xaef0, 0xb4d3, 0x3ba5);
  //               0.7402,  0.0935,  0.1455, -0.2771,  0.3347,  0.7964,  0.6543,
  //               -0.7534,  0.2476,  0.0338,  0.9980,  0.3284,  0.2239,
  //               -0.4551,  0.6694, -0.8550
  VLOAD_16(v2, 0x39ec, 0x2dfc, 0x30a8, 0xb46f, 0x355b, 0x3a5f, 0x393c, 0xba07,
           0x33ec, 0x2853, 0x3bfc, 0x3541, 0x332a, 0xb748, 0x395b, 0xbad7);
  asm volatile("vfnmacc.vv v2, v4, v6");
  //              -0.1313, -0.2517, -0.6758,  0.0863, -0.8223, -0.7246, -0.7271,
  //              0.2788, -0.4846, -0.7271, -1.7559, -0.4583, -0.3916,  0.4795,
  //              -0.4360,  0.7153
  VCMP_U16(1, v2, 0xb033, 0xb407, 0xb968, 0x2d86, 0xba94, 0xb9cc, 0xb9d1,
           0x3476, 0xb7c1, 0xb9d1, 0xbf06, 0xb755, 0xb644, 0x37ac, 0xb6fa,
           0x39b9);

  VSET(16, e32, m2);
  //              -0.17374928, -0.36242354, -0.18093164,  0.94970566,
  //              -0.45790458, -0.17780401, -0.51985794, -0.04832974,
  //              0.13252106,  0.77533042,  0.42536697, -0.72199643,
  //              -0.25088808,  0.28798762,  0.66300607, -0.63549894
  VLOAD_32(v4, 0xbe31eb55, 0xbeb98f94, 0xbe394625, 0x3f731fe9, 0xbeea7278,
           0xbe361241, 0xbf051569, 0xbd45f569, 0x3e07b39a, 0x3f467c0e,
           0x3ed9c9b3, 0xbf38d4c2, 0xbe807467, 0x3e93731d, 0x3f29bac4,
           0xbf22b00f);
  //              -0.61242568,  0.71439523, -0.15632962,  0.10917858,
  //              0.19637996, -0.88467985,  0.73412597, -0.98048240, 0.25438991,
  //              -0.02058743, -0.00876777,  0.21936898, -0.71130067,
  //              -0.29675287, -0.96093589,  0.24695934
  VLOAD_32(v6, 0xbf1cc7ee, 0x3f36e29b, 0xbe2014df, 0x3ddf9905, 0x3e4917d4,
           0xbf627a61, 0x3f3befae, 0xbf7b00e5, 0x3e823f65, 0xbca8a6f9,
           0xbc0fa6af, 0x3e60a243, 0xbf3617cd, 0xbe97effe, 0xbf75ffe5,
           0x3e7ce2e9);
  //               0.77600455,  0.02542816, -0.63618338,  0.11704731,
  //               0.45613721, -0.90825689,  0.21235447,  0.35766414,
  //               0.08650716, -0.98431164,  0.21029140, -0.92919809,
  //               0.46440944,  0.70648551, -0.80876821, -0.19595607
  VLOAD_32(v2, 0x3f46a83c, 0x3cd04eb8, 0xbf22dcea, 0x3defb680, 0x3ee98ad1,
           0xbf688386, 0x3e597373, 0x3eb71fc1, 0x3db12aaa, 0xbf7bfbd9,
           0x3e5756a1, 0xbf6ddfed, 0x3eedc713, 0x3f34dc3c, 0xbf4f0b6f,
           0xbe48a8b5);
  asm volatile("vfnmacc.vv v2, v4, v6");
  //              -0.88241309,  0.23348548,  0.60789841, -0.22073483,
  //              -0.36621392,  0.75095725,  0.16928674, -0.40505061,
  //              -0.12021918,  1.00027370, -0.20656188,  1.08758175,
  //              -0.64286631, -0.62102437,  1.44587445,  0.35289848
  VCMP_U32(2, v2, 0xbf61e5d3, 0x3e6f16d2, 0x3f1b9f3b, 0xbe62084f, 0xbebb8064,
           0x3f403ebc, 0x3e2d5982, 0xbecf62cb, 0xbdf63579, 0x3f8008f8,
           0xbe5384f5, 0x3f8b35e1, 0xbf2492e3, 0xbf1efb74, 0x3fb9126b,
           0x3eb4af1c);

#if ELEN == 64
  VSET(16, e64, m2);
  //              -0.3252450595073633,  0.4758165631309326, -0.1595578232245429,
  //              -0.5062008461482019, -0.8497827573746595, -0.1941654045426651,
  //              0.5653121187716577, -0.9852357785633095, -0.4238236947700038,
  //              0.5852522737985073,  0.4009389814391957, -0.8725649196362917,
  //              -0.5946782335830663,  0.4175703122760628, -0.6355596052793091,
  //              -0.3469340725892474
  VLOAD_64(v4, 0xbfd4d0d0a77142c0, 0x3fde73c75062b7e8, 0xbfc46c6408490198,
           0xbfe032cc1ded3ff0, 0xbfeb316b9bf41faa, 0xbfc8da6977433ee0,
           0x3fe2170970c503fe, 0xbfef870d2ef8e992, 0xbfdb1fed6b13a6c0,
           0x3fe2ba62f9fbf9aa, 0x3fd9a8fbf93e43f0, 0xbfebec0d442f3114,
           0xbfe3079aa59c3bf4, 0x3fdab978d4c06588, 0xbfe4568118eaaa68,
           0xbfd6342af7e8e3dc);
  //               0.9024789401717532,  0.1750129013440402,  0.5031110880652467,
  //               -0.2303324647743561, -0.3880673069078899,
  //               -0.9441232974464955, -0.9718449040015202, 0.6713775626400460,
  //               -0.0912048565692380, -0.5347347522064834,
  //               -0.5209348837668262,  0.1676058792979986,
  //               -0.3611782231841894,  0.5839305722445856,
  //               -0.5690013462620132, -0.7273345685963009
  VLOAD_64(v6, 0x3fece11b83abb9b8, 0x3fc666d29fd34b08, 0x3fe0197c6cafd8c4,
           0xbfcd7b88c1b4daf0, 0xbfd8d61841f43c54, 0xbfee36420fbd9482,
           0xbfef195a7bef10b4, 0x3fe57beccc59d47e, 0xbfb7593394338500,
           0xbfe11c8c0e185e4a, 0xbfe0ab7fa223f876, 0x3fc5741c0519e298,
           0xbfd71d8b44269f74, 0x3fe2af8f2add9a18, 0xbfe235424fb26902,
           0xbfe74653252be25a);
  //              -0.0769255470598902, -0.8447241112550155, -0.1913688167412757,
  //              0.7663381230505260,  0.2058488268749510, -0.0251549939511286,
  //              0.5275264461714482, -0.7602756587514194,  0.6498044022974587,
  //              -0.7128277097157256, -0.8385947434294139,  0.8834902787005550,
  //              0.5936682304042178,  0.1532178226844403, -0.5096194622607613,
  //              -0.8578075287458693
  VLOAD_64(v2, 0xbfb3b16484d96110, 0xbfeb07fadbff7462, 0xbfc87ec5fcb06230,
           0x3fe885d78705c2d4, 0x3fca59411dac8758, 0xbf99c23b11679a80,
           0x3fe0e17f24429b70, 0xbfe8542d9e4907ce, 0x3fe4cb329a1542de,
           0xbfe6cf7c0e9d2c04, 0xbfead5c4a4b40f1a, 0x3fec458d67ab4a36,
           0x3fe2ff5484485472, 0x3fc39ca440cc0820, 0xbfe04ecd797a151a,
           0xbfeb7328c6473c1e);
  asm volatile("vfnmacc.vv v2, v4, v6");
  //               0.3704523636601942,  0.7614500740339213,  0.2716441267930978,
  //               -0.8829326116147059, -0.5356217329860959,
  //               -0.1581610880357251, 0.0218692556270895,  1.4217408543890222,
  //               -0.6884591815896014,  1.0257824393236514,  1.0474578451230310,
  //               -0.7372432681003268, -0.8084530581760619,
  //               -0.3970498940841519,  0.1479851912270807,  0.6054703847278113
  VCMP_U64(3, v2, 0x3fd7b57dd4a95f28, 0x3fe85dcc8bb06629, 0x3fd1629e0c2e846c,
           0xbfec40fbe46ea001, 0xbfe123d0304677d2, 0xbfc43e9f5e4e7ddd,
           0x3f9664e4e6d32991, 0x3ff6bf73568fcea3, 0xbfe607db8cb1dd4b,
           0x3ff0699ad8db4c8b, 0x3ff0c263284bdf71, 0xbfe7977f31b5fc6d,
           0xbfe9ded8f2a6f4b5, 0xbfd96943f57e3046, 0x3fc2f12dc24e6a42,
           0x3fe360036da34794);
#endif
};

// Simple random test with similar values + 1 subnormal (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE2(void) {
  VSET(16, e16, m2);
  //              -0.6377, -0.2332,  0.9458, -0.2612, -0.6772,  0.4543,  0.1002,
  //              0.7764,  0.7979, -0.8599,  0.7837, -0.2461,  0.4221,  0.2251,
  //              0.7739,  0.1461
  VLOAD_16(v4, 0xb91a, 0xb376, 0x3b91, 0xb42e, 0xb96b, 0x3745, 0x2e69, 0x3a36,
           0x3a62, 0xbae1, 0x3a45, 0xb3e0, 0x36c1, 0x3334, 0x3a31, 0x30ad);
  //               0.9551, -0.6787,  0.5605, -0.7305, -0.7197, -0.1581,  0.7271,
  //               0.6113,  0.2971, -0.8062,  0.9668, -0.5278,  0.3972, -0.1084,
  //               -0.3015,  0.9556
  VLOAD_16(v6, 0x3ba4, 0xb96e, 0x387c, 0xb9d8, 0xb9c2, 0xb10f, 0x39d1, 0x38e4,
           0x34c1, 0xba73, 0x3bbc, 0xb839, 0x365b, 0xaef0, 0xb4d3, 0x3ba5);
  VLOAD_8(v0, 0xAA, 0xAA);
  //               0.7402,  0.0935,  0.1455, -0.2771,  0.3347,  0.7964,  0.6543,
  //               -0.7534,  0.2476,  0.0338,  0.9980,  0.3284,  0.2239,
  //               -0.4551,  0.6694, -0.8550
  VLOAD_16(v2, 0x39ec, 0x2dfc, 0x30a8, 0xb46f, 0x355b, 0x3a5f, 0x393c, 0xba07,
           0x33ec, 0x2853, 0x3bfc, 0x3541, 0x332a, 0xb748, 0x395b, 0xbad7);
  asm volatile("vfnmacc.vv v2, v4, v6, v0.t");
  //               0.0000, -0.2517,  0.0000,  0.0863,  0.0000, -0.7246,  0.0000,
  //               0.2788,  0.0000, -0.7271,  0.0000, -0.4583,  0.0000,  0.4795,
  //               0.0000,  0.7153
  VCMP_U16(4, v2, 0x39ec, 0xb407, 0x30a8, 0x2d86, 0x355b, 0xb9cc, 0x393c,
           0x3476, 0x33ec, 0xb9d1, 0x3bfc, 0xb755, 0x332a, 0x37ac, 0x395b,
           0x39b9);

  VSET(16, e32, m2);
  //              -0.17374928, -0.36242354, -0.18093164,  0.94970566,
  //              -0.45790458, -0.17780401, -0.51985794, -0.04832974,
  //              0.13252106,  0.77533042,  0.42536697, -0.72199643,
  //              -0.25088808,  0.28798762,  0.66300607, -0.63549894
  VLOAD_32(v4, 0xbe31eb55, 0xbeb98f94, 0xbe394625, 0x3f731fe9, 0xbeea7278,
           0xbe361241, 0xbf051569, 0xbd45f569, 0x3e07b39a, 0x3f467c0e,
           0x3ed9c9b3, 0xbf38d4c2, 0xbe807467, 0x3e93731d, 0x3f29bac4,
           0xbf22b00f);
  //              -0.61242568,  0.71439523, -0.15632962,  0.10917858,
  //              0.19637996, -0.88467985,  0.73412597, -0.98048240, 0.25438991,
  //              -0.02058743, -0.00876777,  0.21936898, -0.71130067,
  //              -0.29675287, -0.96093589,  0.24695934
  VLOAD_32(v6, 0xbf1cc7ee, 0x3f36e29b, 0xbe2014df, 0x3ddf9905, 0x3e4917d4,
           0xbf627a61, 0x3f3befae, 0xbf7b00e5, 0x3e823f65, 0xbca8a6f9,
           0xbc0fa6af, 0x3e60a243, 0xbf3617cd, 0xbe97effe, 0xbf75ffe5,
           0x3e7ce2e9);
  VLOAD_8(v0, 0xAA, 0xAA);
  //               0.77600455,  0.02542816, -0.63618338,  0.11704731,
  //               0.45613721, -0.90825689,  0.21235447,  0.35766414,
  //               0.08650716, -0.98431164,  0.21029140, -0.92919809,
  //               0.46440944,  0.70648551, -0.80876821, -0.19595607
  VLOAD_32(v2, 0x3f46a83c, 0x3cd04eb8, 0xbf22dcea, 0x3defb680, 0x3ee98ad1,
           0xbf688386, 0x3e597373, 0x3eb71fc1, 0x3db12aaa, 0xbf7bfbd9,
           0x3e5756a1, 0xbf6ddfed, 0x3eedc713, 0x3f34dc3c, 0xbf4f0b6f,
           0xbe48a8b5);
  asm volatile("vfnmacc.vv v2, v4, v6, v0.t");
  //               0.00000000,  0.23348548,  0.00000000, -0.22073483,
  //               0.00000000,  0.75095725,  0.00000000, -0.40505061,
  //               0.00000000,  1.00027370,  0.00000000,  1.08758175,
  //               0.00000000, -0.62102437,  0.00000000,  0.35289848
  VCMP_U32(5, v2, 0x3f46a83c, 0x3e6f16d2, 0xbf22dcea, 0xbe62084f, 0x3ee98ad1,
           0x3f403ebc, 0x3e597373, 0xbecf62cb, 0x3db12aaa, 0x3f8008f8,
           0x3e5756a1, 0x3f8b35e1, 0x3eedc713, 0xbf1efb74, 0xbf4f0b6f,
           0x3eb4af1c);

#if ELEN == 64
  VSET(16, e64, m2);
  //              -0.3252450595073633,  0.4758165631309326, -0.1595578232245429,
  //              -0.5062008461482019, -0.8497827573746595, -0.1941654045426651,
  //              0.5653121187716577, -0.9852357785633095, -0.4238236947700038,
  //              0.5852522737985073,  0.4009389814391957, -0.8725649196362917,
  //              -0.5946782335830663,  0.4175703122760628, -0.6355596052793091,
  //              -0.3469340725892474
  VLOAD_64(v4, 0xbfd4d0d0a77142c0, 0x3fde73c75062b7e8, 0xbfc46c6408490198,
           0xbfe032cc1ded3ff0, 0xbfeb316b9bf41faa, 0xbfc8da6977433ee0,
           0x3fe2170970c503fe, 0xbfef870d2ef8e992, 0xbfdb1fed6b13a6c0,
           0x3fe2ba62f9fbf9aa, 0x3fd9a8fbf93e43f0, 0xbfebec0d442f3114,
           0xbfe3079aa59c3bf4, 0x3fdab978d4c06588, 0xbfe4568118eaaa68,
           0xbfd6342af7e8e3dc);
  //               0.9024789401717532,  0.1750129013440402,  0.5031110880652467,
  //               -0.2303324647743561, -0.3880673069078899,
  //               -0.9441232974464955, -0.9718449040015202, 0.6713775626400460,
  //               -0.0912048565692380, -0.5347347522064834,
  //               -0.5209348837668262,  0.1676058792979986,
  //               -0.3611782231841894,  0.5839305722445856,
  //               -0.5690013462620132, -0.7273345685963009
  VLOAD_64(v6, 0x3fece11b83abb9b8, 0x3fc666d29fd34b08, 0x3fe0197c6cafd8c4,
           0xbfcd7b88c1b4daf0, 0xbfd8d61841f43c54, 0xbfee36420fbd9482,
           0xbfef195a7bef10b4, 0x3fe57beccc59d47e, 0xbfb7593394338500,
           0xbfe11c8c0e185e4a, 0xbfe0ab7fa223f876, 0x3fc5741c0519e298,
           0xbfd71d8b44269f74, 0x3fe2af8f2add9a18, 0xbfe235424fb26902,
           0xbfe74653252be25a);
  VLOAD_8(v0, 0xAA, 0xAA);
  //              -0.0769255470598902, -0.8447241112550155, -0.1913688167412757,
  //              0.7663381230505260,  0.2058488268749510, -0.0251549939511286,
  //              0.5275264461714482, -0.7602756587514194,  0.6498044022974587,
  //              -0.7128277097157256, -0.8385947434294139,  0.8834902787005550,
  //              0.5936682304042178,  0.1532178226844403, -0.5096194622607613,
  //              -0.8578075287458693
  VLOAD_64(v2, 0xbfb3b16484d96110, 0xbfeb07fadbff7462, 0xbfc87ec5fcb06230,
           0x3fe885d78705c2d4, 0x3fca59411dac8758, 0xbf99c23b11679a80,
           0x3fe0e17f24429b70, 0xbfe8542d9e4907ce, 0x3fe4cb329a1542de,
           0xbfe6cf7c0e9d2c04, 0xbfead5c4a4b40f1a, 0x3fec458d67ab4a36,
           0x3fe2ff5484485472, 0x3fc39ca440cc0820, 0xbfe04ecd797a151a,
           0xbfeb7328c6473c1e);
  asm volatile("vfnmacc.vv v2, v4, v6, v0.t");
  //               0.0000000000000000,  0.7614500740339213,  0.0000000000000000,
  //               -0.8829326116147059,  0.0000000000000000,
  //               -0.1581610880357251, 0.0000000000000000,  1.4217408543890222,
  //               0.0000000000000000,  1.0257824393236514,  0.0000000000000000,
  //               -0.7372432681003268,  0.0000000000000000,
  //               -0.3970498940841519,  0.0000000000000000,  0.6054703847278113
  VCMP_U64(6, v2, 0xbfb3b16484d96110, 0x3fe85dcc8bb06629, 0xbfc87ec5fcb06230,
           0xbfec40fbe46ea001, 0x3fca59411dac8758, 0xbfc43e9f5e4e7ddd,
           0x3fe0e17f24429b70, 0x3ff6bf73568fcea3, 0x3fe4cb329a1542de,
           0x3ff0699ad8db4c8b, 0xbfead5c4a4b40f1a, 0xbfe7977f31b5fc6d,
           0x3fe2ff5484485472, 0xbfd96943f57e3046, 0xbfe04ecd797a151a,
           0x3fe360036da34794);
#endif
};

// Simple random test with similar values (vector-scalar)
void TEST_CASE3(void) {
  VSET(16, e16, m2);
  float fscalar_16;
  //                              0.1300
  BOX_HALF_IN_FLOAT(fscalar_16, 0x3029);
  //              -0.2844,  0.4070, -0.1837, -0.2321, -0.5283, -0.6104, -0.7183,
  //              -0.1191,  0.7998,  0.1169,  0.1169, -0.9214, -0.4360, -0.6250,
  //              -0.5386,  0.6543
  VLOAD_16(v4, 0xb48d, 0x3683, 0xb1e1, 0xb36d, 0xb83a, 0xb8e2, 0xb9bf, 0xaf9f,
           0x3a66, 0x2f7c, 0x2f7c, 0xbb5f, 0xb6fa, 0xb900, 0xb84f, 0x393c);
  //               0.9268, -0.3337, -0.3225, -0.8306, -0.1857, -0.6831,  0.0557,
  //               0.5586,  0.2352,  0.6294,  0.6294, -0.8877, -0.2426,  0.5488,
  //               0.4001,  0.1772
  VLOAD_16(v2, 0x3b6a, 0xb557, 0xb529, 0xbaa5, 0xb1f1, 0xb977, 0x2b21, 0x3878,
           0x3387, 0x3909, 0x3909, 0xbb1a, 0xb3c3, 0x3864, 0x3667, 0x31ac);
  asm volatile("vfnmacc.vf v2, %[A], v4" ::[A] "f"(fscalar_16));
  //              -0.8896,  0.2808,  0.3464,  0.8608,  0.2544,  0.7627,  0.0377,
  //              -0.5430, -0.3394, -0.6445, -0.6445,  1.0078,  0.2993, -0.4675,
  //              -0.3301, -0.2622
  VCMP_U16(7, v2, 0xbb1e, 0x347e, 0x358b, 0x3ae3, 0x3412, 0x3a1a, 0x28d3,
           0xb858, 0xb56d, 0xb928, 0xb928, 0x3c08, 0x34ca, 0xb77b, 0xb548,
           0xb432);

  VSET(16, e32, m2);
  float fscalar_32;
  //                              -0.26917368
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbe89d122);
  //              -0.27745819, -0.86308837, -0.16746511, -0.68674469,
  //              -0.49064314, -0.74352056, -0.17169137,  0.26071417,
  //              0.71857828,  0.07920383, -0.43244356, -0.58339220, 0.80679923,
  //              0.23900302,  0.73513943, -0.80685192
  VLOAD_32(v4, 0xbe8e0f00, 0xbf5cf35c, 0xbe2b7bf9, 0xbf2fce80, 0xbefb3594,
           0xbf3e575d, 0xbe2fcfdd, 0x3e857c54, 0x3f37f4bf, 0x3da2359e,
           0xbedd693e, 0xbf155931, 0x3f4e8a65, 0x3e74bd35, 0x3f3c3219,
           0xbf4e8dd9);
  //               0.13509545, -0.29169917,  0.80494332, -0.63637137,
  //               0.63772237, -0.87242430, -0.44194883, -0.41286576,
  //               -0.57735479,  0.61664599,  0.94073379, -0.89744234,
  //               -0.70681161,  0.23247144,  0.06774496, -0.38581881
  VLOAD_32(v2, 0x3e0a5676, 0xbe955998, 0x3f4e10c4, 0xbf22e93c, 0x3f2341c6,
           0xbf5f5733, 0xbee2471e, 0xbed36324, 0xbf13cd86, 0x3f1ddc83,
           0x3f70d3ee, 0xbf65bec8, 0xbf34f19b, 0x3e6e0cfe, 0x3d8abdde,
           0xbec58a0b);
  asm volatile("vfnmacc.vf v2, %[A], v4" ::[A] "f"(fscalar_32));
  //              -0.20977989,  0.05937849, -0.85002053,  0.45151776,
  //              -0.76979059,  0.67228812,  0.39573404,  0.48304313,
  //              0.77077717, -0.59532642, -1.05713618,  0.74040854, 0.92398071,
  //              -0.16813812,  0.13013524,  0.16863550
  VCMP_U32(8, v2, 0xbe56d08a, 0x3d7336de, 0xbf599af2, 0x3ee72d57, 0xbf4510ff,
           0x3f2c1b13, 0x3eca9da7, 0x3ef7516f, 0x3f4551a7, 0xbf186750,
           0xbf87503d, 0x3f3d8b6a, 0x3f6c8a00, 0xbe2c2c66, 0x3e05422c,
           0x3e2caec9);

#if ELEN == 64
  VSET(16, e64, m2);
  double dscalar_64;
  //                                0.1021836258281641
  BOX_DOUBLE_IN_DOUBLE(dscalar_64, 0x3fba28b4c31e60e0);
  //                0.3274079371230154,  0.9254873656544997, 0.9683609308176633,
  //                -0.6778040243955326,  0.9669615854915627,
  //                0.8026267269428324, -0.7388821308618641, 0.7432413708598076,
  //                0.9355143976513562, -0.4219868517017851, 0.8950700270161456,
  //                0.6727820676214205, -0.8833440526985297, 0.0357808590148252,
  //                -0.3802125831332157,  0.9831607630398518
  VLOAD_64(v4, 0x3fd4f4406b993a2c, 0x3fed9d97ae0b1cd6, 0x3feefcd01012c05e,
           0xbfe5b09210bc082e, 0x3feef1596c459614, 0x3fe9af1e3ee3adfa,
           0xbfe7a4ec2374d0e2, 0x3fe7c8a2209c110c, 0x3fedefbbe3db30dc,
           0xbfdb01d523d9acc0, 0x3feca469e5b540fa, 0x3fe5876e42389dca,
           0xbfec445abf2e99e4, 0x3fa251de66953a60, 0xbfd8556728856e10,
           0x3fef760d8f7eee22);
  //                0.1671854121593166,  0.6264287337062140, 0.1587305627009998,
  //                -0.3348358495277817,  0.4721131630506652,
  //                0.2878076790245236,  0.5083797506594245, 0.9444607965181537,
  //                -0.2805814092841707, -0.7218856627753110,
  //                -0.3443302881655670,  0.3680926220616383,
  //                -0.2344410843781140,  0.3553553454507421,
  //                0.0951222110617760, -0.8329780449088213
  VLOAD_64(v2, 0x3fc56654e2cbd888, 0x3fe40bb445915f4a, 0x3fc4514877d696a0,
           0xbfd56df357d00344, 0x3fde371a20d41408, 0x3fd26b70e63cabf0,
           0x3fe044a59c60fcd4, 0x3fee3905d92cc95e, 0xbfd1f50bba2f6e40,
           0xbfe719aff62247a4, 0xbfd60981e7ac601c, 0x3fd78ed45b69d4fc,
           0xbfce022a5b1f1348, 0x3fd6be2458cadcb0, 0x3fb859ede1a22f80,
           0xbfeaa7c192a56bc8);
  asm volatile("vfnmacc.vf v2, %[A], v4" ::[A] "f"(dscalar_64));
  //              -0.2006411422994659, -0.7209983883869466, -0.2576811937222847,
  //              0.4040963223414386, -0.5709208038927434, -0.3698229881701340,
  //              -0.4328780954683192, -1.0204078946581041,  0.1849871561177042,
  //              0.7650058093340112,  0.2528687874349445, -0.4368399331233641,
  //              0.3247043825365946, -0.3590115633601233, -0.0562707107317317,
  //              0.7325151133694248
  VCMP_U64(9, v2, 0xbfc9ae9be43442c9, 0xbfe7126b3652e68a, 0xbfd07dd942f53687,
           0x3fd9dcb6d238fb8a, 0xbfe244fbb4aa695e, 0xbfd7ab2e09dffb6d,
           0xbfdbb44653cc3c92, 0xbff053973a823036, 0x3fc7ada8bcda50a5,
           0x3fe87aed768addeb, 0x3fd02f00910d95b4, 0xbfdbf52f7a9681dc,
           0x3fd4c7f4e3f733f9, 0xbfd6fa0ba2e11fba, 0xbfaccf83bca18481,
           0x3fe770c388f7eacc);
#endif
};

// Simple random test with similar values (vector-scalar) (masked)
void TEST_CASE4(void) {
  VSET(16, e16, m2);
  float fscalar_16;
  //                              0.1300
  BOX_HALF_IN_FLOAT(fscalar_16, 0x3029);
  //               -0.2844,  0.4070, -0.1837, -0.2321, -0.5283, -0.6104,
  //               -0.7183, -0.1191,  0.7998,  0.1169,  0.2551, -0.9214,
  //               -0.4360, -0.6250, -0.5386,  0.6543
  VLOAD_16(v4, 0xb48d, 0x3683, 0xb1e1, 0xb36d, 0xb83a, 0xb8e2, 0xb9bf, 0xaf9f,
           0x3a66, 0x2f7c, 0x3415, 0xbb5f, 0xb6fa, 0xb900, 0xb84f, 0x393c);
  VLOAD_8(v0, 0xAA, 0xAA);
  //                0.9268, -0.3337, -0.3225, -0.8306, -0.1857, -0.6831, 0.0557,
  //                0.5586,  0.2352,  0.6294, -0.0325, -0.8877, -0.2426, 0.5488,
  //                0.4001,  0.1772
  VLOAD_16(v2, 0x3b6a, 0xb557, 0xb529, 0xbaa5, 0xb1f1, 0xb977, 0x2b21, 0x3878,
           0x3387, 0x3909, 0xa828, 0xbb1a, 0xb3c3, 0x3864, 0x3667, 0x31ac);
  asm volatile("vfnmacc.vf v2, %[A], v4, v0.t" ::[A] "f"(fscalar_16));
  //                0.0000,  0.2808,  0.0000,  0.8608,  0.0000,  0.7627, 0.0000,
  //                -0.5430,  0.0000, -0.6445,  0.0000,  1.0078,  0.0000,
  //                -0.4675,  0.0000, -0.2622
  VCMP_U16(10, v2, 0x3b6a, 0x347e, 0xb529, 0x3ae3, 0xb1f1, 0x3a1a, 0x2b21,
           0xb858, 0x3387, 0xb928, 0xa828, 0x3c08, 0xb3c3, 0xb77b, 0x3667,
           0xb432);

  VSET(16, e32, m2);
  float fscalar_32;
  //                              -0.26917368
  BOX_FLOAT_IN_FLOAT(fscalar_32, 0xbe89d122);
  //               -0.27745819, -0.86308837, -0.16746511, -0.68674469,
  //               -0.49064314, -0.74352056, -0.17169137,  0.26071417,
  //               0.71857828,  0.07920383, -0.43244356, -0.58339220,
  //               0.80679923,  0.23900302,  0.73513943, -0.80685192
  VLOAD_32(v4, 0xbe8e0f00, 0xbf5cf35c, 0xbe2b7bf9, 0xbf2fce80, 0xbefb3594,
           0xbf3e575d, 0xbe2fcfdd, 0x3e857c54, 0x3f37f4bf, 0x3da2359e,
           0xbedd693e, 0xbf155931, 0x3f4e8a65, 0x3e74bd35, 0x3f3c3219,
           0xbf4e8dd9);
  VLOAD_8(v0, 0xAA, 0xAA);
  //                0.13509545, -0.29169917,  0.80494332, -0.63637137,
  //                0.63772237, -0.87242430, -0.44194883, -0.41286576,
  //                -0.57735479,  0.61664599,  0.94073379, -0.89744234,
  //                -0.70681161,  0.23247144,  0.06774496, -0.38581881
  VLOAD_32(v2, 0x3e0a5676, 0xbe955998, 0x3f4e10c4, 0xbf22e93c, 0x3f2341c6,
           0xbf5f5733, 0xbee2471e, 0xbed36324, 0xbf13cd86, 0x3f1ddc83,
           0x3f70d3ee, 0xbf65bec8, 0xbf34f19b, 0x3e6e0cfe, 0x3d8abdde,
           0xbec58a0b);
  asm volatile("vfnmacc.vf v2, %[A], v4, v0.t" ::[A] "f"(fscalar_32));
  //                0.00000000,  0.05937849,  0.00000000,  0.45151776,
  //                0.00000000,  0.67228812,  0.00000000,  0.48304313,
  //                0.00000000, -0.59532642,  0.00000000,  0.74040854,
  //                0.00000000, -0.16813812,  0.00000000,  0.16863550
  VCMP_U32(11, v2, 0x3e0a5676, 0x3d7336de, 0x3f4e10c4, 0x3ee72d57, 0x3f2341c6,
           0x3f2c1b13, 0xbee2471e, 0x3ef7516f, 0xbf13cd86, 0xbf186750,
           0x3f70d3ee, 0x3f3d8b6a, 0xbf34f19b, 0xbe2c2c66, 0x3d8abdde,
           0x3e2caec9);

#if ELEN == 64
  VSET(16, e64, m2);
  double dscalar_64;
  //                                0.1021836258281641
  BOX_DOUBLE_IN_DOUBLE(dscalar_64, 0x3fba28b4c31e60e0);
  //                 0.3274079371230154,  0.9254873656544997,
  //                 0.9683609308176633, -0.6778040243955326,
  //                 0.9669615854915627,  0.8026267269428324,
  //                 -0.7388821308618641,  0.7432413708598076,
  //                 0.9355143976513562, -0.4219868517017851,
  //                 0.8950700270161456,  0.6727820676214205,
  //                 -0.8833440526985297,  0.0357808590148252,
  //                 -0.3802125831332157,  0.9831607630398518
  VLOAD_64(v4, 0x3fd4f4406b993a2c, 0x3fed9d97ae0b1cd6, 0x3feefcd01012c05e,
           0xbfe5b09210bc082e, 0x3feef1596c459614, 0x3fe9af1e3ee3adfa,
           0xbfe7a4ec2374d0e2, 0x3fe7c8a2209c110c, 0x3fedefbbe3db30dc,
           0xbfdb01d523d9acc0, 0x3feca469e5b540fa, 0x3fe5876e42389dca,
           0xbfec445abf2e99e4, 0x3fa251de66953a60, 0xbfd8556728856e10,
           0x3fef760d8f7eee22);
  VLOAD_8(v0, 0xAA, 0xAA);
  //                 0.1671854121593166,  0.6264287337062140,
  //                 0.1587305627009998, -0.3348358495277817,
  //                 0.4721131630506652,  0.2878076790245236,
  //                 0.5083797506594245,  0.9444607965181537,
  //                 -0.2805814092841707, -0.7218856627753110,
  //                 -0.3443302881655670,  0.3680926220616383,
  //                 -0.2344410843781140,  0.3553553454507421,
  //                 0.0951222110617760, -0.8329780449088213
  VLOAD_64(v2, 0x3fc56654e2cbd888, 0x3fe40bb445915f4a, 0x3fc4514877d696a0,
           0xbfd56df357d00344, 0x3fde371a20d41408, 0x3fd26b70e63cabf0,
           0x3fe044a59c60fcd4, 0x3fee3905d92cc95e, 0xbfd1f50bba2f6e40,
           0xbfe719aff62247a4, 0xbfd60981e7ac601c, 0x3fd78ed45b69d4fc,
           0xbfce022a5b1f1348, 0x3fd6be2458cadcb0, 0x3fb859ede1a22f80,
           0xbfeaa7c192a56bc8);
  asm volatile("vfnmacc.vf v2, %[A], v4, v0.t" ::[A] "f"(dscalar_64));
  //                -0.2006411422994659, -0.7209983883869466,
  //                -0.2576811937222847,  0.4040963223414386,
  //                -0.5709208038927434, -0.3698229881701340,
  //                -0.4328780954683192, -1.0204078946581041,
  //                0.1849871561177042,  0.7650058093340112, 0.2528687874349445,
  //                -0.4368399331233641,  0.3247043825365946,
  //                -0.3590115633601233, -0.0562707107317317, 0.7325151133694248
  VCMP_U64(12, v2, 0x3fc56654e2cbd888, 0xbfe7126b3652e68a, 0x3fc4514877d696a0,
           0x3fd9dcb6d238fb8a, 0x3fde371a20d41408, 0xbfd7ab2e09dffb6d,
           0x3fe044a59c60fcd4, 0xbff053973a823036, 0xbfd1f50bba2f6e40,
           0x3fe87aed768addeb, 0xbfd60981e7ac601c, 0xbfdbf52f7a9681dc,
           0xbfce022a5b1f1348, 0xbfd6fa0ba2e11fba, 0x3fb859ede1a22f80,
           0x3fe770c388f7eacc);
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
