// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Transformer pipelines with FP16 GeMM inputs and FP32 outputs (O-POPE SDOTP).
//
// Activations are FEATURE-MAJOR (see opope_transformer.h) and the GeMM operand that carries them is
// PAIR-PACKED along the rows: act_pk[R/2][T], word (p,t) = {A[2p][t] (lo), A[2p+1][t] (hi)}.
// Non-linear stages (LayerNorm, GELU, softmax) run in FP32; the stage that feeds a GeMM narrows its FP32
// rows to FP16 (vfncvt, exact RNE) and writes them straight into the packed operand: rows 2p / 2p+1 go to the
// lo / hi halfwords of word row p with two stride-4 16-bit stores. The narrowing + packing is fused into the
// last loop of the producing stage, so it costs one narrow and one strided store per row.
// One strided access is kept in flight at a time (opope_vsync after each).

#ifndef OPOPE_TF16_H
#define OPOPE_TF16_H

#include "opope_transformer.h"

// Narrow the FP32 row held in the LMUL=4 group x (vl elements starting at column q0) to FP16 and store it into
// row `row` of the packed operand dstw[R/2][T]. tmp: free LMUL=2 group. vtype must be e32/m4 on entry/exit.
#define TF_NARROW_STORE(x, tmp, dstw, row, T, q0, vl)                                           \
  do {                                                                                          \
    asm volatile("vsetvli zero, %0, e16, m2, ta, ma" ::"r"(vl));                                \
    asm volatile("vfncvt.f.f.w " tmp ", " x);                                                   \
    asm volatile("vsse16.v " tmp ", (%0), %1"                                                   \
                 ::"r"((uint16_t *)((dstw) + ((row) >> 1) * (T) + (q0)) + ((row) & 1)), "r"(4)  \
                 : "memory");                                                                   \
    opope_vsync(dstw);                                                                          \
    asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(vl));                                \
  } while (0)

// dstw[R/2][T] <- narrow(src[R][T]) packed by row pairs
static void tf_pack_rows(uint32_t *dstw, const float *src, unsigned int R, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    for (unsigned int p = 0; p < R / 2; p++) {
      asm volatile("vle32.v v0, (%0)" ::"r"(src + (2 * p) * T + q0) : "memory");
      asm volatile("vle32.v v4, (%0)" ::"r"(src + (2 * p + 1) * T + q0) : "memory");
      TF_NARROW_STORE("v0", "v24", dstw, 2 * p, T, q0, vl);
      TF_NARROW_STORE("v4", "v24", dstw, 2 * p + 1, T, q0, vl);
    }
  }
}

// LayerNorm over the C rows of every column (FP32), output narrowed + packed: dstw[C/2][T]
static void tf_layernorm_pack(uint32_t *dstw, const float *in, const float *gamma, const float *beta,
                              unsigned int C, float inv_c, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    asm volatile("vmv.v.i v0, 0");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(in + c * T + q0) : "memory");
      asm volatile("vfadd.vv v0, v0, v4");
    }
    asm volatile("vfmul.vf v0, v0, %0" ::"f"(inv_c));
    asm volatile("vmv.v.i v8, 0");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(in + c * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      asm volatile("vfmul.vv v4, v4, v4");
      asm volatile("vfadd.vv v8, v8, v4");
    }
    asm volatile("vfmul.vf v8, v8, %0" ::"f"(inv_c));
    asm volatile("vfadd.vf v8, v8, %0" ::"f"(1e-5f));
    TF_RSQRT("v8", "v12", "v16", "v20");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(in + c * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      asm volatile("vfmul.vv v4, v4, v12");
      asm volatile("vfmul.vf v4, v4, %0" ::"f"(gamma[c]));
      asm volatile("vfadd.vf v4, v4, %0" ::"f"(beta[c]));
      TF_NARROW_STORE("v4", "v24", dstw, c, T, q0, vl);
    }
  }
}

// GELU (FP32, tanh form) of src[R][T], output narrowed + packed: dstw[R/2][T]
static void tf_gelu_pack(uint32_t *dstw, const float *src, unsigned int R, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v0, (%0)" ::"r"(src + r * T + q0) : "memory");
      asm volatile("vfmul.vv v4, v0, v0");
      asm volatile("vfmul.vf v4, v4, %0" ::"f"(0.044715f));
      asm volatile("vfadd.vf v4, v4, %0" ::"f"(1.0f));
      asm volatile("vfmul.vv v4, v4, v0");
      asm volatile("vfmul.vf v4, v4, %0" ::"f"(-1.5957691216f));
      TF_EXP("v4", "v8");
      asm volatile("vfadd.vf v4, v4, %0" ::"f"(1.0f));
      TF_RECIP("v4", "v12", "v16");
      asm volatile("vfmul.vv v0, v0, v12");
      TF_NARROW_STORE("v0", "v24", dstw, r, T, q0, vl);
    }
  }
}

// In-place-free softmax over the R rows of every column of buf[R][T] after multiplying by `scale`
// (FP32; optional causal mask: row r masked for the columns t < r). buf keeps the un-normalised
// exponentials; the normalised probabilities are narrowed + packed into dstw[R/2][T].
static void tf_softmax_pack(uint32_t *dstw, float *buf, unsigned int R, unsigned int T, float scale,
                            int causal) {
  if (causal) {
    opope_vsync(buf);
    for (unsigned int r = 1; r < R; r++)
      for (unsigned int t = 0; t < r && t < T; t++) buf[r * T + t] = -1e30f;
    opope_vsync(buf);
  }
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    asm volatile("vfmv.v.f v0, %0" ::"f"(-1e30f));
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmax.vv v0, v0, v4");
    }
    asm volatile("vmv.v.i v28, 0");
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      asm volatile("vfmul.vf v4, v4, %0" ::"f"(scale));
      TF_EXP("v4", "v8");
      asm volatile("vse32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfadd.vv v28, v28, v4");
    }
    TF_RECIP("v28", "v12", "v16");
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmul.vv v4, v4, v12");
      TF_NARROW_STORE("v4", "v24", dstw, r, T, q0, vl);
    }
  }
}

#endif  // OPOPE_TF16_H
