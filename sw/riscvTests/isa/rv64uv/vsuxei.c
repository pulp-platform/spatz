// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: Riccardo Giunti - Fondazione CHIPS-IT

#include "vector_macros.h"

#define INIT 98

void reset_vec8(volatile uint8_t *vec, int rst_val, uint64_t len) {
  for (uint64_t i = 0; i < len; ++i) vec[i] = rst_val;
}
void reset_vec16(volatile uint16_t *vec, int rst_val, uint64_t len) {
  for (uint64_t i = 0; i < len; ++i) vec[i] = rst_val;
}
void reset_vec32(volatile uint32_t *vec, int rst_val, uint64_t len) {
  for (uint64_t i = 0; i < len; ++i) vec[i] = rst_val;
}
void reset_vec64(volatile uint64_t *vec, int rst_val, uint64_t len) {
  for (uint64_t i = 0; i < len; ++i) vec[i] = rst_val;
}

// EEW dest = EEW idx  (unmasked)
void TEST_CASE1(void) {
    { // eew_dest = eew_idx = e8
      volatile uint8_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(12, e8, m4);
      VLOAD_8(v4, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae, 0x91, 0x02, 0x59, 0x89);
      VLOAD_8(v8, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15);
      asm volatile("vsuxei8.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U8(1, BUF, INIT, 0xd3, 0x40, 0xd1, 0x84, 0x48, INIT, 0x88, 0x88, 0xae, INIT, 0x91, 0x02, 0x59, INIT, 0x89);
    }

    { // eew_dest = eew_idx = e16
      volatile uint16_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(12, e16, m4);
      VLOAD_16(v4, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae, 0x4891, 0x4902, 0x8759, 0x1989);
      VLOAD_16(v8, 2, 4, 6, 8, 10, 14, 16, 18, 22, 24, 26, 30);
      asm volatile("vsuxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U16(2, BUF, INIT, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, INIT, 0x9388, 0x8188, 0x11ae, INIT, 0x4891, 0x4902, 0x8759, INIT, 0x1989);
    }

    { // eew_dest = eew_idx = e32
      volatile uint32_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e32, m4);
      VLOAD_32(v4, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee);
      VLOAD_32(v8, 4, 8, 12, 16, 20, 28, 32, 36);
      asm volatile("vsuxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U32(3, BUF, INIT, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598, INIT, 0x81937598, 0x18747547, 0x3eeeeeee, INIT, INIT, INIT, INIT, INIT, INIT);
    }
}

// EEW dest = EEW idx  (masked)
void TEST_CASE2(void) {
    { // eew_dest = eew_idx = e8
      volatile uint8_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(12, e8, m4);
      VLOAD_8(v0, 0xaa, 0x0a);
      VLOAD_8(v4, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae, 0x91, 0x02, 0x59, 0x89);
      VLOAD_8(v8, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15);
      asm volatile("vsuxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U8(4, BUF, INIT, INIT, 0x40, INIT, 0x84, INIT, INIT, 0x88, INIT, 0xae, INIT, INIT, 0x02, INIT, INIT, 0x89);
    }

    { // eew_dest = eew_idx = e16
      volatile uint16_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(12, e16, m4);
      VLOAD_8(v0, 0xaa, 0x0a);
      VLOAD_16(v4, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae, 0x4891, 0x4902, 0x8759, 0x1989);
      VLOAD_16(v8, 2, 4, 6, 8, 10, 14, 16, 18, 22, 24, 26, 30);
      asm volatile("vsuxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U16(5, BUF, INIT, INIT, 0x3840, INIT, 0x9384, INIT, INIT, 0x9388, INIT, 0x11ae, INIT, INIT, 0x4902, INIT, INIT, 0x1989);
    }

    { // eew_dest = eew_idx = e32
      volatile uint32_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(12, e32, m4);
      VLOAD_8(v0, 0xaa, 0x0a);
      VLOAD_32(v4, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee, 0xab8b9148, 0x90318509, 0x31897598, 0x89139848);
      VLOAD_32(v8, 4, 8, 12, 16, 20, 28, 32, 36, 44, 48, 52, 60);
      asm volatile("vsuxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U32(6, BUF, INIT, INIT, 0xa11a9384, INIT, 0x9fa831c7, INIT, INIT, 0x81937598, INIT, 0x3eeeeeee, INIT, INIT, 0x90318509, INIT, INIT, 0x89139848);
    }
}

// EEW dest >  EEW idx  (unmasked)
void TEST_CASE3(void) {
    { // eew_dest = e16, eew_idx = e8
      volatile uint16_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e16, m4);
      VLOAD_16(v4, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae);
      VLOAD_8(v8, 0, 2, 6, 8, 14, 18, 24, 30);
      asm volatile("vsuxei8.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U16(7, BUF, 0xbbd3, 0x3840, INIT, 0x8cd1, 0x9384, INIT, INIT, 0x7548, INIT, 0x9388, INIT, INIT, 0x8188, INIT, INIT, 0x11ae);
    }

    { // eew_dest = e32, eew_idx = e8
      volatile uint32_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e32, m4);
      VLOAD_32(v4, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee);
      VLOAD_8(v8, 0, 4, 12, 20, 28, 40, 52, 60);
      asm volatile("vsuxei8.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U32(8, BUF, 0xf9aa71f0, 0xa11a9384, INIT, 0x99991348, INIT, 0x9fa831c7, INIT, 0x38197598, INIT, INIT, 0x81937598, INIT, INIT, 0x18747547, INIT, 0x3eeeeeee);
    }

    { // eew_dest = e32, eew_idx = e16
      volatile uint32_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e32, m4);
      VLOAD_32(v4, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee);
      VLOAD_16(v8, 4, 8, 16, 24, 32, 44, 56, 60);
      asm volatile("vsuxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U32(9, BUF, INIT, 0xf9aa71f0, 0xa11a9384, INIT, 0x99991348, INIT, 0x9fa831c7, INIT, 0x38197598, INIT, INIT, 0x81937598, INIT, INIT, 0x18747547, 0x3eeeeeee);
    }
}

// EEW dest >  EEW idx  (unmasked, e64)
void TEST_CASE4(void) {
#if ELEN == 64
    { // eew_dest = e64, eew_idx = e8
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);    
      VSET(5, e64, m4);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548);
      VLOAD_8(v8, 8, 16, 40, 72, 120);
      asm volatile("vsuxei8.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U64(10, BUF, INIT, 0xf9aa71f0c394bbd3, 0x8913984898951989, INIT, INIT, 0x99991348a9f38cd1, INIT, INIT, INIT, 0x9fa831c7a11a9384, INIT, INIT, INIT, INIT, INIT, 0x3819759853987548);
    }

    { // eew_dest = e64, eew_idx = e16
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(5, e64, m4);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548);
      VLOAD_16(v8, 8, 32, 56, 96, 120);
      asm volatile("vsuxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U64(11, BUF, INIT, 0xf9aa71f0c394bbd3, INIT, INIT, 0x8913984898951989, INIT, INIT, 0x99991348a9f38cd1, INIT, INIT, INIT, INIT, 0x9fa831c7a11a9384, INIT, INIT, 0x3819759853987548);
    }

    { // eew_dest = e64, eew_idx = e32
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(5, e64, m4);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548);
      VLOAD_32(v8, 8, 24, 48, 88, 120);
      asm volatile("vsuxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U64(12, BUF, INIT, 0xf9aa71f0c394bbd3, INIT, 0x8913984898951989, INIT, INIT, 0x99991348a9f38cd1, INIT, INIT, INIT, INIT, 0x9fa831c7a11a9384, INIT, INIT, INIT, 0x3819759853987548);
    }
#endif
}

// EEW dest >  EEW idx  (masked, v0.t)
void TEST_CASE5(void) {
    { // eew_dest = e16, eew_idx = e8
      volatile uint16_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e16, m4);
      VLOAD_8(v0, 0xb6);
      VLOAD_16(v4, 0xbbd3, 0x3840, 0x8cd1, 0x9384, 0x7548, 0x9388, 0x8188, 0x11ae);
      VLOAD_8(v8, 0, 2, 6, 8, 14, 18, 24, 30);
      asm volatile("vsuxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U16(13, BUF, INIT, 0x3840, INIT, 0x8cd1, INIT, INIT, INIT, 0x7548, INIT, 0x9388, INIT, INIT, INIT, INIT, INIT, 0x11ae);
    }

    { // eew_dest = e32, eew_idx = e8
      volatile uint32_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e32, m4);
      VLOAD_8(v0, 0xb6);
      VLOAD_32(v4, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee);
      VLOAD_8(v8, 0, 4, 12, 20, 28, 40, 52, 60);
      asm volatile("vsuxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U32(14, BUF, INIT, 0xa11a9384, INIT, 0x99991348, INIT, INIT, INIT, 0x38197598, INIT, INIT, 0x81937598, INIT, INIT, INIT, INIT, 0x3eeeeeee);
    }

    { // eew_dest = e32, eew_idx = e16
      volatile uint32_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e32, m4);
      VLOAD_8(v0, 0xb6);
      VLOAD_32(v4, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7, 0x38197598, 0x81937598, 0x18747547, 0x3eeeeeee);
      VLOAD_16(v8, 4, 8, 16, 24, 32, 44, 56, 60);
      asm volatile("vsuxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U32(15, BUF, INIT, INIT, 0xa11a9384, INIT, 0x99991348, INIT, INIT, INIT, 0x38197598, INIT, INIT, 0x81937598, INIT, INIT, INIT, 0x3eeeeeee);
    }
}

// EEW dest >  EEW idx  (masked, e64)
void TEST_CASE6(void) {
#if ELEN == 64
    { // eew_dest = e64, eew_idx = e8
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(5, e64, m4);
      VLOAD_8(v0, 0x16);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548);
      VLOAD_8(v8, 8, 16, 40, 72, 120);
      asm volatile("vsuxei8.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U64(16, BUF, INIT, INIT, 0x8913984898951989, INIT, INIT, 0x99991348a9f38cd1, INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT, 0x3819759853987548);
    }

    { // eew_dest = e64, eew_idx = e16
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(5, e64, m4);
      VLOAD_8(v0, 0x16);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548);
      VLOAD_16(v8, 8, 32, 56, 96, 120);
      asm volatile("vsuxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U64(17, BUF, INIT, INIT, INIT, INIT, 0x8913984898951989, INIT, INIT, 0x99991348a9f38cd1, INIT, INIT, INIT, INIT, INIT, INIT, INIT, 0x3819759853987548);
    }

    { // eew_dest = e64, eew_idx = e32
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(5, e64, m4);
      VLOAD_8(v0, 0x16);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x99991348a9f38cd1, 0x9fa831c7a11a9384, 0x3819759853987548);
      VLOAD_32(v8, 8, 24, 48, 88, 120);
      asm volatile("vsuxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U64(18, BUF, INIT, INIT, INIT, 0x8913984898951989, INIT, INIT, 0x99991348a9f38cd1, INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT, 0x3819759853987548);
    }
#endif
}

// EEW dest <  EEW idx  (unmasked)
void TEST_CASE7(void) {
    { // eew_dest = e8, eew_idx = e16
      volatile uint8_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e8, m4);
      VLOAD_8(v4, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae);
      VLOAD_16(v8, 0, 1, 3, 5, 8, 11, 13, 15);
      asm volatile("vsuxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U8(19, BUF, 0xd3, 0x40, INIT, 0xd1, INIT, 0x84, INIT, INIT, 0x48, INIT, INIT, 0x88, INIT, 0x88, INIT, 0xae);
    }

    { // eew_dest = e8, eew_idx = e32
      volatile uint8_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(4, e8, m4);
      VLOAD_8(v4, 0xd3, 0x40, 0xd1, 0x84);
      VLOAD_32(v8, 1, 5, 10, 15);
      asm volatile("vsuxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U8(20, BUF, INIT, 0xd3, INIT, INIT, INIT, 0x40, INIT, INIT, INIT, INIT, 0xd1, INIT, INIT, INIT, INIT, 0x84);
    }

    { // eew_dest = e16, eew_idx = e32
      volatile uint16_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(4, e16, m4);
      VLOAD_16(v4, 0xbbd3, 0x3840, 0x8cd1, 0x9384);
      VLOAD_32(v8, 2, 8, 20, 30);
      asm volatile("vsuxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U16(21, BUF, INIT, 0xbbd3, INIT, INIT, 0x3840, INIT, INIT, INIT, INIT, INIT, 0x8cd1, INIT, INIT, INIT, INIT, 0x9384);
    }
}

// EEW dest <  EEW idx  (masked, v0.t)
void TEST_CASE8(void) {
    { // eew_dest = e8, eew_idx = e16 masked
      volatile uint8_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(8, e8, m4);
      VLOAD_8(v0, 0xb6);
      VLOAD_8(v4, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x88, 0x88, 0xae);
      VLOAD_16(v8, 0, 1, 3, 5, 8, 11, 13, 15);
      asm volatile("vsuxei16.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U8(22, BUF, INIT, 0x40, INIT, 0xd1, INIT, INIT, INIT, INIT, 0x48, INIT, INIT, 0x88, INIT, INIT, INIT, 0xae);
    }

    { // eew_dest = e8, eew_idx = e32 masked
      volatile uint8_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(4, e8, m4);
      VLOAD_8(v0, 0x0d);
      VLOAD_8(v4, 0xd3, 0x40, 0xd1, 0x84);
      VLOAD_32(v8, 1, 5, 10, 15);
      asm volatile("vsuxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U8(23, BUF, INIT, 0xd3, INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT, 0xd1, INIT, INIT, INIT, INIT, 0x84);
    }

    { // eew_dest = e16, eew_idx = e32 masked
      volatile uint16_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(4, e16, m4);
      VLOAD_8(v0, 0x0d);
      VLOAD_16(v4, 0xbbd3, 0x3840, 0x8cd1, 0x9384);
      VLOAD_32(v8, 2, 8, 20, 30);
      asm volatile("vsuxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U16(24, BUF, INIT, 0xbbd3, INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT, 0x8cd1, INIT, INIT, INIT, INIT, 0x9384);
    }
}

// VRF word-crossing
void TEST_CASE9(void) {
#if ELEN == 64
    { // tail in 2nd word
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(5, e64, m4);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x0000000000000001, 0x0000000000000002, 0x0000000000000008);
      VLOAD_16(v8, 8, 16, 32, 48, 120);
      asm volatile("vsuxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U64(25, BUF, INIT, 0xf9aa71f0c394bbd3, 0x8913984898951989, INIT, 0x0000000000000001, INIT, 0x0000000000000002, INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT, 0x0000000000000008);
    }

    { // vl=9 m2, EEW16 idx stays in 1 VRF word (18B<32B)
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(9, e64, m4);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x0000000000000001, 0x0000000000000002, 0x0000000000000003, 0x0000000000000004, 0x0000000000000005, 0x0000000000000006, 0x0000000000000007);
      VLOAD_16(v8, 8, 16, 32, 40, 48, 64, 88, 96, 120);
      asm volatile("vsuxei16.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U64(26, BUF, INIT, 0xf9aa71f0c394bbd3, 0x8913984898951989, INIT, 0x0000000000000001, 0x0000000000000002, 0x0000000000000003, INIT, 0x0000000000000004, INIT, INIT, 0x0000000000000005, 0x0000000000000006, INIT, INIT, 0x0000000000000007);
    }

    { // vl=9 m2, EEW32 idx CROSSES VRF word at index 8 (36B>32B)
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(9, e64, m4);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x0000000000000001, 0x0000000000000002, 0x0000000000000003, 0x0000000000000004, 0x0000000000000005, 0x0000000000000006, 0x0000000000000007);
      VLOAD_32(v8, 8, 16, 24, 32, 40, 48, 56, 64, 120);
      asm volatile("vsuxei32.v v4, (%0), v8" ::"r"(&BUF[0]));
      VVCMP_U64(27, BUF, INIT, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x0000000000000001, 0x0000000000000002, 0x0000000000000003, 0x0000000000000004, 0x0000000000000005, 0x0000000000000006, INIT, INIT, INIT, INIT, INIT, INIT, 0x0000000000000007);
    }

    { // vl=9 m2, EEW32 idx crossing + masked
      volatile uint64_t BUF[] = {
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT,
        INIT, INIT, INIT, INIT, INIT, INIT, INIT, INIT};
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(9, e64, m4);
      VLOAD_8(v0, 0xaa, 0x01);
      VLOAD_64(v4, 0xf9aa71f0c394bbd3, 0x8913984898951989, 0x0000000000000001, 0x0000000000000002, 0x0000000000000003, 0x0000000000000004, 0x0000000000000005, 0x0000000000000006, 0x0000000000000007);
      VLOAD_32(v8, 8, 16, 24, 32, 40, 48, 56, 64, 120);
      asm volatile("vsuxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      VVCMP_U64(28, BUF, INIT, INIT, 0x8913984898951989, INIT, 0x0000000000000002, INIT, 0x0000000000000004, INIT, 0x0000000000000006, INIT, INIT, INIT, INIT, INIT, INIT, 0x0000000000000007);
    }
#endif
}


void TEST_CASE10(void) {
  {
      volatile uint16_t BUF[64];
      for (int i = 0; i < 64; i++) BUF[i] = INIT;

      VCLEAR(v4);
      VCLEAR(v8);
      VSET(25, e16, m4);
      VLOAD_16(v4, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007,
                  0x2008, 0x2009, 0x200a, 0x200b, 0x200c, 0x200d, 0x200e, 0x200f,
                  0x2010, 0x2011, 0x2012, 0x2013, 0x2014, 0x2015, 0x2016, 0x2017, 0x2018);

      VLOAD_32(v8, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
                  100, 110, 120, 118, 126, 60, 34, 44, 50);
      asm volatile("vsuxei32.v v4, (%0), v8" ::"r"(&BUF[0]));

      VVCMP_U16(29, BUF,
      0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007,
      0x2008, 0x2009, 0x200a, 0x200b, 0x200c, 0x200d, 0x200e, 0x200f,
      INIT,   0x2016, INIT,   INIT,   INIT,   INIT,   0x2017, INIT,
      INIT,   0x2018, INIT,   INIT,   INIT,   INIT,   0x2015, INIT,
      INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,
      INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,
      INIT,   INIT,   0x2010, INIT,   INIT,   INIT,   INIT,   0x2011,
      INIT,   INIT,   INIT,   0x2013, 0x2012, INIT,   INIT,   0x2014);
  }
 
  {
      volatile uint16_t BUF[64];
      for (int i = 0; i < 64; i++) BUF[i] = INIT;
      
      VCLEAR(v0);
      VCLEAR(v4);
      VCLEAR(v8);
      VSET(25, e16, m4);
      VLOAD_8(v0, 0xaa, 0xaa, 0xaa, 0x0a);
      VLOAD_16(v4, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007,
                  0x2008, 0x2009, 0x200a, 0x200b, 0x200c, 0x200d, 0x200e, 0x200f,
                  0x2010, 0x2011, 0x2012, 0x2013, 0x2014, 0x2015, 0x2016, 0x2017, 0x2018);
      VLOAD_32(v8, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
                  100, 110, 120, 118, 126, 60, 34, 44, 50);
      asm volatile("vsuxei32.v v4, (%0), v8, v0.t" ::"r"(&BUF[0]));
      
      // active elements: 1,3,5,7,9,11,13,15,17,19,21,23
      VVCMP_U16(30, BUF,
      INIT,   0x2001, INIT,   0x2003, INIT,   0x2005, INIT,   0x2007,
      INIT,   0x2009, INIT,   0x200b, INIT,   0x200d, INIT,   0x200f,
      INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   0x2017, INIT,
      INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   0x2015, INIT,
      INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,
      INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,
      INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   INIT,   0x2011,
      INIT,   INIT,   INIT,   0x2013, INIT,   INIT,   INIT,   INIT);
  } 
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
  EXIT_CHECK();
}