// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "float_macros.h"
#include "vector_macros.h"

// We assume RNE rounding when not specified by the encoding

/////////////////
// vfwcvt.xu.f //
/////////////////

void TEST_CASE1(void) {
  VSET(16, e16, m2);
  //                 56.438,    -30.938,    -68.438,    -32.969,     56.438,
  //                 -5.816,   53.094, -29.875, -93.562, -90.750, -65.875,
  //                 -91.062,   16.281, -77.938, -67.000, -51.844
  VLOAD_16(v4, 0x530e, 0xcfbc, 0xd447, 0xd01f, 0x530e, 0xc5d1, 0x52a3, 0xcf78,
           0xd5d9, 0xd5ac, 0xd41e, 0xd5b1, 0x4c12, 0xd4df, 0xd430, 0xd27b);
  asm volatile("vfwcvt.xu.f.v v8, v4");
  //                           56,              0,              0, 0, 56, 0, 53,
  //                           0,              0,              0, 0, 0, 16, 0,
  //                           0,              0
  VCMP_U32(1, v8, 0x00000038, 0x00000000, 0x00000000, 0x00000000, 0x00000038,
           0x00000000, 0x00000035, 0x00000000, 0x00000000, 0x00000000,
           0x00000000, 0x00000000, 0x00000010, 0x00000000, 0x00000000,
           0x00000000);

  VSET(16, e32, m2);
  //                  -54444.973,      43481.863,   88447.461,   32690.551,
  //                  -37979.809,   68218.094, -43036.512, -38011.395,
  //                  -36599.363,   48418.234,   81414.820,   16330.853,
  //                  75606.320, -85030.219,   13033.059,   7375.421
  VLOAD_32(v4, 0xc754acf9, 0x4729d9dd, 0x47acbfbb, 0x46ff651a, 0xc7145bcf,
           0x47853d0c, 0xc7281c83, 0xc7147b65, 0xc70ef75d, 0x473d223c,
           0x479f0369, 0x467f2b69, 0x4793ab29, 0xc7a6131c, 0x464ba43c,
           0x45e67b5f);
  asm volatile("vfwcvt.xu.f.v v8, v4");
  //                                   0,                  43482, 88447, 32691,
  //                                   0,                  68218, 0, 0, 0,
  //                                   48418,                  81415, 16331,
  //                                   75606,                      0, 13033,
  //                                   7375
  VCMP_U64(2, v8, 0x0000000000000000, 0x000000000000a9da, 0x000000000001597f,
           0x0000000000007fb3, 0x0000000000000000, 0x0000000000010a7a,
           0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
           0x000000000000bd22, 0x0000000000013e07, 0x0000000000003fcb,
           0x0000000000012756, 0x0000000000000000, 0x00000000000032e9,
           0x0000000000001ccf);
};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE2(void) {
  VSET(16, e16, m2);
  //                 -36.375,    56.438, -68.438, -32.969,   56.438,
  //                 -5.816,   53.094, -29.875, -93.562, -90.750, -65.875,
  //                 -91.062,   16.281, -77.938, -67.000, -51.844
  VLOAD_16(v4, 0xd08c, 0x530e, 0xd447, 0xd01f, 0x530e, 0xc5d1, 0x52a3, 0xcf78,
           0xd5d9, 0xd5ac, 0xd41e, 0xd5b1, 0x4c12, 0xd4df, 0xd430, 0xd27b);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.xu.f.v v8, v4, v0.t");
  //                           0,             56,              0, 0, 0, 0, 0, 0,
  //                           0,              0,              0, 0, 0, 0, 0, 0
  VCMP_U32(3, v8, 0x00000000, 0x00000038, 0x00000000, 0x00000000, 0x00000000,
           0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
           0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
           0x00000000);

  VSET(16, e32, m2);
  //              -54444.973,   43481.863,   88447.461,   32690.551, -37979.809,
  //              68218.094, -43036.512, -38011.395, -36599.363,   48418.234,
  //              81414.820,   16330.853,   75606.320, -85030.219,   13033.059,
  //              7375.421
  VLOAD_32(v4, 0xc754acf9, 0x4729d9dd, 0x47acbfbb, 0x46ff651a, 0xc7145bcf,
           0x47853d0c, 0xc7281c83, 0xc7147b65, 0xc70ef75d, 0x473d223c,
           0x479f0369, 0x467f2b69, 0x4793ab29, 0xc7a6131c, 0x464ba43c,
           0x45e67b5f);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.xu.f.v v8, v4, v0.t");
  //                                   0,                  43482, 0, 32691, 0,
  //                                   68218,                      0, 0, 0,
  //                                   48418,                      0, 16331, 0,
  //                                   0,                      0, 7375
  VCMP_U64(4, v8, 0x0000000000000000, 0x000000000000a9da, 0x0000000000000000,
           0x0000000000007fb3, 0x0000000000000000, 0x0000000000010a7a,
           0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
           0x000000000000bd22, 0x0000000000000000, 0x0000000000003fcb,
           0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
           0x0000000000001ccf);
};

