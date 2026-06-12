// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: Diyou Shen

// Chaining correctness tests for strided and indexed EW_8/EW_16 loads.
//
// These instruction types cause the VLSU to issue partial VRF writes (wbe !=
// all-ones) before the full VRF word is coherent. A dependent vector
// instruction chained on the load destination must not read stale data.
//
// Each test case performs:
//   1. A strided or indexed load into v1 (partial-wbe writes).
//   2. An immediately chained vadd reading v1.
//   3. VCMP on the vadd result.
//
// Without the VLSU coalescing buffer, the controller enables chaining after
// the first partial write, so vadd reads a partially-written VRF word and
// produces wrong results.

#include "vector_macros.h"

// EW_8 strided load chained with vadd.vv
// stride=3 forces single-element commits; 8 elements span 2 VRF write cycles.
void TEST_CASE1(void) {
  volatile uint8_t INP1[] = {
    0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
    0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3,
    0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
    0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  uint64_t stride = 3;

  // stride=3: offsets 0,3,6,9,12,15,18,21
  // v1 = {0x9f, 0x20, 0x05, 0xaa, 0xc3, 0xd3, 0x19, 0x2e}
  VSET(8, e8, m1);
  VLOAD_8(v3, 1, 1, 1, 1, 1, 1, 1, 1);
  asm volatile("vlse8.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  asm volatile("vadd.vv v2, v1, v3");
  VCMP_U8(1, v2, 0xa0, 0x21, 0x06, 0xab, 0xc4, 0xd4, 0x1a, 0x2f);
}

// EW_16 strided load chained with vadd.vv
// stride=4 bytes (skips every other element); 8 elements span 2 VRF write cycles.
void TEST_CASE2(void) {
  volatile uint16_t INP1[] = {
    0x9fe4, 0x1920, 0x8f2e, 0x05e0, 0xf9aa, 0x71f0, 0xc394, 0xbbd3,
    0xa11a, 0x9384, 0xa716, 0x3840, 0x9999, 0x1348, 0xa9f3, 0x8cd1};
  uint64_t stride = 4;

  // stride=4 bytes: byte offsets 0,4,8,12,16,20,24,28
  // v1 = {0x9fe4, 0x8f2e, 0xf9aa, 0xc394, 0xa11a, 0xa716, 0x9999, 0xa9f3}
  VSET(8, e16, m1);
  VLOAD_16(v3, 1, 1, 1, 1, 1, 1, 1, 1);
  asm volatile("vlse16.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  asm volatile("vadd.vv  v2, v1, v3");
  VCMP_U16(2, v2, 0x9fe5, 0x8f2f, 0xf9ab, 0xc395, 0xa11b, 0xa717, 0x999a,
           0xa9f4);
}

// EW_8 indexed load (vloxei8.v) chained with vadd.vv
void TEST_CASE3(void) {
  volatile uint8_t INP1[] = {
    0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
    0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89};

  // byte offsets {1,2,3,4,5,7,8,9}
  // v1 = {0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae}
  VSET(8, e8, m1);
  VLOAD_8(v2, 1, 2, 3, 4, 5, 7, 8, 9);
  VLOAD_8(v3, 1, 1, 1, 1, 1, 1, 1, 1);
  asm volatile("vloxei8.v v1, (%0), v2" ::"r"(INP1));
  asm volatile("vadd.vv   v4, v1, v3");
  VCMP_U8(3, v4, 0xd4, 0x41, 0xd2, 0x85, 0x49, 0x89, 0x89, 0xaf);
}

// EW_16 indexed load (vloxei16.v) chained with vadd.vv
void TEST_CASE4(void) {
  volatile uint16_t INP1[] = {
    0x05e0, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388,
    0x8188, 0x11ae, 0x5808, 0x4891, 0x4902, 0x8759, 0x1111, 0x1989};

  // byte offsets {2,4,6,8,10,12,14,16}
  // v1 = {0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x3489, 0x9388, 0x8188}
  VSET(8, e16, m1);
  VLOAD_16(v2, 2, 4, 6, 8, 10, 12, 14, 16);
  VLOAD_16(v3, 1, 1, 1, 1, 1, 1, 1, 1);
  asm volatile("vloxei16.v v1, (%0), v2" ::"r"(INP1));
  asm volatile("vadd.vv    v4, v1, v3");
  VCMP_U16(4, v4, 0xbbd4, 0x3841, 0x8cd2, 0x9385, 0x7549, 0x348a, 0x9389,
           0x8189);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();

  EXIT_CHECK();
}
