// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: Riccardo Giunti - Fondazione CHIPS-IT

#include "vector_macros.h"

#define INIT 98

static inline uint32_t get_vlenb_hw(void) {
  uint32_t vl;
  asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(1024));
  return vl;                       // 32 on smallvrf, 64 on standard
}

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
    asm volatile("vluxei8.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei8.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei8.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei8.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei16.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8" ::"r"(&BUF[0]) : "memory");
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
    asm volatile("vluxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]) : "memory");
    VCMP_U64(28, v4, 0x0000000000000000, 0x99991348a9f38cd1, 0x0000000000000000,
                     0x3819759853987548, 0x0000000000000000, 0x2222222222222222,
                     0x0000000000000000, 0x4444444444444444, 0xbbbbbbbbbbbbbbbb);
  }
#endif
}

void TEST_CASE10(void) {
  VSET(4, e32, m1);
  // Allocate array and zero it out
  volatile uint32_t MEM[256] = {0};

  // Base points to the middle of the array
  uint32_t *base = (uint32_t*)&MEM[128];

  // Set the values at the BYTE offsets (128 bytes = 32 elements)
  MEM[128 - 32] = 0xBAD0BAD0; // Expected if sign-extended (-128 bytes)
  MEM[128 + 32] = 0x600D600D; // Expected if zero-extended (+128 bytes)

  // Set the value for the second index (132 bytes = 33 elements)
  MEM[128 - 31] = 0xDEADBEEF; // Expected if sign-extended (-124 bytes)
  MEM[128 + 33] = 0x12345678; // Expected if zero-extended (+132 bytes)

  // 8-bit indices: 128 (0x80) and 132 (0x84)
  uint8_t indices[] = {128, 132, 0, 0};

  asm volatile(
    "vle8.v v2, (%1)\n\t"        // Load 8-bit byte-indices into v2
    "vluxei8.v v1, (%0), v2\n\t" // Unordered indexed load
    :: "r"(base), "r"(indices) : "memory"
  );

  // If Spatz has bug D22, v1 gets {0xBAD0BAD0, 0xDEADBEEF}
  // If Spatz is fixed, v1 gets {0x600D600D, 0x12345678}
  VCMP_U32(29, v1, 0x600D600D, 0x12345678, 0, 0);
}

void TEST_CASE11(void) {
  const int small = (get_vlenb_hw() == 32);
  volatile uint32_t IDX[32];
  volatile uint16_t SRC[32], OUT[64];
  for (int i = 0; i < 32; i++) {
    IDX[i] = 2 * i;                              // byte offset of OUT[i]
    SRC[i] = 0x1000 + i;
  }
  for (int i = 0; i < 64; i++) OUT[i] = 0;

  if (small) {
    VSET(32, e32, m4);
    asm volatile("vle32.v v4, (%0)\n\t" :: "r"(IDX) : "memory");
    for (volatile int w = 0; w < 1000; w++) {}
    VSET(32, e16, m2);
    asm volatile(
      "vle16.v v8, (%0)\n\t"
      "vsuxei32.v v8, (%1), v4\n\t"
      "vmv.v.i v6, 0\n\t"
      :: "r"(SRC), "r"(OUT) : "memory");
  } else {
    // Index group v4,v5: 32 x 32-bit indices
    VSET(32, e32, m2);
    asm volatile("vle32.v v4, (%0)\n\t" :: "r"(IDX) : "memory");
    for (volatile int w = 0; w < 1000; w++) {}     // indices fully loaded

    VSET(32, e16, m1);
    asm volatile(
      "vle16.v v8, (%0)\n\t"                       // store data, loaded via the VLSU so the store starts late
      "vsuxei32.v v8, (%1), v4\n\t"                // 32-bit indices under e16: index EMUL = 2 (v4,v5)
      "vmv.v.i v5, 0\n\t"                          // WAR on v5: must wait until the store has read it
      :: "r"(SRC), "r"(OUT) : "memory");
  }

  for (volatile int w = 0; w < 1000; w++) {}     // let the store finish before reloading

  asm volatile("vle16.v v12, (%0)\n\t" :: "r"(OUT + 16) : "memory");   // OUT[16..47]
  VSET(8, e16, m1);
  VCMP_U16(30, v12, 0x1010, 0x1011, 0x1012, 0x1013, 0x1014, 0x1015, 0x1016, 0x1017);
}

void TEST_CASE12(void) {
  uint32_t vlenb;
  asm volatile("csrr %0, vlenb" : "=r"(vlenb));

  // Calculate how many 32-bit elements fit into LMUL=2
  uint32_t vl_m2 = vlenb / 2;

  volatile uint32_t MEM[128];
  volatile uint32_t IDX[128];
  volatile uint32_t DUMMY[128];

  // Initialize arrays cleanly across the entire m2 span
  for (uint32_t i = 0; i < vl_m2; i++) {
    IDX[i] = i * 4;       // 32-bit byte offsets
    MEM[i] = 1285;        // The correct load data (0x0505)
    DUMMY[i] = 9999;      // Distinct dummy pre-fill data
  }

  // Pre-fill v2/v3 with 9999, and v4/v5 with offsets.
  // Using vle32.v guarantees the whole register group is written correctly.
  VSET(vl_m2, e32, m2);
  asm volatile(
    "vle32.v v2, (%0)\n\t"
    "vle32.v v4, (%1)\n\t"
    :: "r"(DUMMY), "r"(IDX) : "memory"
  );

  // Hard barrier to ensure the pre-fill is entirely committed to the VRF
  for (volatile int w = 0; w < 1000; w++) {}

  // Trigger the Hazard
  asm volatile(
    "vsetvli zero, %2, e32, m2, ta, ma\n\t"
    "vluxei32.v v2, (%0), v4\n\t"

    // Isolate the read to strictly v3
    "vsetivli zero, 4, e32, m1, ta, ma\n\t"
    "vadd.vi v6, v3, 0\n\t"
    :: "r"(MEM), "r"(IDX), "r"(vl_m2) : "memory"
  );

  // Wait loop to ensure final resolution
  for (volatile int w = 0; w < 1000; w++) {}

  // - If FIXED: vadd correctly waited for vluxei to finish writing to v3. v6 gets 1285.
  // - If BROKEN: vadd bypassed the scoreboard lock on v3. v6 reads the early pre-fill of 9999.
  VSET(4, e32, m1);
  VCMP_U32(31, v6, 1285, 1285, 1285, 1285);
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
  TEST_CASE10();
  TEST_CASE11();
  TEST_CASE12();
  EXIT_CHECK();
}