////////////////
// vfwcvt.x.f //
////////////////

// Simple random test with similar values
void TEST_CASE3(void) {
  VSET(16, e16, m2);
  //                 -55.656,    -23.391,     53.094, -0.356,   26.859, -81.938,
  //                 63.625, -54.594, -36.375,   77.312,   73.188, -79.500,
  //                 -22.047, -30.500,   33.375, -26.281
  VLOAD_16(v4, 0xd2f5, 0xcdd9, 0x52a3, 0xb5b2, 0x4eb7, 0xd51f, 0x53f4, 0xd2d3,
           0xd08c, 0x54d5, 0x5493, 0xd4f8, 0xcd83, 0xcfa0, 0x502c, 0xce92);
  asm volatile("vfwcvt.x.f.v v8, v4");
  //                         -56,            -23,             53, 0, 27, -82,
  //                         64,            -55,            -36,             77,
  //                         73,            -80,            -22,            -30,
  //                         33,            -26
  VCMP_U32(5, v8, 0xffffffc8, 0xffffffe9, 0x00000035, 0x00000000, 0x0000001b,
           0xffffffae, 0x00000040, 0xffffffc9, 0xffffffdc, 0x0000004d,
           0x00000049, 0xffffffb0, 0xffffffea, 0xffffffe2, 0x00000021,
           0xffffffe6);

  VSET(16, e32, m2);
  //                  -22345.104,     -55208.160,   60155.754, -4924.268,
  //                  -42337.285, -60609.004,   51795.328,   33876.547,
  //                  -99812.922,   59419.867, -78706.844,   72266.555,
  //                  -70664.008, -83501.727, -15981.749, -2004.535
  VLOAD_32(v4, 0xc6ae9235, 0xc757a829, 0x476afbc1, 0xc599e225, 0xc7256149,
           0xc76cc101, 0x474a5354, 0x4704548c, 0xc7c2f276, 0x47681bde,
           0xc799b96c, 0x478d2547, 0xc78a0401, 0xc7a316dd, 0xc679b6ff,
           0xc4fa9120);
  asm volatile("vfwcvt.x.f.v v8, v4");
  //                              -22345,                 -55208, 60156, -4924,
  //                              -42337,                 -60609, 51795, 33877,
  //                              -99813,                  59420, -78707, 72267,
  //                              -70664,                 -83502, -15982, -2005
  VCMP_U64(6, v8, 0xffffffffffffa8b7, 0xffffffffffff2858, 0x000000000000eafc,
           0xffffffffffffecc4, 0xffffffffffff5a9f, 0xffffffffffff133f,
           0x000000000000ca53, 0x0000000000008455, 0xfffffffffffe7a1b,
           0x000000000000e81c, 0xfffffffffffecc8d, 0x0000000000011a4b,
           0xfffffffffffeebf8, 0xfffffffffffeb9d2, 0xffffffffffffc192,
           0xfffffffffffff82b);
};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE4(void) {
  VSET(16, e16, m2);
  //                 -55.656,    -23.391,   53.094, -0.356,   26.859,
  //                 -81.938,   63.625, -54.594, -36.375,   77.312,   73.188,
  //                 -79.500, -22.047, -30.500,   33.375, -26.281
  VLOAD_16(v4, 0xd2f5, 0xcdd9, 0x52a3, 0xb5b2, 0x4eb7, 0xd51f, 0x53f4, 0xd2d3,
           0xd08c, 0x54d5, 0x5493, 0xd4f8, 0xcd83, 0xcfa0, 0x502c, 0xce92);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.x.f.v v8, v4, v0.t");
  //                           0,            -23,              0, 0, 0, -82, 0,
  //                           -55,              0,             77, 0, -80, 0,
  //                           -30,              0,            -26
  VCMP_U32(7, v8, 0x00000000, 0xffffffe9, 0x00000000, 0x00000000, 0x00000000,
           0xffffffae, 0x00000000, 0xffffffc9, 0x00000000, 0x0000004d,
           0x00000000, 0xffffffb0, 0x00000000, 0xffffffe2, 0x00000000,
           0xffffffe6);

  VSET(16, e32, m2);
  //                  -22345.104,     -55208.160,   60155.754, -4924.268,
  //                  -42337.285, -60609.004,   51795.328,   33876.547,
  //                  -99812.922,   59419.867, -78706.844,   72266.555,
  //                  -70664.008, -83501.727, -15981.749, -2004.535
  VLOAD_32(v4, 0xc6ae9235, 0xc757a829, 0x476afbc1, 0xc599e225, 0xc7256149,
           0xc76cc101, 0x474a5354, 0x4704548c, 0xc7c2f276, 0x47681bde,
           0xc799b96c, 0x478d2547, 0xc78a0401, 0xc7a316dd, 0xc679b6ff,
           0xc4fa9120);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.x.f.v v8, v4, v0.t");
  //                                   0,                 -55208, 0, -4924, 0,
  //                                   -60609,                      0, 33877, 0,
  //                                   59420,                      0, 72267, 0,
  //                                   -83502,                      0, -2005
  VCMP_U64(8, v8, 0x0000000000000000, 0xffffffffffff2858, 0x0000000000000000,
           0xffffffffffffecc4, 0x0000000000000000, 0xffffffffffff133f,
           0x0000000000000000, 0x0000000000008455, 0x0000000000000000,
           0x000000000000e81c, 0x0000000000000000, 0x0000000000011a4b,
           0x0000000000000000, 0xfffffffffffeb9d2, 0x0000000000000000,
           0xfffffffffffff82b);
};

