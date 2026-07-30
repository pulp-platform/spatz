// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: Riccardo Giunti - Fondazione CHIPS-IT

#include "vector_macros.h"

#define INIT 98

// EEW dest = EEW idx  (unmasked)
void TEST_CASE1(void) {
  { // eew_dest = eew_idx = e8
    volatile uint8_t BUF[] = {
      0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae,
      0x91, 0x02, 0x59, 0x89, 0x11, 0x22, 0x33, 0x44};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(12, e8, m4);
    VLOAD_8(v8, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15);
    asm volatile("vloxei8.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U8(1, v4, 0x40, 0xd1, 0x84, 0x48, 0x88, 0xae, 0x91, 0x02, 0x89, 0x11, 0x22, 0x44);
  }
  { // eew_dest = eew_idx = e16
    volatile uint16_t BUF[] = {
      0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae,
      0x4891, 0x4902, 0x8759, 0x1989, 0x1111, 0x2222, 0x3333, 0x4444};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(12, e16, m4);
    VLOAD_16(v8, 2, 4, 6, 8, 10, 14, 16, 18, 22, 24, 26, 30);
    asm volatile("vloxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U16(2, v4, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x11ae, 0x4891, 0x4902, 0x1989, 0x1111, 0x2222, 0x4444);
  }
  { // eew_dest = eew_idx = e32
    volatile uint32_t BUF[] = {
      0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
      0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee,
      0x11111111, 0x22222222, 0x33333333, 0x44444444,
      0x55555555, 0x66666666, 0x77777777, 0x88888888};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e32, m4);
    VLOAD_32(v8, 4, 8, 12, 16, 20, 28, 32, 36);
    asm volatile("vloxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U32(3, v4, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598, 0x81937598, 0x3eeeeeee, 0x11111111, 0x22222222);
  }
}

// EEW dest = EEW idx  (masked)
void TEST_CASE2(void) {
  { // eew_dest = eew_idx = e8
    volatile uint8_t BUF[] = {
      0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae,
      0x91, 0x02, 0x59, 0x89, 0x11, 0x22, 0x33, 0x44};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(12, e8, m4);
    VLOAD_8(v0, 0xaa, 0x0a);
    VLOAD_8(v8, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15);
    asm volatile("vloxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U8(4, v4, 0x00, 0xd1, 0x00, 0x48, 0x00, 0xae, 0x00, 0x02, 0x00, 0x11, 0x00, 0x44);
  }
  { // eew_dest = eew_idx = e16
    volatile uint16_t BUF[] = {
      0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae,
      0x4891, 0x4902, 0x8759, 0x1989, 0x1111, 0x2222, 0x3333, 0x4444};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(12, e16, m4);
    VLOAD_8(v0, 0xaa, 0x0a);
    VLOAD_16(v8, 2, 4, 6, 8, 10, 14, 16, 18, 22, 24, 26, 30);
    asm volatile("vloxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U16(5, v4, 0x0000, 0x8cd1, 0x0000, 0x7548, 0x0000, 0x11ae, 0x0000, 0x4902, 0x0000, 0x1111, 0x0000, 0x4444);
  }
  { // eew_dest = eew_idx = e32
    volatile uint32_t BUF[] = {
      0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
      0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee,
      0x11111111, 0x22222222, 0x33333333, 0x44444444,
      0x55555555, 0x66666666, 0x77777777, 0x88888888};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(12, e32, m4);
    VLOAD_8(v0, 0xaa, 0x0a);
    VLOAD_32(v8, 4, 8, 12, 16, 20, 28, 32, 36, 44, 48, 52, 60);
    asm volatile("vloxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U32(6, v4, 0x00000000, 0x99991348, 0x00000000, 0x38197598, 0x00000000, 0x3eeeeeee,
                    0x00000000, 0x22222222, 0x00000000, 0x55555555, 0x00000000, 0x88888888);
  }
}

// EEW dest >  EEW idx  (unmasked)
void TEST_CASE3(void) {
  { // eew_dest = e16, eew_idx = e8
    volatile uint16_t BUF[] = {
      0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae,
      0x4891, 0x4902, 0x8759, 0x1989, 0x1111, 0x2222, 0x3333, 0x4444};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e16, m4);
    VLOAD_8(v8, 0, 2, 6, 8, 14, 18, 24, 30);
    asm volatile("vloxei8.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U16(7, v4, 0xbbd3, 0x3840, 0x9384, 0x7548, 0x11ae, 0x4902, 0x1111, 0x4444);
  }
  { // eew_dest = e32, eew_idx = e8
    volatile uint32_t BUF[] = {
      0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
      0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee,
      0x11111111, 0x22222222, 0x33333333, 0x44444444,
      0x55555555, 0x66666666, 0x77777777, 0x88888888};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e32, m4);
    VLOAD_8(v8, 0, 4, 12, 20, 28, 40, 52, 60);
    asm volatile("vloxei8.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U32(8, v4, 0xf9aa71f0, 0xa11a9384, 0x9fa831c7, 0x81937598, 0x3eeeeeee, 0x33333333, 0x66666666, 0x88888888);
  }
  { // eew_dest = e32, eew_idx = e16
    volatile uint32_t BUF[] = {
      0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
      0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee,
      0x11111111, 0x22222222, 0x33333333, 0x44444444,
      0x55555555, 0x66666666, 0x77777777, 0x88888888};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e32, m4);
    VLOAD_16(v8, 4, 8, 16, 24, 32, 44, 56, 60);
    asm volatile("vloxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U32(9, v4, 0xa11a9384, 0x99991348, 0x38197598, 0x18747547, 0x11111111, 0x44444444, 0x77777777, 0x88888888);
  }
}

// EEW dest >  EEW idx  (unmasked, e64)
void TEST_CASE4(void) {
#if ELEN == 64
  { // eew_dest = e64, eew_idx = e8
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(5, e64, m4);
    VLOAD_8(v8, 8, 16, 40, 72, 120);
    asm volatile("vloxei8.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U64(10, v4, 0x8913984898951989, 0x99991348a9f38cd1, 0x1111111111111111,
                     0x5555555555555555, 0xbbbbbbbbbbbbbbbb);
  }
  { // eew_dest = e64, eew_idx = e16
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(5, e64, m4);
    VLOAD_16(v8, 8, 32, 56, 96, 120);
    asm volatile("vloxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U64(11, v4, 0x8913984898951989, 0x3819759853987548, 0x3333333333333333,
                     0x8888888888888888, 0xbbbbbbbbbbbbbbbb);
  }
  { // eew_dest = e64, eew_idx = e32
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(5, e64, m4);
    VLOAD_32(v8, 8, 24, 48, 88, 120);
    asm volatile("vloxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U64(12, v4, 0x8913984898951989, 0x9fa831c7a11a9384, 0x2222222222222222,
                     0x7777777777777777, 0xbbbbbbbbbbbbbbbb);
  }
#endif
}

// EEW dest >  EEW idx  (masked, v0.t)
void TEST_CASE5(void) {
  { // eew_dest = e16, eew_idx = e8
    volatile uint16_t BUF[] = {
      0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae,
      0x4891, 0x4902, 0x8759, 0x1989, 0x1111, 0x2222, 0x3333, 0x4444};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e16, m4);
    VLOAD_8(v0, 0xb6);
    VLOAD_8(v8, 0, 2, 6, 8, 14, 18, 24, 30);
    asm volatile("vloxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U16(13, v4, 0x0000, 0x3840, 0x9384, 0x0000, 0x11ae, 0x4902, 0x0000, 0x4444);
  }
  { // eew_dest = e32, eew_idx = e8
    volatile uint32_t BUF[] = {
      0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
      0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee,
      0x11111111, 0x22222222, 0x33333333, 0x44444444,
      0x55555555, 0x66666666, 0x77777777, 0x88888888};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e32, m4);
    VLOAD_8(v0, 0xb6);
    VLOAD_8(v8, 0, 4, 12, 20, 28, 40, 52, 60);
    asm volatile("vloxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U32(14, v4, 0x00000000, 0xa11a9384, 0x9fa831c7, 0x00000000, 0x3eeeeeee, 0x33333333, 0x00000000, 0x88888888);
  }
  { // eew_dest = e32, eew_idx = e16
    volatile uint32_t BUF[] = {
      0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
      0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee,
      0x11111111, 0x22222222, 0x33333333, 0x44444444,
      0x55555555, 0x66666666, 0x77777777, 0x88888888};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e32, m4);
    VLOAD_8(v0, 0xb6);
    VLOAD_16(v8, 4, 8, 16, 24, 32, 44, 56, 60);
    asm volatile("vloxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U32(15, v4, 0x00000000, 0x99991348, 0x38197598, 0x00000000, 0x11111111, 0x44444444, 0x00000000, 0x88888888);
  }
}

// EEW dest >  EEW idx  (masked, e64)
void TEST_CASE6(void) {
#if ELEN == 64
  { // eew_dest = e64, eew_idx = e8
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(5, e64, m4);
    VLOAD_8(v0, 0x16);
    VLOAD_8(v8, 8, 16, 40, 72, 120);
    asm volatile("vloxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U64(16, v4, 0x0000000000000000, 0x99991348a9f38cd1, 0x1111111111111111,
                     0x0000000000000000, 0xbbbbbbbbbbbbbbbb);
  }
  { // eew_dest = e64, eew_idx = e16
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(5, e64, m4);
    VLOAD_8(v0, 0x16);
    VLOAD_16(v8, 8, 32, 56, 96, 120);
    asm volatile("vloxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U64(17, v4, 0x0000000000000000, 0x3819759853987548, 0x3333333333333333,
                     0x0000000000000000, 0xbbbbbbbbbbbbbbbb);
  }
  { // eew_dest = e64, eew_idx = e32
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(5, e64, m4);
    VLOAD_8(v0, 0x16);
    VLOAD_32(v8, 8, 24, 48, 88, 120);
    asm volatile("vloxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U64(18, v4, 0x0000000000000000, 0x9fa831c7a11a9384, 0x2222222222222222,
                     0x0000000000000000, 0xbbbbbbbbbbbbbbbb);
  }
#endif
}


// EEW dest <  EEW idx  (unmasked)
void TEST_CASE7(void) {
  { // eew_dest = e8, eew_idx = e16
    volatile uint8_t BUF[] = {
      0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae,
      0x91, 0x02, 0x59, 0x89, 0x11, 0x22, 0x33, 0x44};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e8, m4);
    VLOAD_16(v8, 0, 1, 3, 5, 8, 11, 13, 15);
    asm volatile("vloxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U8(19, v4, 0xd3, 0x40, 0x84, 0x88, 0x91, 0x89, 0x22, 0x44);
  }
  { // eew_dest = e8, eew_idx = e32
    volatile uint8_t BUF[] = {
      0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae,
      0x91, 0x02, 0x59, 0x89, 0x11, 0x22, 0x33, 0x44};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(4, e8, m4);
    VLOAD_32(v8, 1, 5, 10, 15);
    asm volatile("vloxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U8(20, v4, 0x40, 0x88, 0x59, 0x44);
  }
  { // eew_dest = e16, eew_idx = e32
    volatile uint16_t BUF[] = {
      0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae,
      0x4891, 0x4902, 0x8759, 0x1989, 0x1111, 0x2222, 0x3333, 0x4444};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(4, e16, m4);
    VLOAD_32(v8, 2, 8, 20, 30);
    asm volatile("vloxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U16(21, v4, 0x3840, 0x7548, 0x8759, 0x4444);
  }
}

// EEW dest <  EEW idx  (masked, v0.t)
void TEST_CASE8(void) {
  { // eew_dest = e8, eew_idx = e16 masked
    volatile uint8_t BUF[] = {
      0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae,
      0x91, 0x02, 0x59, 0x89, 0x11, 0x22, 0x33, 0x44};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(8, e8, m4);
    VLOAD_8(v0, 0xb6);
    VLOAD_16(v8, 0, 1, 3, 5, 8, 11, 13, 15);
    asm volatile("vloxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U8(22, v4, 0x00, 0x40, 0x84, 0x00, 0x91, 0x89, 0x00, 0x44);
  }
  { // eew_dest = e8, eew_idx = e32 masked
    volatile uint8_t BUF[] = {
      0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae,
      0x91, 0x02, 0x59, 0x89, 0x11, 0x22, 0x33, 0x44};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(4, e8, m4);
    VLOAD_8(v0, 0x0d);
    VLOAD_32(v8, 1, 5, 10, 15);
    asm volatile("vloxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U8(23, v4, 0x40, 0x00, 0x59, 0x44);
  }
  { // eew_dest = e16, eew_idx = e32  masked
    volatile uint16_t BUF[] = {
      0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae,
      0x4891, 0x4902, 0x8759, 0x1989, 0x1111, 0x2222, 0x3333, 0x4444};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(4, e16, m4);
    VLOAD_8(v0, 0x0d);
    VLOAD_32(v8, 2, 8, 20, 30);
    asm volatile("vloxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U16(24, v4, 0x3840, 0x0000, 0x8759, 0x4444);
  }
}

// VRF word-crossing
void TEST_CASE9(void) {
#if ELEN == 64
  { // tail in 2nd word
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(5, e64, m4);
    VLOAD_16(v8, 8, 16, 32, 48, 120);
    asm volatile("vloxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U64(25, v4, 0x8913984898951989, 0x99991348a9f38cd1, 0x3819759853987548,
                     0x2222222222222222, 0xbbbbbbbbbbbbbbbb);
  }
  { // vl=9 m2, EEW16 idx stays in 1 VRF word (18B<32B)
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(9, e64, m4);
    VLOAD_16(v8, 8, 16, 32, 40, 48, 64, 88, 96, 120);
    asm volatile("vloxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U64(26, v4, 0x8913984898951989, 0x99991348a9f38cd1, 0x3819759853987548,
                     0x1111111111111111, 0x2222222222222222, 0x4444444444444444,
                     0x7777777777777777, 0x8888888888888888, 0xbbbbbbbbbbbbbbbb);
  }
  { // vl=9 m2, EEW32 idx CROSSES VRF word at index 8 (36B>32B)
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(9, e64, m4);
    VLOAD_32(v8, 8, 16, 24, 32, 40, 48, 56, 64, 120);
    asm volatile("vloxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
    VCMP_U64(27, v4, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
                     0x3819759853987548, 0x1111111111111111, 0x2222222222222222,
                     0x3333333333333333, 0x4444444444444444, 0xbbbbbbbbbbbbbbbb);
  }
  { // vl=9 m2, EEW32 idx crossing + masked
    volatile uint64_t BUF[] = {
      0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384,
      0x3819759853987548, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333,
      0x4444444444444444, 0x5555555555555555, 0x6666666666666666, 0x7777777777777777,
      0x8888888888888888, 0x9999999999999999, 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb};
    VCLEAR(v0);
    VCLEAR(v4);
    VCLEAR(v8);
    VSET(9, e64, m4);
    VLOAD_8(v0, 0xaa, 0x01);
    VLOAD_32(v8, 8, 16, 24, 32, 40, 48, 56, 64, 120);
    asm volatile("vloxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
    VCMP_U64(28, v4, 0x0000000000000000, 0x99991348a9f38cd1, 0x0000000000000000,
                     0x3819759853987548, 0x0000000000000000, 0x2222222222222222,
                     0x0000000000000000, 0x4444444444444444, 0xbbbbbbbbbbbbbbbb);
  }
#endif
}



int main(void) {
  INIT_CHECK();
  enable_vec();
  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();
  TEST_CASE6();
  TEST_CASE7();
  TEST_CASE8();
  TEST_CASE9();
  EXIT_CHECK();
}