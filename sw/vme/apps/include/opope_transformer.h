// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Spatz building blocks of the transformer pipelines (FP32).
//
// Layout convention: activations are FEATURE-MAJOR, A^T[C][T] (channel rows, one column per
// token). With it every linear layer is a single O-POPE job without transposes:
//   Z^T[Cout][T] = Y + Wt * A^T   (M=Cout, N=Cin, K=T; X operand = W[Cin][Cout] stored as it
//                                  comes, i.e. "in x out" row-major = the transposed X operand)
// and the per-token reductions (LayerNorm, softmax) run ACROSS ROWS with the tokens as the vector
// dimension (unit-stride loads, no transposes).
//
// The cluster has no vector/scalar divider and no vfsqrt/vfrec7/vfrsqrt7 (illegal instructions):
//   1/x      : bit-trick initial guess + 3 Newton-Raphson iterations (any positive normal x)
//   1/sqrt(x): "fast inverse square root" guess + 3 Newton-Raphson iterations
//   exp(x)   : clamped, scaled Taylor polynomial + 6 squarings (max rel. error ~1e-4)
//   log(x)   : scalar, exponent extraction + polynomial (used by the cross-entropy loss only)
// Never divide in C code (it would compile to fdiv.s) and never printf("%f").

#ifndef OPOPE_TRANSFORMER_H
#define OPOPE_TRANSFORMER_H

#include "opope_app.h"

// Copy a constant array from DRAM (initial data placement, outside of the measured region)
#define TF_DMA(ptr, arr) snrt_dma_start_1d((ptr), (arr), sizeof(arr))

// ---------------------------------------------------------------------------------------------
// Vector math macros. Arguments are register names (string literals) of LMUL=4 register groups.
// ---------------------------------------------------------------------------------------------

// One Newton-Raphson step of the reciprocal: r(t0) <- r * (2 - x * r)
#define TF_NR_RECIP_STEP(x, t0, t1)                                  \
  do {                                                               \
    asm volatile("vfmul.vv " t1 ", " x ", " t0);                     \
    asm volatile("vfrsub.vf " t1 ", " t1 ", %0" ::"f"(2.0f));        \
    asm volatile("vfmul.vv " t0 ", " t0 ", " t1);                    \
  } while (0)

// t0 = 1 / x   (x, t1 are preserved / clobbered respectively)
#define TF_RECIP(x, t0, t1)                                              \
  do {                                                                   \
    asm volatile("vrsub.vx " t0 ", " x ", %0" ::"r"(0x7EF311C7u));       \
    TF_NR_RECIP_STEP(x, t0, t1);                                         \
    TF_NR_RECIP_STEP(x, t0, t1);                                         \
    TF_NR_RECIP_STEP(x, t0, t1);                                         \
  } while (0)

// One Newton-Raphson step of 1/sqrt: r(t0) <- r * (1.5 - 0.5*x * r^2), t2 = 0.5*x
#define TF_NR_RSQRT_STEP(t0, t1, t2)                                 \
  do {                                                               \
    asm volatile("vfmul.vv " t1 ", " t0 ", " t0);                    \
    asm volatile("vfmul.vv " t1 ", " t1 ", " t2);                    \
    asm volatile("vfrsub.vf " t1 ", " t1 ", %0" ::"f"(1.5f));        \
    asm volatile("vfmul.vv " t0 ", " t0 ", " t1);                    \
  } while (0)

// t0 = 1 / sqrt(x)   (x preserved, t1/t2 clobbered)
#define TF_RSQRT(x, t0, t1, t2)                                          \
  do {                                                                   \
    asm volatile("vsrl.vi " t0 ", " x ", 1");                            \
    asm volatile("vrsub.vx " t0 ", " t0 ", %0" ::"r"(0x5F3759DFu));      \
    asm volatile("vfmul.vf " t2 ", " x ", %0" ::"f"(0.5f));              \
    TF_NR_RSQRT_STEP(t0, t1, t2);                                        \
    TF_NR_RSQRT_STEP(t0, t1, t2);                                        \
    TF_NR_RSQRT_STEP(t0, t1, t2);                                        \
  } while (0)

// x = exp(clamp(x, -16, 20)); t is clobbered. FP-only (no float<->int conversions).
#define TF_EXP(x, t)                                                     \
  do {                                                                   \
    asm volatile("vfmax.vf " x ", " x ", %0" ::"f"(-16.0f));             \
    asm volatile("vfmin.vf " x ", " x ", %0" ::"f"(20.0f));              \
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

// Strip-mining helper: vector length of the next chunk (LMUL=4, FP32)
static inline unsigned int tf_vsetvl(unsigned int n) {
  unsigned int vl;
  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(n));
  return vl;
}