/////////////////////
// vfwcvt.rtz.xu.f //
/////////////////////

// Simple random test with similar values
void TEST_CASE5(void) {
  VSET(16, e16, m2);
  //                26304.000, -31056.000,   6932.000,   63168.000, -10920.000,
  //                -38528.000,   inf, -inf, -1313.000,   52736.000,   inf,
  //                -inf, -61024.000, -inf, -5672.000,   53824.000
  VLOAD_16(v4, 0x766c, 0xf795, 0x6ec5, 0x7bb6, 0xf155, 0xf8b4, 0x7c00, 0xfc00,
           0xe521, 0x7a70, 0x7c00, 0xfc00, 0xfb73, 0xfc00, 0xed8a, 0x7a92);
  asm volatile("vfwcvt.rtz.xu.f.v v8, v4");
  //                       26304,              0,           6932, 63168, 0, 0,
  //                       0,              0,              0,          52736, 0,
  //                       0,              0,              0,              0,
  //                       53824
  VCMP_U32(9, v8, 0x000066c0, 0x00000000, 0x00001b14, 0x0000f6c0, 0x00000000,
           0x00000000, 0xffffffff, 0x00000000, 0x00000000, 0x0000ce00,
           0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
           0x0000d240);

  VSET(16, e32, m2);
  //           -139027333120.000,   783549530112.000,   166903955456.000,
  //           -848099999744.000, -671032279040.000, -402660294656.000,
  //           -259808854016.000,   248555126784.000,   712853684224.000,
  //           -492155797504.000, -448682098688.000, -916605566976.000,
  //           67602378752.000,   519669350400.000,   569111478272.000,
  //           -920229773312.000
  VLOAD_32(v4, 0xd2017ab3, 0x53366f31, 0x521b7101, 0xd34576b3, 0xd31c3ca4,
           0xd2bb80d9, 0xd271f742, 0x52677c29, 0x5325f964, 0xd2e52d8b,
           0xd2d0ef13, 0xd35569f3, 0x517bd6a7, 0x52f1fd6a, 0x530481b0,
           0xd35641f8);
  asm volatile("vfwcvt.rtz.xu.f.v v8, v4");
  //                                   0,           783549530112, 166903955456,
  //                                   0,                      0, 0, 0,
  //                                   248555126784,           712853684224, 0,
  //                                   0,                      0, 67602378752,
  //                                   519669350400,           569111478272, 0
  VCMP_U64(10, v8, 0x0000000000000000, 0x000000b66f310000, 0x00000026dc404000,
           0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
           0x0000000000000000, 0x00000039df0a4000, 0x000000a5f9640000,
           0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
           0x0000000fbd6a7000, 0x00000078feb50000, 0x0000008481b00000,
           0x0000000000000000);
};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE6(void) {
  VSET(16, e16, m2);
  //                26304.000, -31056.000,   6932.000,   63168.000, -10920.000,
  //                -38528.000,   inf, -inf, -1313.000,   52736.000,   inf,
  //                -inf, -61024.000, -inf, -5672.000,   53824.000
  VLOAD_16(v4, 0x766c, 0xf795, 0x6ec5, 0x7bb6, 0xf155, 0xf8b4, 0x7c00, 0xfc00,
           0xe521, 0x7a70, 0x7c00, 0xfc00, 0xfb73, 0xfc00, 0xed8a, 0x7a92);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.rtz.xu.f.v v8, v4, v0.t");
  //                           0,              0,              0, 63168, 0, 0,
  //                           0,              0,              0, 52736, 0, 0,
  //                           0,              0,              0,          53824
  VCMP_U32(11, v8, 0x00000000, 0x00000000, 0x00000000, 0x0000f6c0, 0x00000000,
           0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000ce00,
           0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
           0x0000d240);

  VSET(16, e32, m2);
  //              -139027333120.000,   783549530112.000,   166903955456.000,
  //              -848099999744.000, -671032279040.000, -402660294656.000,
  //              -259808854016.000,   248555126784.000,   712853684224.000,
  //              -492155797504.000, -448682098688.000, -916605566976.000,
  //              67602378752.000,   519669350400.000,   569111478272.000,
  //              -920229773312.000
  VLOAD_32(v4, 0xd2017ab3, 0x53366f31, 0x521b7101, 0xd34576b3, 0xd31c3ca4,
           0xd2bb80d9, 0xd271f742, 0x52677c29, 0x5325f964, 0xd2e52d8b,
           0xd2d0ef13, 0xd35569f3, 0x517bd6a7, 0x52f1fd6a, 0x530481b0,
           0xd35641f8);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.rtz.xu.f.v v8, v4, v0.t");
  //                                   0,           783549530112, 0, 0, 0, 0, 0,
  //                                   248555126784,                      0, 0,
  //                                   0,                      0, 0,
  //                                   519669350400,                      0, 0
  VCMP_U64(12, v8, 0x0000000000000000, 0x000000b66f310000, 0x0000000000000000,
           0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
           0x0000000000000000, 0x00000039df0a4000, 0x0000000000000000,
           0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
           0x0000000000000000, 0x00000078feb50000, 0x0000000000000000,
           0x0000000000000000);
};

