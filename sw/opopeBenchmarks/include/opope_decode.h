// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Decode of pair-block codebooks (see common/pairblock.py) into the packed W operand [N/2][K] of the widening GeMM.
// Per group (pair p, columns g*16 .. +16): two unit-stride vector loads (16 words = 32 halfwords = one register each), one FP16
// add (it adds the even and the odd weight row at once: the halves of a word are independent), one unit-stride store. The
// indices of two groups share one 32-bit word (7-bit fields), so there is one scalar load per two groups; the word of the
// next pair of groups is loaded before the vector accesses of the current one are issued (a scalar load cannot be issued
// while vector memory accesses are in flight; the scalar ALU work that extracts the offsets overlaps with them).
// No integer vector op (4x slower than FP ops on this cluster), no zip, no strided access, no vsetvli in the loop.
// (The two loads are separated from the next pair of loads by the add and the store: four back-to-back loads hang.)

#ifndef OPOPE_DECODE_H
#define OPOPE_DECODE_H

#include "opope_app.h"

// cb0 / cb1: byte addresses of the two codebooks; entry = 64 bytes (16 words); CBN <= 128 entries
static void decode_pairblock(uint32_t *wd, const char *cb0, const char *cb1, const uint32_t *idxw, unsigned int pairs,
                             unsigned int groups, unsigned int k) {
  unsigned int vl;
  asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(32));
  const unsigned int total = pairs * (groups / 2);
  unsigned int w = idxw[0];
  unsigned int i = 0;
  for (unsigned int p = 0; p < pairs; p++)
    for (unsigned int h = 0; h < groups / 2; h++, i++) {
      const unsigned int wn = idxw[(i + 1 < total) ? i + 1 : i];
      uint32_t *dst = wd + p * k + h * 32;
      asm volatile("vle16.v v0, (%0)" ::"r"(cb0 + ((w & 127u) << 6)) : "memory");
      asm volatile("vle16.v v1, (%0)" ::"r"(cb1 + (((w >> 7) & 127u) << 6)) : "memory");
      asm volatile("vfadd.vv v0, v0, v1");
      asm volatile("vse16.v v0, (%0)" ::"r"(dst) : "memory");
      asm volatile("vle16.v v2, (%0)" ::"r"(cb0 + (((w >> 14) & 127u) << 6)) : "memory");
      asm volatile("vle16.v v3, (%0)" ::"r"(cb1 + ((w >> 21) << 6)) : "memory");
      asm volatile("vfadd.vv v2, v2, v3");
      asm volatile("vse16.v v2, (%0)" ::"r"(dst + 16) : "memory");
      w = wn;
    }
}

#endif
