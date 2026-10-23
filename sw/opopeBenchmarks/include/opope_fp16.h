// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// FP16 <-> FP32 conversions on Spatz (checked on the cluster):
//   narrowing  fp32 -> fp16 : vfncvt.f.f.w is exact (RNE, zeros/subnormals/inf handled).
//   widening   fp16 -> fp32 : vfwcvt.f.f.v is an ILLEGAL instruction. The widening multiply
//     vfwmul.vf by 1.0 (any vfw* works the same) is exact for normal numbers, but the widening datapath
//     does not classify zero/subnormal/inf/NaN operands: a zero exponent field is treated as a
//     normal number (0.0 -> 2^-15, subnormal -> 2^-15*(1+m)) and an all-ones exponent as a normal
//     number (inf -> 2^16). TF_WIDEN_FP16 therefore flushes to zero every result below 2^-14 (the
//     smallest fp16 normal) with a few integer ops. Subnormals are flushed to +0, inf/NaN are not
//     supported (the benchmarks do not produce them).

#ifndef OPOPE_FP16_H
#define OPOPE_FP16_H

#include "opope_app.h"

// dst (e32, LMUL=4 group) = widen(src (e16, LMUL=2 group)); t (LMUL=4 group) is clobbered.
// The caller must have vtype = e16/m2 with the desired vl on entry; on exit vtype is e32/m4 (same vl).
#define TF_WIDEN_FP16(src, dst, t, vl)                                       \
  do {                                                                       \
    _Float16 one16_ = (_Float16)1.0f;                                        \
    asm volatile("vfwmul.vf " dst ", " src ", %0" ::"f"(one16_));            \
    asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(vl));             \
    asm volatile("vsll.vi " t ", " dst ", 1");                               \
    asm volatile("vsrl.vi " t ", " t ", 24");                                \
    asm volatile("vsub.vx " t ", " t ", %0" ::"r"(0x70));                    \
    asm volatile("vminu.vx " t ", " t ", %0" ::"r"(1));                      \
    asm volatile("vrsub.vx " t ", " t ", zero");                             \
    asm volatile("vand.vv " dst ", " dst ", " t);                            \
  } while (0)

#endif  // OPOPE_FP16_H