////////////////////
// vfwcvt.rtz.x.f //
////////////////////

// Simple random test with similar values
void TEST_CASE7(void) {
  VSET(16, e16, m2);
  //                5.844,   36.219, -86.250,   20.406, -45.688,   13.961,
  //                -96.562,   81.000, -32.594,   51.281,   80.750,
  //                -17.750,   14.516,   58.000,   69.938, -94.688
  VLOAD_16(v4, 0x45d8, 0x5087, 0xd564, 0x4d1a, 0xd1b6, 0x4afb, 0xd609, 0x5510,
           0xd013, 0x5269, 0x550c, 0xcc70, 0x4b42, 0x5340, 0x545f, 0xd5eb);
  asm volatile("vfwcvt.rtz.x.f.v v8, v4");
  //                           5,             36,            -86, 20, -45, 13,
  //                           -96,             81,            -32, 51, 80, -17,
  //                           14,             58,             69, -94
  VCMP_U32(13, v8, 0x00000005, 0x00000024, 0xffffffaa, 0x00000014, 0xffffffd3,
           0x0000000d, 0xffffffa0, 0x00000051, 0xffffffe0, 0x00000033,
           0x00000050, 0xffffffef, 0x0000000e, 0x0000003a, 0x00000045,
           0xffffffa2);

  VSET(16, e32, m2);
  //                2116.345,  -810274979840.000, -5833.340, -6088.383,
  //                -9260.508, -2389.850,   9361.639,   5574.592, -6825.026,
  //                2473.934, -6756.971, -7155.075,   2251.162, -2899.548,
  //                -3184.759, -1954.714
  VLOAD_32(v4, 0x45044584, 0xd33ca827, 0xc5b64ab9, 0xc5be4311, 0xc610b208,
           0xc5155d98, 0x4612468e, 0x45ae34bd, 0xc5d54835, 0x451a9ef1,
           0xc5d327c5, 0xc5df9899, 0x450cb297, 0xc53538c6, 0xc5470c23,
           0xc4f456dc);
  asm volatile("vfwcvt.rtz.x.f.v v8, v4");
  //                                2116,         -810274979840, -5833, -6088,
  //                                -9260,                  -2389, 9361, 5574,
  //                                -6825,                   2473, -6756, -7155,
  //                                2251,                  -2899, -3184, -1954
  VCMP_U64(14, v8, 0x0000000000000844, 0xffffff4357d90000, 0xffffffffffffe937,
           0xffffffffffffe838, 0xffffffffffffdbd4, 0xfffffffffffff6ab,
           0x0000000000002491, 0x00000000000015c6, 0xffffffffffffe557,
           0x00000000000009a9, 0xffffffffffffe59c, 0xffffffffffffe40d,
           0x00000000000008cb, 0xfffffffffffff4ad, 0xfffffffffffff390,
           0xfffffffffffff85e);
};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE8(void) {
  VSET(16, e16, m2);
  //                5.844,   36.219, -86.250,   20.406, -45.688,   13.961,
  //                -96.562,   81.000, -32.594,   51.281,   80.750,
  //                -17.750,   14.516,   58.000,   69.938, -94.688
  VLOAD_16(v4, 0x45d8, 0x5087, 0xd564, 0x4d1a, 0xd1b6, 0x4afb, 0xd609, 0x5510,
           0xd013, 0x5269, 0x550c, 0xcc70, 0x4b42, 0x5340, 0x545f, 0xd5eb);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.rtz.x.f.v v8, v4, v0.t");
  //                           0,             36,              0, 20, 0, 13, 0,
  //                           81,              0,             51, 0, -17, 0,
  //                           58,              0,            -94
  VCMP_U32(15, v8, 0x00000000, 0x00000024, 0x00000000, 0x00000014, 0x00000000,
           0x0000000d, 0x00000000, 0x00000051, 0x00000000, 0x00000033,
           0x00000000, 0xffffffef, 0x00000000, 0x0000003a, 0x00000000,
           0xffffffa2);

  VSET(16, e32, m2);
  //                2116.345, -6652.860, -5833.340, -6088.383, -9260.508,
  //                -2389.850,   9361.639,   5574.592, -6825.026,   2473.934,
  //                -6756.971, -7155.075,   2251.162, -2899.548, -3184.759,
  //                -1954.714
  VLOAD_32(v4, 0x45044584, 0xc5cfe6e1, 0xc5b64ab9, 0xc5be4311, 0xc610b208,
           0xc5155d98, 0x4612468e, 0x45ae34bd, 0xc5d54835, 0x451a9ef1,
           0xc5d327c5, 0xc5df9899, 0x450cb297, 0xc53538c6, 0xc5470c23,
           0xc4f456dc);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.rtz.x.f.v v8, v4, v0.t");
  //                                   0,                  -6652, 0, -6088, 0,
  //                                   -2389,                      0, 5574, 0,
  //                                   2473,                      0, -7155, 0,
  //                                   -2899,                      0, -1954
  VCMP_U64(16, v8, 0x0000000000000000, 0xffffffffffffe604, 0x0000000000000000,
           0xffffffffffffe838, 0x0000000000000000, 0xfffffffffffff6ab,
           0x0000000000000000, 0x00000000000015c6, 0x0000000000000000,
           0x00000000000009a9, 0x0000000000000000, 0xffffffffffffe40d,
           0x0000000000000000, 0xfffffffffffff4ad, 0x0000000000000000,
           0xfffffffffffff85e);
};

