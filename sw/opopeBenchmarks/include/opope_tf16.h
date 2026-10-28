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

// ---------------------------------------------------------------------------------------------
// Optimised stages of the FP16 suite (see sw_optimization_report.md)
// ---------------------------------------------------------------------------------------------

// exp(x) for x <= 0 (softmax arguments after the max subtraction): TF_EXP without the upper clamp
#define TF16_EXP_NEG(x, t)                                               \
  do {                                                                   \
    asm volatile("vfmax.vf " x ", " x ", %0" ::"f"(-16.0f));             \
    asm volatile("vfmul.vf " x ", " x ", %0" ::"f"(0.015625f));          \
    asm volatile("vfmv.v.f " t ", %0" ::"f"(0.0083333333f));             \
    asm volatile("vfmul.vv " t ", " t ", " x);                           \
    asm volatile("vfadd.vf " t ", " t ", %0" ::"f"(0.0416666667f));      \
    asm volatile("vfmul.vv " t ", " t ", " x);                           \
    asm volatile("vfadd.vf " t ", " t ", %0" ::"f"(0.1666666667f));      \
    asm volatile("vfmul.vv " t ", " t ", " x);                           \
    asm volatile("vfadd.vf " t ", " t ", %0" ::"f"(0.5f));               \
    asm volatile("vfmul.vv " t ", " t ", " x);                           \
    asm volatile("vfadd.vf " t ", " t ", %0" ::"f"(1.0f));               \
    asm volatile("vfmul.vv " t ", " t ", " x);                           \
    asm volatile("vfadd.vf " t ", " t ", %0" ::"f"(1.0f));               \
    asm volatile("vfmul.vv " x ", " t ", " t);                           \
    asm volatile("vfmul.vv " x ", " x ", " x);                           \
    asm volatile("vfmul.vv " x ", " x ", " x);                           \
    asm volatile("vfmul.vv " x ", " x ", " x);                           \
    asm volatile("vfmul.vv " x ", " x ", " x);                           \
    asm volatile("vfmul.vv " x ", " x ", " x);                           \
  } while (0)

// t0 = 1 / x with two Newton-Raphson steps (rel. error ~2e-4: enough for GELU, measured max abs diff 1.9e-5)
#define TF16_RECIP2(x, t0, t1)                                           \
  do {                                                                   \
    asm volatile("vrsub.vx " t0 ", " x ", %0" ::"r"(0x7EF311C7u));       \
    TF_NR_RECIP_STEP(x, t0, t1);                                         \
    TF_NR_RECIP_STEP(x, t0, t1);                                         \
  } while (0)

// v0 <- gelu(v0) (tanh form), LMUL=4 groups; v4, v8, v12, v16 are clobbered
#define TF16_GELU_V0()                                                   \
  do {                                                                   \
    asm volatile("vfmul.vv v4, v0, v0");                                 \
    asm volatile("vfmul.vf v4, v4, %0" ::"f"(0.044715f));                \
    asm volatile("vfadd.vf v4, v4, %0" ::"f"(1.0f));                     \
    asm volatile("vfmul.vv v4, v4, v0");                                 \
    asm volatile("vfmul.vf v4, v4, %0" ::"f"(-1.5957691216f));           \
    TF_EXP("v4", "v8");                                                  \
    asm volatile("vfadd.vf v4, v4, %0" ::"f"(1.0f));                     \
    TF16_RECIP2("v4", "v12", "v16");                                     \
    asm volatile("vfmul.vv v0, v0, v12");                                \
  } while (0)

// z[0..n) = gelu(z), FP32 in place (flat)
static void tf16_gelu(float *z, unsigned int n) {
  for (unsigned int i = 0, vl; i < n; i += vl) {
    vl = tf_vsetvl(n - i);
    asm volatile("vle32.v v0, (%0)" ::"r"(z + i) : "memory");
    TF16_GELU_V0();
    asm volatile("vse32.v v0, (%0)" ::"r"(z + i) : "memory");
  }
}

// GELU of src[R][T] narrowed + packed into dstw[R/2][T] (T <= 64: one row per vector)
static void tf16_gelu_pack(uint32_t *dstw, const float *src, unsigned int R, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v0, (%0)" ::"r"(src + r * T + q0) : "memory");
      TF16_GELU_V0();
      TF_NARROW_STORE("v0", "v24", dstw, r, T, q0, vl);
    }
  }
}

// Same for T == 32: the two rows of a pair are contiguous (64 floats = one LMUL=4 vector), so the GELU runs once
// per pair on full vectors; the narrowed result is v24 (row 2p) / v25 (row 2p+1), each 32 halfwords.
static void tf16_gelu_pack_t32(uint32_t *dstw, const float *src, unsigned int R) {
  for (unsigned int p = 0; p < R / 2; p++) {
    tf_vsetvl(64);
    asm volatile("vle32.v v0, (%0)" ::"r"(src + 2 * p * 32) : "memory");
    TF16_GELU_V0();
    asm volatile("vsetvli zero, %0, e16, m2, ta, ma" ::"r"(64));
    asm volatile("vfncvt.f.f.w v24, v0");
    asm volatile("vsetvli zero, %0, e16, m1, ta, ma" ::"r"(32));
    asm volatile("vsse16.v v24, (%0), %1" ::"r"((uint16_t *)(dstw + p * 32)), "r"(4) : "memory");
    opope_vsync(dstw);
    asm volatile("vsse16.v v25, (%0), %1" ::"r"((uint16_t *)(dstw + p * 32) + 1), "r"(4) : "memory");
    opope_vsync(dstw);
  }
}