// ---------------------------------------------------------------------------------------------
// Row / elementwise helpers on feature-major matrices [rows][T]
// ---------------------------------------------------------------------------------------------

// z[r][0..T) = bias[r]
static void tf_fill_rows(float *z, const float *bias, unsigned int rows, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    for (unsigned int r = 0; r < rows; r++) {
      asm volatile("vfmv.v.f v0, %0" ::"f"(bias[r]));
      asm volatile("vse32.v v0, (%0)" ::"r"(z + r * T + q0) : "memory");
    }
  }
}

// z[r][0..T) += bias[r]
static void tf_add_bias_rows(float *z, const float *bias, unsigned int rows, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    for (unsigned int r = 0; r < rows; r++) {
      asm volatile("vle32.v v0, (%0)" ::"r"(z + r * T + q0) : "memory");
      asm volatile("vfadd.vf v0, v0, %0" ::"f"(bias[r]));
      asm volatile("vse32.v v0, (%0)" ::"r"(z + r * T + q0) : "memory");
    }
  }
}

// z[r][0..T) = row r of `bias` broadcast along the columns: z[r][t] = bias[t] (token-major Y)
static void tf_fill_cols(float *z, const float *bias, unsigned int rows, unsigned int len) {
  for (unsigned int q0 = 0, vl; q0 < len; q0 += vl) {
    vl = tf_vsetvl(len - q0);
    asm volatile("vle32.v v0, (%0)" ::"r"(bias + q0) : "memory");
    for (unsigned int r = 0; r < rows; r++)
      asm volatile("vse32.v v0, (%0)" ::"r"(z + r * len + q0) : "memory");
  }
}

// dst[0..n) += src[0..n)      (residual connection)
static void tf_add(float *dst, const float *src, unsigned int n) {
  for (unsigned int i = 0, vl; i < n; i += vl) {
    vl = tf_vsetvl(n - i);
    asm volatile("vle32.v v0, (%0)" ::"r"(dst + i) : "memory");
    asm volatile("vle32.v v4, (%0)" ::"r"(src + i) : "memory");
    asm volatile("vfadd.vv v0, v0, v4");
    asm volatile("vse32.v v0, (%0)" ::"r"(dst + i) : "memory");
  }
}

// z[0..n) = max(z, 0)
static void tf_relu(float *z, unsigned int n) {
  for (unsigned int i = 0, vl; i < n; i += vl) {
    vl = tf_vsetvl(n - i);
    asm volatile("vle32.v v0, (%0)" ::"r"(z + i) : "memory");
    asm volatile("vfmax.vf v0, v0, %0" ::"f"(0.0f));
    asm volatile("vse32.v v0, (%0)" ::"r"(z + i) : "memory");
  }
}

// z[0..n) = gelu(z), tanh form: 0.5 x (1 + tanh(c (x + 0.044715 x^3))) = x / (1 + exp(-2 c (x + 0.044715 x^3)))
static void tf_gelu(float *z, unsigned int n) {
  for (unsigned int i = 0, vl; i < n; i += vl) {
    vl = tf_vsetvl(n - i);
    asm volatile("vle32.v v0, (%0)" ::"r"(z + i) : "memory");
    asm volatile("vfmul.vv v4, v0, v0");
    asm volatile("vfmul.vf v4, v4, %0" ::"f"(0.044715f));
    asm volatile("vfadd.vf v4, v4, %0" ::"f"(1.0f));
    asm volatile("vfmul.vv v4, v4, v0");                          // x + 0.044715 x^3
    asm volatile("vfmul.vf v4, v4, %0" ::"f"(-1.5957691216f));    // -2 sqrt(2/pi) (...)
    TF_EXP("v4", "v8");
    asm volatile("vfadd.vf v4, v4, %0" ::"f"(1.0f));
    TF_RECIP("v4", "v12", "v16");
    asm volatile("vfmul.vv v0, v0, v12");
    asm volatile("vse32.v v0, (%0)" ::"r"(z + i) : "memory");
  }
}

// out[c][t] = (in[c][t] - mean_t) * rstd_t * gamma[c] + beta[c], statistics over the C rows of
// every column t (out may alias in). inv_c = 1/C, passed in because there is no divider.
static void tf_layernorm(float *out, const float *in, const float *gamma, const float *beta,
                         unsigned int C, float inv_c, unsigned int T) {
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    // mean (v0)
    asm volatile("vmv.v.i v0, 0");
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(in + c * T + q0) : "memory");
      asm volatile("vfadd.vv v0, v0, v4");
    }
    asm volatile("vfmul.vf v0, v0, %0" ::"f"(inv_c));
    // variance (v8) -> rstd (v12)
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
    // normalise + affine
    for (unsigned int c = 0; c < C; c++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(in + c * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      asm volatile("vfmul.vv v4, v4, v12");
      asm volatile("vfmul.vf v4, v4, %0" ::"f"(gamma[c]));
      asm volatile("vfadd.vf v4, v4, %0" ::"f"(beta[c]));
      asm volatile("vse32.v v4, (%0)" ::"r"(out + c * T + q0) : "memory");
    }
  }
}