/////////////////
// vfwcvt.f.xu //
/////////////////

// Simple random test with similar values
void TEST_CASE9(void) {
  VSET(16, e16, m2);
  //                    64656,       64687,       64823,         970, 543,
  //                    65038,       65122,         966,         180, 389, 337,
  //                    341,       65240,          51,       64922,       64676
  VLOAD_16(v4, 0xfc90, 0xfcaf, 0xfd37, 0x03ca, 0x021f, 0xfe0e, 0xfe62, 0x03c6,
           0x00b4, 0x0185, 0x0151, 0x0155, 0xfed8, 0x0033, 0xfd9a, 0xfca4);
  asm volatile("vfwcvt.f.xu.v v8, v4");
  //                   64656.000,      64687.000,      64823.000,      970.000,
  //                   543.000,      65038.000,      65122.000,      966.000,
  //                   180.000,      389.000,      337.000,      341.000,
  //                   65240.000,      51.000,      64922.000,      64676.000
  VCMP_U32(17, v8, 0x477c9000, 0x477caf00, 0x477d3700, 0x44728000, 0x4407c000,
           0x477e0e00, 0x477e6200, 0x44718000, 0x43340000, 0x43c28000,
           0x43a88000, 0x43aa8000, 0x477ed800, 0x424c0000, 0x477d9a00,
           0x477ca400);

  VSET(16, e32, m2);
  //                            97144,          4294936082,               42555,
  //                            4294893205,               55337, 4294948570,
  //                            4294931792,          4294924170, 4294912208,
  //                            4294947132,          4294903099, 4294944521,
  //                            4294923920,          4294889958, 31133, 30359
  VLOAD_32(v4, 0x00017b78, 0xffff8612, 0x0000a63b, 0xfffede95, 0x0000d829,
           0xffffb6da, 0xffff7550, 0xffff578a, 0xffff28d0, 0xffffb13c,
           0xffff053b, 0xffffa709, 0xffff5690, 0xfffed1e6, 0x0000799d,
           0x00007697);
  asm volatile("vfwcvt.f.xu.v v8, v4");
  //                   97144.000,      4294936082.000,      42555.000,
  //                   4294893205.000,      55337.000,      4294948570.000,
  //                   4294931792.000,      4294924170.000,      4294912208.000,
  //                   4294947132.000,      4294903099.000,      4294944521.000,
  //                   4294923920.000,      4294889958.000,      31133.000,
  //                   30359.000
  VCMP_U64(18, v8, 0x40f7b78000000000, 0x41effff0c2400000, 0x40e4c76000000000,
           0x41efffdbd2a00000, 0x40eb052000000000, 0x41effff6db400000,
           0x41efffeeaa000000, 0x41efffeaf1400000, 0x41efffe51a000000,
           0x41effff627800000, 0x41efffe0a7600000, 0x41effff4e1200000,
           0x41efffead2000000, 0x41efffda3cc00000, 0x40de674000000000,
           0x40dda5c000000000);
};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE10(void) {
  VSET(16, e16, m2);
  //                    64656,       64687,       64823,         970, 543,
  //                    65038,       65122,         966,         180, 389, 337,
  //                    341,       65240,          51,       64922,       64676
  VLOAD_16(v4, 0xfc90, 0xfcaf, 0xfd37, 0x03ca, 0x021f, 0xfe0e, 0xfe62, 0x03c6,
           0x00b4, 0x0185, 0x0151, 0x0155, 0xfed8, 0x0033, 0xfd9a, 0xfca4);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.f.xu.v v8, v4, v0.t");
  //                   0.000,      64687.000,      0.000,      970.000, 0.000,
  //                   65038.000,      0.000,      966.000,      0.000, 389.000,
  //                   0.000,      341.000,      0.000,      51.000,      0.000,
  //                   64676.000
  VCMP_U32(19, v8, 0x0, 0x477caf00, 0x0, 0x44728000, 0x0, 0x477e0e00, 0x0,
           0x44718000, 0x0, 0x43c28000, 0x0, 0x43aa8000, 0x0, 0x424c0000, 0x0,
           0x477ca400);

  VSET(16, e32, m2);
  //                            97144,          4294936082,               42555,
  //                            4294893205,               55337, 4294948570,
  //                            4294931792,          4294924170, 4294912208,
  //                            4294947132,          4294903099, 4294944521,
  //                            4294923920,          4294889958, 31133, 30359
  VLOAD_32(v4, 0x00017b78, 0xffff8612, 0x0000a63b, 0xfffede95, 0x0000d829,
           0xffffb6da, 0xffff7550, 0xffff578a, 0xffff28d0, 0xffffb13c,
           0xffff053b, 0xffffa709, 0xffff5690, 0xfffed1e6, 0x0000799d,
           0x00007697);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.f.xu.v v8, v4, v0.t");
  //                   0.000,      4294936082.000,      0.000, 4294893205.000,
  //                   0.000,      4294948570.000,      0.000, 4294924170.000,
  //                   0.000,      4294947132.000,      0.000, 4294944521.000,
  //                   0.000,      4294889958.000,      0.000,      30359.000
  VCMP_U64(20, v8, 0x0, 0x41effff0c2400000, 0x0, 0x41efffdbd2a00000, 0x0,
           0x41effff6db400000, 0x0, 0x41efffeaf1400000, 0x0, 0x41effff627800000,
           0x0, 0x41effff4e1200000, 0x0, 0x41efffda3cc00000, 0x0,
           0x40dda5c000000000);
};

