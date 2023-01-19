// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "vector_macros.h"

#define AXI_DWIDTH 128

static volatile uint8_t ALIGNED_I8[16] __attribute__((aligned(AXI_DWIDTH))) = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static volatile uint16_t ALIGNED_I16[16]
    __attribute__((aligned(AXI_DWIDTH))) = {
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

static volatile uint32_t ALIGNED_I32[16]
    __attribute__((aligned(AXI_DWIDTH))) = {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000};

static volatile uint64_t ALIGNED_I64[16]
    __attribute__((aligned(AXI_DWIDTH))) = {
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
        0x0000000000000000};

void TEST_CASE1(void) {
  VSET(16, e8, m4);
  VLOAD_8(v0, 0xe0, 0x84, 0x88, 0x02, 0x59, 0xae, 0x48, 0xd3, 0x40, 0x89, 0x08,
          0x11, 0x89, 0x91, 0x88, 0xd1);
  VLOAD_8(v4, 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3);
  asm volatile("vsuxei8.v v0, (%0), v4" ::"r"(ALIGNED_I8));
  VVCMP_U8(1, ALIGNED_I8, 0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88, 0x88,
           0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89);

  for (int i = 0; i < 16; ++i)
    ALIGNED_I8[i] = 0;

  VSET(16, e8, m4);
  VLOAD_8(v0, 0xe0, 0x84, 0x88, 0x02, 0x59, 0xae, 0x48, 0xd3, 0x40, 0x89, 0x08,
          0x11, 0x89, 0x91, 0x88, 0xd1);
  VLOAD_8(v4, 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3);
  asm volatile("vsoxei8.v v0, (%0), v4" ::"r"(ALIGNED_I8));
  VVCMP_U8(2, ALIGNED_I8, 0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88, 0x88,
           0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89);
}

void TEST_CASE2(void) {
  VSET(16, e16, m4);
  VLOAD_16(v0, 0x05e0, 0x9384, 0x8188, 0x4902, 0x8759, 0x11ae, 0x7548, 0xbbd3,
           0x3840, 0x3489, 0x5808, 0x1111, 0x1989, 0x4891, 0x9388, 0x8cd1);
  VLOAD_16(v4, 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3);
  asm volatile("vsuxei16.v v0, (%0), v4" ::"r"(ALIGNED_I16));
  VVCMP_U16(3, ALIGNED_I16, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548,
            0x3489, 0x9388, 0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759,
            0x1111, 0x1989);

  for (int i = 0; i < 16; ++i)
    ALIGNED_I16[i] = 0;

  VSET(16, e16, m4);
  VLOAD_16(v0, 0x05e0, 0x9384, 0x8188, 0x4902, 0x8759, 0x11ae, 0x7548, 0xbbd3,
           0x3840, 0x3489, 0x5808, 0x1111, 0x1989, 0x4891, 0x9388, 0x8cd1);
  VLOAD_16(v4, 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3);
  asm volatile("vsoxei16.v v0, (%0), v4" ::"r"(ALIGNED_I16));
  VVCMP_U16(4, ALIGNED_I16, 0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548,
            0x3489, 0x9388, 0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759,
            0x1111, 0x1989);
}

void TEST_CASE3(void) {
  VSET(16, e32, m4);
  VLOAD_32(v0, 0x9fe41920, 0x9fa831c7, 0x18747547, 0x90318509, 0x31897598,
           0x3eeeeeee, 0x38197598, 0xf9aa71f0, 0xa11a9384, 0x18931795,
           0x90139301, 0x83195999, 0x89139848, 0xab8b9148, 0x81937598,
           0x99991348);
  VLOAD_32(v4, 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3);
  asm volatile("vsuxei32.v v0, (%0), v4" ::"r"(ALIGNED_I32));
  VVCMP_U32(5, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);

  for (int i = 0; i < 16; ++i)
    ALIGNED_I32[i] = 0;

  VSET(16, e32, m4);
  VLOAD_32(v0, 0x9fe41920, 0x9fa831c7, 0x18747547, 0x90318509, 0x31897598,
           0x3eeeeeee, 0x38197598, 0xf9aa71f0, 0xa11a9384, 0x18931795,
           0x90139301, 0x83195999, 0x89139848, 0xab8b9148, 0x81937598,
           0x99991348);
  VLOAD_32(v4, 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3);
  asm volatile("vsoxei32.v v0, (%0), v4" ::"r"(ALIGNED_I32));
  VVCMP_U32(6, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}

/*void TEST_CASE4(void) {
#if ELEN == 64
  VSET(16, e64, m4);
  VLOAD_64(v0, 0x9fe419208f2e05e0, 0x9fa831c7a11a9384, 0x1874754791888188,
           0x9031850931584902, 0x3189759837598759, 0x3eeeeeeee33111ae,
           0x3819759853987548, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
           0x1893179501093489, 0x9013930148815808, 0x8319599991911111,
           0x8913984898951989, 0xab8b914891484891, 0x81937598aa819388,
           0x99991348a9f38cd1);
  VLOAD_64(v4, 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3);
  asm volatile("vsuxei64.v v0, (%0), v4" ::"r"(ALIGNED_I64));
  VVCMP_U64(7, ALIGNED_I64, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3,
            0xa11a9384a7163840, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
            0x3819759853987548, 0x1893179501093489, 0x81937598aa819388,
            0x1874754791888188, 0x3eeeeeeee33111ae, 0x9013930148815808,
            0xab8b914891484891, 0x9031850931584902, 0x3189759837598759,
            0x8319599991911111, 0x8913984898951989);

  for (int i = 0; i < 16; ++i)
    ALIGNED_I64[i] = 0;

  VSET(16, e64, m4);
  VLOAD_64(v0, 0x9fe419208f2e05e0, 0x9fa831c7a11a9384, 0x1874754791888188,
           0x9031850931584902, 0x3189759837598759, 0x3eeeeeeee33111ae,
           0x3819759853987548, 0xf9aa71f0c394bbd3, 0xa11a9384a7163840,
           0x1893179501093489, 0x9013930148815808, 0x8319599991911111,
           0x8913984898951989, 0xab8b914891484891, 0x81937598aa819388,
           0x99991348a9f38cd1);
  VLOAD_64(v4, 0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3);
  asm volatile("vsoxei64.v v0, (%0), v4" ::"r"(ALIGNED_I64));
  VVCMP_U64(8, ALIGNED_I64, 0x9fe419208f2e05e0, 0xf9aa71f0c394bbd3,
            0xa11a9384a7163840, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
            0x3819759853987548, 0x1893179501093489, 0x81937598aa819388,
            0x1874754791888188, 0x3eeeeeeee33111ae, 0x9013930148815808,
            0xab8b914891484891, 0x9031850931584902, 0x3189759837598759,
            0x8319599991911111, 0x8913984898951989);
#endif
}*/

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  // TEST_CASE4();

  EXIT_CHECK();
}