// LayerNorm statistics only (gamma/beta are folded into the following GeMM offline), output narrowed + packed
static void tf16_layernorm_pack(uint32_t *dstw, const float *in, unsigned int C, float inv_c, unsigned int T) {
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
      TF_NARROW_STORE("v4", "v24", dstw, c, T, q0, vl);
    }
  }
}

// In-place softmax over the R rows of every column of buf[R][T] (FP32, scale already folded into the inputs).
// Optionally stores the column maxima / sums (for the log-sum-exp cross entropy).
static void tf16_softmax_cols(float *buf, unsigned int R, unsigned int T, float *colmax, float *colsum) {
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
      TF16_EXP_NEG("v4", "v8");
      asm volatile("vse32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfadd.vv v28, v28, v4");
    }
    if (colmax) asm volatile("vse32.v v0, (%0)" ::"r"(colmax + q0) : "memory");
    if (colsum) asm volatile("vse32.v v28, (%0)" ::"r"(colsum + q0) : "memory");
    TF_RECIP("v28", "v12", "v16");
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmul.vv v4, v4, v12");
      asm volatile("vse32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
    }
  }
}

// Causal softmax over the keys (rows) of buf[R][T] (R == T), narrowed + packed into dstw[R/2][T]. The causal mask is
// already in buf (it is the Y operand of the score GeMM: -1e30 for key > query), so there is no mask loop. For a chunk of
// queries [q0, q0+vl) only the keys r <= q0+vl-1 can be unmasked: the other rows are skipped (their packed
// probabilities stay at the zeros that dstw was initialised with).
static void tf16_softmax_pack_causal(uint32_t *dstw, float *buf, unsigned int R, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    unsigned int rmax = q0 + vl < R ? q0 + vl : R;
    asm volatile("vfmv.v.f v0, %0" ::"f"(-1e30f));
    for (unsigned int r = 0; r < rmax; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmax.vv v0, v0, v4");
    }
    asm volatile("vmv.v.i v28, 0");
    for (unsigned int r = 0; r < rmax; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      TF16_EXP_NEG("v4", "v8");
      asm volatile("vse32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfadd.vv v28, v28, v4");
    }
    TF_RECIP("v28", "v12", "v16");
    for (unsigned int r = 0; r < rmax; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmul.vv v4, v4, v12");
      TF_NARROW_STORE("v4", "v24", dstw, r, T, q0, vl);
    }
  }
}

// loss[t] = log(colsum[t]) + colmax[t] - tgt[t]   (log-softmax identity; no division, no scalar log).
// log(x) = e*ln2 + 2*atanh((m-1)/(m+1)), x = 2^e * m, m in [1, 2): exponent bits -> float with the 2^23 trick.
static void tf16_ce_loss(float *loss, const float *colsum, const float *colmax, const float *tgt, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    asm volatile("vle32.v v0, (%0)" ::"r"(colsum + q0) : "memory");
    asm volatile("vsrl.vi v4, v0, 23");
    asm volatile("vor.vx v4, v4, %0" ::"r"(0x4B000000u));
    asm volatile("vfsub.vf v4, v4, %0" ::"f"(8388735.0f));       // 2^23 + 127: e as float
    asm volatile("vand.vx v8, v0, %0" ::"r"(0x007FFFFFu));
    asm volatile("vor.vx v8, v8, %0" ::"r"(0x3F800000u));       // m in [1, 2)
    asm volatile("vfadd.vf v12, v8, %0" ::"f"(1.0f));
    asm volatile("vfsub.vf v16, v8, %0" ::"f"(1.0f));
    TF_RECIP("v12", "v20", "v24");                               // v20 = 1 / (m + 1)
    asm volatile("vfmul.vv v16, v16, v20");                      // s
    asm volatile("vfmul.vv v20, v16, v16");                      // s^2
    asm volatile("vfmv.v.f v24, %0" ::"f"(0.1111111111f));
    asm volatile("vfmul.vv v24, v24, v20");
    asm volatile("vfadd.vf v24, v24, %0" ::"f"(0.1428571429f));
    asm volatile("vfmul.vv v24, v24, v20");
    asm volatile("vfadd.vf v24, v24, %0" ::"f"(0.2f));
    asm volatile("vfmul.vv v24, v24, v20");
    asm volatile("vfadd.vf v24, v24, %0" ::"f"(0.3333333333f));
    asm volatile("vfmul.vv v24, v24, v20");
    asm volatile("vfadd.vf v24, v24, %0" ::"f"(1.0f));
    asm volatile("vfmul.vv v24, v24, v16");
    asm volatile("vfmul.vf v24, v24, %0" ::"f"(2.0f));          // log(m)
    asm volatile("vfmul.vf v4, v4, %0" ::"f"(0.69314718056f));
    asm volatile("vfadd.vv v4, v4, v24");
    asm volatile("vle32.v v8, (%0)" ::"r"(colmax + q0) : "memory");
    asm volatile("vle32.v v12, (%0)" ::"r"(tgt + q0) : "memory");
    asm volatile("vfadd.vv v4, v4, v8");
    asm volatile("vfsub.vv v4, v4, v12");
    asm volatile("vse32.v v4, (%0)" ::"r"(loss + q0) : "memory");
  }
}

#endif  // OPOPE_TF16_H