////////////////
// vfwcvt.f.x //
////////////////

// Simple random test with similar values
void TEST_CASE11(void) {
  VSET(16, e16, m2);
  //                     -263,        -943,         111,        -140, -792,
  //                     -320,        -384,         250,        -308, 578, -830,
  //                     -865,         908,         264,          93, 833
  VLOAD_16(v4, 0xfef9, 0xfc51, 0x006f, 0xff74, 0xfce8, 0xfec0, 0xfe80, 0x00fa,
           0xfecc, 0x0242, 0xfcc2, 0xfc9f, 0x038c, 0x0108, 0x005d, 0x0341);
  asm volatile("vfwcvt.f.x.v v8, v4");
  //                  -263.000,     -943.000,      111.000,     -140.000,
  //                  -792.000,     -320.000,     -384.000,      250.000,
  //                  -308.000,      578.000,     -830.000,     -865.000,
  //                  908.000,      264.000,      93.000,      833.000
  VCMP_U32(21, v8, 0xc3838000, 0xc46bc000, 0x42de0000, 0xc30c0000, 0xc4460000,
           0xc3a00000, 0xc3c00000, 0x437a0000, 0xc39a0000, 0x44108000,
           0xc44f8000, 0xc4584000, 0x44630000, 0x43840000, 0x42ba0000,
           0x44504000);

  VSET(16, e32, m2);
  //                           -85277,               33391,               84804,
  //                           -45155,              -68903,               19141,
  //                           -10026,               87992,               13128,
  //                           95737,              -70832,               43360,
  //                           32471,                  51,               50027,
  //                           -57346
  VLOAD_32(v4, 0xfffeb2e3, 0x0000826f, 0x00014b44, 0xffff4f9d, 0xfffef2d9,
           0x00004ac5, 0xffffd8d6, 0x000157b8, 0x00003348, 0x000175f9,
           0xfffeeb50, 0x0000a960, 0x00007ed7, 0x00000033, 0x0000c36b,
           0xffff1ffe);
  asm volatile("vfwcvt.f.x.v v8, v4");
  //                  -85277.000,      33391.000,      84804.000, -45155.000,
  //                  -68903.000,      19141.000,     -10026.000, 87992.000,
  //                  13128.000,      95737.000,     -70832.000,      43360.000,
  //                  32471.000,      51.000,      50027.000,     -57346.000
  VCMP_U64(22, v8, 0xc0f4d1d000000000, 0x40e04de000000000, 0x40f4b44000000000,
           0xc0e60c6000000000, 0xc0f0d27000000000, 0x40d2b14000000000,
           0xc0c3950000000000, 0x40f57b8000000000, 0x40c9a40000000000,
           0x40f75f9000000000, 0xc0f14b0000000000, 0x40e52c0000000000,
           0x40dfb5c000000000, 0x4049800000000000, 0x40e86d6000000000,
           0xc0ec004000000000);
};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE12(void) {
  VSET(16, e16, m2);
  //                     -263,        -943,         111,        -140, -792,
  //                     -320,        -384,         250,        -308, 578, -830,
  //                     -865,         908,         264,          93, 833
  VLOAD_16(v4, 0xfef9, 0xfc51, 0x006f, 0xff74, 0xfce8, 0xfec0, 0xfe80, 0x00fa,
           0xfecc, 0x0242, 0xfcc2, 0xfc9f, 0x038c, 0x0108, 0x005d, 0x0341);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.f.x.v v8, v4, v0.t");
  //                   0.000,     -943.000,      0.000,     -140.000, 0.000,
  //                   -320.000,      0.000,      250.000,      0.000, 578.000,
  //                   0.000,     -865.000,      0.000,      264.000, 0.000,
  //                   833.000
  VCMP_U32(23, v8, 0x0, 0xc46bc000, 0x0, 0xc30c0000, 0x0, 0xc3a00000, 0x0,
           0x437a0000, 0x0, 0x44108000, 0x0, 0xc4584000, 0x0, 0x43840000, 0x0,
           0x44504000);

  VSET(16, e32, m2);
  //                           -85277,               33391,               84804,
  //                           -45155,              -68903,               19141,
  //                           -10026,               87992,               13128,
  //                           95737,              -70832,               43360,
  //                           32471,                  51,               50027,
  //                           -57346
  VLOAD_32(v4, 0xfffeb2e3, 0x0000826f, 0x00014b44, 0xffff4f9d, 0xfffef2d9,
           0x00004ac5, 0xffffd8d6, 0x000157b8, 0x00003348, 0x000175f9,
           0xfffeeb50, 0x0000a960, 0x00007ed7, 0x00000033, 0x0000c36b,
           0xffff1ffe);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.f.x.v v8, v4, v0.t");
  //                   0.000,      33391.000,      0.000,     -45155.000, 0.000,
  //                   19141.000,      0.000,      87992.000,      0.000,
  //                   95737.000,      0.000,      43360.000,
  //                   0.000,      51.000,      0.000,     -57346.000
  VCMP_U64(24, v8, 0x0, 0x40e04de000000000, 0x0, 0xc0e60c6000000000, 0x0,
           0x40d2b14000000000, 0x0, 0x40f57b8000000000, 0x0, 0x40f75f9000000000,
           0x0, 0x40e52c0000000000, 0x0, 0x4049800000000000, 0x0,
           0xc0ec004000000000);
};

