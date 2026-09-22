// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>
//         Riccardo Giunti, Fondazione Chips-IT

#include "vector_macros.h"

// Mixed mask, e32
void TEST_CASE1() {
  VSET(14, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0x39AD); // elements 0,2,3,5,7,8,11,12,13
  VSET(14, e32, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U32(1, v8, 1,3,4,6,8,9,12,13,14,100,101,102,103,104);
}

// All-zero mask: nothing is written, vd must be untouched, and the
// instruction must still retire (FLUSH with wbe == 0)
void TEST_CASE2() {
  VSET(14, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0x0);
  VSET(14, e32, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U32(2, v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
}

// All-ones mask, total is an exact multiple of a VRF word (16 x 4 B = 64 B):
// every source word emits, the last emit happens during the last read and
// there must be NO flush
void TEST_CASE3() {
  VSET(16, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0xFFFF);
  VSET(16, e32, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U32(3, v8, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
}

// Non-trivial mask whose total is an exact multiple (8 x 4 B = 32 B)
void TEST_CASE4() {
  VSET(14, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0x326E); // elements 1,2,3,5,6,9,12,13
  VSET(14, e32, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U32(4, v8, 2,3,4,6,7,10,13,14,99,100,101,102,103,104);
}

// Source words with no active element in the middle, single active element in
// the last (partial) source word at a non-zero byte offset. The only write is the final flush
void TEST_CASE5() {
  VSET(14, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0x2001); // elements 0 and 13
  VSET(14, e32, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U32(5, v8, 1,14,93,94,95,96,97,98,99,100,101,102,103,104);
}

// Mask bits beyond vl must be ignored.  vl = 10, mask has bits 10..15 set:
// the last source word (elements 8..11) is only half inside vl
void TEST_CASE6() {
  VSET(10, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0xFE01); // elements 0, 9 (in vl) + 10..15 (out of vl)
  VSET(10, e32, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U32(6, v8, 1,10,93,94,95,96,97,98,99,100);
}

// e16 (8 elements per source word)
void TEST_CASE7() {
  VSET(20, e16, m8);
  VLOAD_16(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20);
  VLOAD_16(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0x0AFF99); // elements 0,3,4,7, 8..15, 17,19
  VSET(20, e16, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U16(7, v8, 1,4,5,8,9,10,11,12,13,14,15,16,18,20,105,106,107,108,109,110);
}

// e8 (16 elements per source word), even elements only

void TEST_CASE8() {
  VSET(40, e8, m8);
  VLOAD_8(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
               21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40);
  VLOAD_8(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,
              111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130);
  VSET(2, e32, m1);
  VLOAD_32(v24, 0x55555555, 0x55);  // bits 0..39, even elements
  VSET(40, e8, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U8(8, v8, 1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33,35,37,39,
                 111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130);
}

// e8 (16 elements per source word), every element with i % 3 != 0; 64-bit mask
void TEST_CASE9() {
  VSET(64, e8, m8);
  VLOAD_8(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
               33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64);
  VLOAD_8(v8, 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
              144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
              160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
              176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191);
  VSET(2, e32, m1);
  VLOAD_32(v24, 0xB6DB6DB6, 0x6DB6DB6D);  // 0x6DB6DB6DB6DB6DB6
  VSET(64, e8, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U8(9, v8, 2,3,5,6,8,9,11,12,14,15,17,18,20,21,23,24,26,27,29,30,32,33,
                 35,36,38,39,41,42,44,45,47,48,50,51,53,54,56,57,59,60,62,63,
                 170,171,172,173,174,175,176,177,178,179,180,181,
                 182,183,184,185,186,187,188,189,190,191);
}

// Mask in v31: the mask word address is 31 << zero_fill_idx and needs the full VRF address width
void TEST_CASE10() {
  VSET(14, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e32, m1);
  VLOAD_32(v31, 0x39AD);
  VSET(14, e32, m8);
  asm volatile("vcompress.vm v8, v16, v31");
  VCMP_U32(10, v8, 1,3,4,6,8,9,12,13,14,100,101,102,103,104);
}

// Mask in v0: v0 is an ordinary operand for vcompress (vm = 1) and must not interfere with the v0.t masking path
void TEST_CASE11() {
  VSET(14, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e32, m1);
  VLOAD_32(v0, 0x39AD);
  VSET(14, e32, m8);
  asm volatile("vcompress.vm v8, v16, v0");
  VCMP_U32(11, v8, 1,3,4,6,8,9,12,13,14,100,101,102,103,104);
}

// Two vcompress back to back with different masks: catches state that is not
// reset between instructions (accumulator, prefix sum, total byte count)
void TEST_CASE12() {
  VSET(14, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VLOAD_32(v0, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0x39AD);
  VLOAD_32(v31, 0x2001);
  VSET(14, e32, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  asm volatile("vcompress.vm v0, v16, v31");
  VCMP_U32(12, v8, 1,3,4,6,8,9,12,13,14,100,101,102,103,104);
  VCMP_U32(12, v0, 1,14,93,94,95,96,97,98,99,100,101,102,103,104);
}

// vcompress immediately followed by a slide that consumes its result
void TEST_CASE13() {
  VSET(14, e32, m8);
  VLOAD_32(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_32(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e32, m1);
  VLOAD_32(v24, 0x39AD);
  VSET(14, e32, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VSET(12, e32, m8);
  asm volatile("vslidedown.vi v0, v8, 2");
  VCMP_U32(13, v0, 4,6,8,9,12,13,14,100,101,102,103,104);
}

// e8, vl = 136 > 128
void TEST_CASE14() {
  VSET(136, e8, m8);
  VLOAD_8(v16,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
                49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
                65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
                81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,
                97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,112,
               113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
               129,130,131,132,133,134,135,136);
  VLOAD_8(v8,  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
               255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
               255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
               255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
               255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
               255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
               255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
               255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
               255,255,255,255,255,255,255,255);
  VSET(5, e32, m1);
  VLOAD_32(v24, 0x00020001, 0x00000100, 0x00000000, 0x80000000, 0xFFFFFF89);                 
  VSET(136, e8, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U8(14, v8,   1, 18, 41,128,129,132,136,255,255,255,255,255,255,255,255,255,
                  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                  255,255,255,255,255,255,255,255);
}

#if ELEN == 64
// Mixed mask, e64
void TEST_CASE15() {
  VSET(14, e64, m8);
  VLOAD_64(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_64(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e64, m1);
  VLOAD_64(v24, 0x39ADULL);
  VSET(14, e64, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U64(15, v8, 1,3,4,6,8,9,12,13,14,100,101,102,103,104);
}

// Exact multiple (64 B), emit with exact fill, no flush
void TEST_CASE16() {
  VSET(14, e64, m8);
  VLOAD_64(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_64(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e64, m1);
  VLOAD_64(v24, 0x326EULL);
  VSET(14, e64, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U64(16, v8, 2,3,4,6,7,10,13,14,99,100,101,102,103,104);
}

// Empty source words, flush-only
void TEST_CASE17() {
  VSET(14, e64, m8);
  VLOAD_64(v16, 1,2,3,4,5,6,7,8,9,10,11,12,13,14);
  VLOAD_64(v8, 91,92,93,94,95,96,97,98,99,100,101,102,103,104);
  VSET(1, e64, m1);
  VLOAD_64(v24, 0x2001ULL);
  VSET(14, e64, m8);
  asm volatile("vcompress.vm v8, v16, v24");
  VCMP_U64(17, v8, 1,14,93,94,95,96,97,98,99,100,101,102,103,104);
}
#endif

int main(void) {
  INIT_CHECK();
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
#if ELEN == 64
  TEST_CASE15();
  TEST_CASE16();
  TEST_CASE17();
#endif

  EXIT_CHECK();
}