// In-place softmax over the R rows of every column of buf[R][T], after multiplying by `scale`.
// With `causal`, row r (key) is masked for the columns t < r (query): buf[r][t] -> -1e30 first
// (the masked probabilities come out as exp(-16) ~ 1e-7 of the maximum, i.e. negligible).
static void tf_softmax_cols(float *buf, unsigned int R, unsigned int T, float scale, int causal) {
  if (causal) {
    opope_vsync(buf);
    for (unsigned int r = 1; r < R; r++)
      for (unsigned int t = 0; t < r && t < T; t++) buf[r * T + t] = -1e30f;
    opope_vsync(buf);
  }
  for (unsigned int q0 = 0, vl; q0 < T; q0 += vl) {
    vl = tf_vsetvl(T - q0);
    // column max (v0)
    asm volatile("vfmv.v.f v0, %0" ::"f"(-1e30f));
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmax.vv v0, v0, v4");
    }
    // p = exp((s - max) * scale), column sums (v28)
    asm volatile("vmv.v.i v28, 0");
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfsub.vv v4, v4, v0");
      asm volatile("vfmul.vf v4, v4, %0" ::"f"(scale));
      TF_EXP("v4", "v8");
      asm volatile("vse32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfadd.vv v28, v28, v4");
    }
    // normalise (v12 = 1/sum)
    TF_RECIP("v28", "v12", "v16");
    for (unsigned int r = 0; r < R; r++) {
      asm volatile("vle32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
      asm volatile("vfmul.vv v4, v4, v12");
      asm volatile("vse32.v v4, (%0)" ::"r"(buf + r * T + q0) : "memory");
    }
  }
}

// natural logarithm of a positive normal float (scalar: exponent extraction + polynomial,
// |error| < 1e-5)
static inline float tf_logf(float x) {
  union {
    float f;
    uint32_t u;
  } v = {x};
  int e = (int)(v.u >> 23) - 127;
  v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;  // mantissa in [1, 2)
  float m = v.f;
  if (m > 1.41421356f) {
    m *= 0.5f;
    e += 1;
  }
  const float z = m - 1.0f;  // log(1+z), z in [-0.293, 0.415]
  // Taylor series of log(1+z) up to z^12 (Horner)
  float p = -0.0833333333f;
  p = p * z + 0.0909090909f;
  p = p * z - 0.1f;
  p = p * z + 0.1111111111f;
  p = p * z - 0.125f;
  p = p * z + 0.1428571429f;
  p = p * z - 0.1666666667f;
  p = p * z + 0.2f;
  p = p * z - 0.25f;
  p = p * z + 0.3333333333f;
  p = p * z - 0.5f;
  p = p * z + 1.0f;
  return (float)e * 0.69314718056f + p * z;
}

// emb[t][0..C) = wte[tok[t]][0..C) + wpe[t][0..C)   (token-major; unit-stride row loads)
static void tf_encoder(float *emb, const uint32_t *tok, const float *wte, const float *wpe,
                       unsigned int T, unsigned int C) {
  for (unsigned int t = 0; t < T; t++) {
    const float *row = wte + tok[t] * C;
    for (unsigned int c0 = 0, vl; c0 < C; c0 += vl) {
      vl = tf_vsetvl(C - c0);
      asm volatile("vle32.v v0, (%0)" ::"r"(row + c0) : "memory");
      asm volatile("vle32.v v4, (%0)" ::"r"(wpe + t * C + c0) : "memory");
      asm volatile("vfadd.vv v0, v0, v4");
      asm volatile("vse32.v v0, (%0)" ::"r"(emb + t * C + c0) : "memory");
    }
  }
}

// ident = identity matrix [n][n]
static void tf_identity(float *m, unsigned int n) {
  opope_vzero(m, n * n);
  for (unsigned int i = 0; i < n; i++) m[i * n + i] = 1.0f;
  opope_vsync(m);
}

// Transpose with O-POPE: dst[cols][rows] = src[rows][cols]^T (dst is zero-filled here). The
// identity must be [rows][rows]; `cols` must be a multiple of 16 (M of the job).
static void tf_transpose(float *dst, const float *src, const float *ident, unsigned int rows,
                         unsigned int cols) {
  opope_vzero(dst, rows * cols);
  opope_gemm(src, ident, dst, cols, rows, rows);
}

#endif  // OPOPE_TRANSFORMER_H