////////////////
// vfwcvt.f.f //
////////////////

// Simple random test with similar values
void TEST_CASE13(void) {
  VSET(16, e16, m2);
  //                83.312, -83.188,   62.469,   94.812,   10.797, -13.070,
  //                -9.039,   54.250, -92.188,   63.688, -32.875, -81.688,
  //                -62.219, -78.250, -29.703, -1.137
  VLOAD_16(v4, 0x5535, 0xd533, 0x53cf, 0x55ed, 0x4966, 0xca89, 0xc885, 0x52c8,
           0xd5c3, 0x53f6, 0xd01c, 0xd51b, 0xd3c7, 0xd4e4, 0xcf6d, 0xbc8c);
  asm volatile("vfwcvt.f.f.v v8, v4");
  //                83.312, -83.188,   62.469,   94.812,   10.797, -13.070,
  //                -9.039,   54.250, -92.188,   63.688, -32.875, -81.688,
  //                -62.219, -78.250, -29.703, -1.137
  VCMP_U32(25, v8, 0x42a6a000, 0xc2a66000, 0x4279e000, 0x42bda000, 0x412cc000,
           0xc1512000, 0xc110a000, 0x42590000, 0xc2b86000, 0x427ec000,
           0xc2038000, 0xc2a36000, 0xc278e000, 0xc29c8000, 0xc1eda000,
           0xbf918000);

  VSET(16, e32, m2);
  //              -69280.273, -24625.789,   58970.254,   57986.516,   34031.016,
  //              61977.340, -84548.211,   89658.250,   4958.967, -73911.508,
  //              -83526.188, -59814.750,   71544.742,   93401.383,   79319.078,
  //              4639.214
  VLOAD_32(v4, 0xc7875023, 0xc6c06394, 0x47665a41, 0x47628284, 0x4704ef04,
           0x47721957, 0xc7a5221b, 0x47af1d20, 0x459af7bc, 0xc7905bc1,
           0xc7a32318, 0xc769a6c0, 0x478bbc5f, 0x47b66cb1, 0x479aeb8a,
           0x4590f9b7);
  asm volatile("vfwcvt.f.f.v v8, v4");
  //              -69280.273, -24625.789,   58970.254,   57986.516,   34031.016,
  //              61977.340, -84548.211,   89658.250,   4958.967, -73911.508,
  //              -83526.188, -59814.750,   71544.742,   93401.383,   79319.078,
  //              4639.214
  VCMP_U64(26, v8, 0xc0f0ea0460000000, 0xc0d80c7280000000, 0x40eccb4820000000,
           0x40ec505080000000, 0x40e09de080000000, 0x40ee432ae0000000,
           0xc0f4a44360000000, 0x40f5e3a400000000, 0x40b35ef780000000,
           0xc0f20b7820000000, 0xc0f4646300000000, 0xc0ed34d800000000,
           0x40f1778be0000000, 0x40f6cd9620000000, 0x40f35d7140000000,
           0x40b21f36e0000000);
};

// Simple random test with similar values (masked)
// The numbers are the same of TEST_CASE1
void TEST_CASE14(void) {
  VSET(16, e16, m2);
  //                83.312, -83.188,   62.469,   94.812,   10.797, -13.070,
  //                -9.039,   54.250, -92.188,   63.688, -32.875, -81.688,
  //                -62.219, -78.250, -29.703, -1.137
  VLOAD_16(v4, 0x5535, 0xd533, 0x53cf, 0x55ed, 0x4966, 0xca89, 0xc885, 0x52c8,
           0xd5c3, 0x53f6, 0xd01c, 0xd51b, 0xd3c7, 0xd4e4, 0xcf6d, 0xbc8c);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.f.f.v v8, v4, v0.t");
  //                0.000, -83.188,   0.000,   94.812,   0.000, -13.070, 0.000,
  //                54.250,   0.000,   63.688,   0.000, -81.688,   0.000,
  //                -78.250,   0.000, -1.137
  VCMP_U32(27, v8, 0x0, 0xc2a66000, 0x0, 0x42bda000, 0x0, 0xc1512000, 0x0,
           0x42590000, 0x0, 0x427ec000, 0x0, 0xc2a36000, 0x0, 0xc29c8000, 0x0,
           0xbf918000);

  VSET(16, e32, m2);
  //              -69280.273, -24625.789,   58970.254,   57986.516,   34031.016,
  //              61977.340, -84548.211,   89658.250,   4958.967, -73911.508,
  //              -83526.188, -59814.750,   71544.742,   93401.383,   79319.078,
  //              4639.214
  VLOAD_32(v4, 0xc7875023, 0xc6c06394, 0x47665a41, 0x47628284, 0x4704ef04,
           0x47721957, 0xc7a5221b, 0x47af1d20, 0x459af7bc, 0xc7905bc1,
           0xc7a32318, 0xc769a6c0, 0x478bbc5f, 0x47b66cb1, 0x479aeb8a,
           0x4590f9b7);
  VLOAD_8(v0, 0xAA, 0xAA);
  VCLEAR(v8);
  asm volatile("vfwcvt.f.f.v v8, v4, v0.t");
  //                0.000, -24625.789,   0.000,   57986.516,   0.000, 61977.340,
  //                0.000,   89658.250,   0.000, -73911.508,   0.000,
  //                -59814.750,   0.000,   93401.383,   0.000,   4639.214
  VCMP_U64(28, v8, 0x0, 0xc0d80c7280000000, 0x0, 0x40ec505080000000, 0x0,
           0x40ee432ae0000000, 0x0, 0x40f5e3a400000000, 0x0, 0xc0f20b7820000000,
           0x0, 0xc0ed34d800000000, 0x0, 0x40f6cd9620000000, 0x0,
           0x40b21f36e0000000);
};

int main(void) {
  enable_vec();
  enable_fp();

  TEST_CASE1();
  TEST_CASE2();

  TEST_CASE3();
  TEST_CASE4();

  TEST_CASE5();
  TEST_CASE6();

  TEST_CASE7();
  TEST_CASE8();

  TEST_CASE9();
  TEST_CASE10();

  TEST_CASE11();
  TEST_CASE12();

  TEST_CASE13();
  TEST_CASE14();

  EXIT_CHECK();
}