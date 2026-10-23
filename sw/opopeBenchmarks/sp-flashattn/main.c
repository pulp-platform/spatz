// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Flash-attention head: O = softmax(Q K^T / sqrt(d)) V, tiled over the keys with an online softmax.
// Per key tile (Bc keys), with the scores kept TRANSPOSED (S^T[Bc][Tq]) so that the softmax
// statistics are vectors across the queries (unit-stride rows, no transposes):
//   O-POPE  : S^T = K_t * Q^T              (M=Bc, N=d,  K=Tq; X = K_t^T tile, W = Q^T)
//   Spatz   : m_new = max(m, colmax(S^T)); alpha = exp((m - m_new)/sqrt(d));
//             P^T = exp((S^T - m_new)/sqrt(d)); l = l*alpha + colsum(P^T); O^T *= alpha
//   O-POPE  : O^T += V_t^T * P^T           (M=dv, N=Bc, K=Tq; X = V_t, W = P^T, Y = O^T rescaled)
// and at the end O^T *= 1/l (no vector divider on this cluster: Newton-Raphson reciprocal).
// exp() is FP-only (clamped, scaled Taylor polynomial + squarings).

#include "opope_app.h"

#include DATAHEADER

float *qt;
float *kt;
float *vt;
float *st;
float *ot;
float *mrow;
float *lrow;

// exp(x) of v0 (LMUL=4) -> v0, clobbers v8. FP-only: clamp x >= -16, y = x/64, degree-5 Taylor
// polynomial of exp(y) (|y| <= 0.25) and 6 squarings (a version with float<->int range reduction
// and a bit-built 2^i gave wrong lanes: dependent int->FP instructions read stale registers).
#define EXP_V0()                                                                      \
  do {                                                                                \
    asm volatile("vfmax.vf v0, v0, %0" ::"f"(-16.0f));                                \
    asm volatile("vfmul.vf v0, v0, %0" ::"f"(0.015625f));                             \
    asm volatile("vfmv.v.f v8, %0" ::"f"(0.0083333333f));                             \
    asm volatile("vfmul.vv v8, v8, v0");                                              \
    asm volatile("vfadd.vf v8, v8, %0" ::"f"(0.0416666667f));                         \
    asm volatile("vfmul.vv v8, v8, v0");                                              \
    asm volatile("vfadd.vf v8, v8, %0" ::"f"(0.1666666667f));                         \
    asm volatile("vfmul.vv v8, v8, v0");                                              \
    asm volatile("vfadd.vf v8, v8, %0" ::"f"(0.5f));                                  \
    asm volatile("vfmul.vv v8, v8, v0");                                              \
    asm volatile("vfadd.vf v8, v8, %0" ::"f"(1.0f));                                  \
    asm volatile("vfmul.vv v8, v8, v0");                                              \
    asm volatile("vfadd.vf v8, v8, %0" ::"f"(1.0f));                                  \
    asm volatile("vfmul.vv v0, v8, v8");                                              \
    asm volatile("vfmul.vv v0, v0, v0");                                              \
    asm volatile("vfmul.vv v0, v0, v0");                                              \
    asm volatile("vfmul.vv v0, v0, v0");                                              \
    asm volatile("vfmul.vv v0, v0, v0");                                              \
    asm volatile("vfmul.vv v0, v0, v0");                                              \
  } while (0)

// Online-softmax update of one score tile (st: S^T[BC][TQ] -> P^T) and rescale of O^T.
static void softmax_update(void) {
  unsigned int vl;
  for (unsigned int q0 = 0; q0 < TQ; q0 += vl) {  // vl may be < TQ with LMUL=4
  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(TQ - q0));
  float *sq = st + q0, *oq = ot + q0, *mq = mrow + q0, *lq = lrow + q0;

  // column max of the raw scores
  asm volatile("vfmv.v.f v0, %0" ::"f"(-1e30f));
  for (unsigned int r = 0; r < BC; r++) {
    asm volatile("vle32.v v12, (%0)" ::"r"(sq + r * TQ) : "memory");
    asm volatile("vfmax.vv v0, v0, v12");
  }
  // m_new (v24), alpha = exp((m_old - m_new) * scale) (v20)
  asm volatile("vle32.v v16, (%0)" ::"r"(mq) : "memory");
  asm volatile("vfmax.vv v24, v16, v0");
  asm volatile("vse32.v v24, (%0)" ::"r"(mq) : "memory");
  asm volatile("vfsub.vv v0, v16, v24");
  asm volatile("vfmul.vf v0, v0, %0" ::"f"(SCALE_F));
  EXP_V0();
  asm volatile("vmv.v.v v20, v0");

  // P^T = exp((S^T - m_new) * scale), column sums (v28)
  asm volatile("vmv.v.i v28, 0");
  for (unsigned int r = 0; r < BC; r++) {
    asm volatile("vle32.v v0, (%0)" ::"r"(sq + r * TQ) : "memory");
    asm volatile("vfsub.vv v0, v0, v24");
    asm volatile("vfmul.vf v0, v0, %0" ::"f"(SCALE_F));
    EXP_V0();
    asm volatile("vse32.v v0, (%0)" ::"r"(sq + r * TQ) : "memory");
    asm volatile("vfadd.vv v28, v28, v0");
  }

  // l = l * alpha + colsum
  asm volatile("vle32.v v0, (%0)" ::"r"(lq) : "memory");
  asm volatile("vfmul.vv v0, v0, v20");
  asm volatile("vfadd.vv v0, v0, v28");
  asm volatile("vse32.v v0, (%0)" ::"r"(lq) : "memory");

  // O^T *= alpha
  for (unsigned int j = 0; j < DV_SIZE; j++) {
    asm volatile("vle32.v v0, (%0)" ::"r"(oq + j * TQ) : "memory");
    asm volatile("vfmul.vv v0, v0, v20");
    asm volatile("vse32.v v0, (%0)" ::"r"(oq + j * TQ) : "memory");
  }
  }
}

// O^T[j][q] *= 1/l[q]. The cluster has no vector divider: the reciprocal is computed with
// Newton-Raphson iterations r <- r*(2 - l*r) starting from r = 1/T (1 <= l <= T, so l*r in
// (0, 1] and the iteration converges monotonically).
static void normalize(void) {
  unsigned int vl;
  for (unsigned int q0 = 0; q0 < TQ; q0 += vl) {
  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(TQ - q0));
  float *oq = ot + q0, *lq = lrow + q0;
  asm volatile("vle32.v v8, (%0)" ::"r"(lq) : "memory");
  asm volatile("vfmv.v.f v16, %0" ::"f"(INV_T_F));  // (no scalar fdiv on this cluster)
  for (int i = 0; i < 16; i++) {
    asm volatile("vfmul.vv v12, v8, v16");               // l*r
    asm volatile("vfrsub.vf v12, v12, %0" ::"f"(2.0f));  // 2 - l*r
    asm volatile("vfmul.vv v16, v16, v12");              // r *= (2 - l*r)
  }
  for (unsigned int j = 0; j < DV_SIZE; j++) {
    asm volatile("vle32.v v0, (%0)" ::"r"(oq + j * TQ) : "memory");
    asm volatile("vfmul.vv v0, v0, v16");
    asm volatile("vse32.v v0, (%0)" ::"r"(oq + j * TQ) : "memory");
  }
  }
}

// dst[0..n) = value
static void vfill(float *dst, unsigned int n, float value) {
  unsigned int vl;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vfmv.v.f v0, %0" ::"f"(value));
    asm volatile("vse32.v v0, (%0)" ::"r"(dst) : "memory");
    dst += vl;
    n -= vl;
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(qt, float, D_SIZE * TQ);
    OPOPE_L1_ALLOC(kt, float, T_SIZE * D_SIZE);
    OPOPE_L1_ALLOC(vt, float, T_SIZE * DV_SIZE);
    OPOPE_L1_ALLOC(st, float, BC * TQ);
    OPOPE_L1_ALLOC(ot, float, DV_SIZE * TQ);
    OPOPE_L1_ALLOC(mrow, float, TQ);
    OPOPE_L1_ALLOC(lrow, float, TQ);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(qt, fa_qt_dram, D_SIZE * TQ * sizeof(float));
    snrt_dma_start_1d(kt, fa_kt_dram, T_SIZE * D_SIZE * sizeof(float));
    snrt_dma_start_1d(vt, fa_v_dram, T_SIZE * DV_SIZE * sizeof(float));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // Initial state: m = -inf, l = 0, O = 0
    vfill(mrow, TQ, -1e30f);
    vfill(lrow, TQ, 0.0f);
    vfill(ot, DV_SIZE * TQ, 0.0f);

    for (unsigned int t = 0; t < NT; t++) {
      // S^T = K_t * Q^T  (Y = 0)
      opope_vzero(st, BC * TQ);
      opope_gemm(kt + t * D_SIZE * BC, qt, st, BC, D_SIZE, TQ);

      // Online softmax (Spatz): P^T in place of S^T, O^T rescaled
      softmax_update();

      // O^T += V_t^T * P^T
      opope_gemm(vt + t * BC * DV_SIZE, st, ot, DV_SIZE, BC, TQ);
    }

    // Final normalisation (Spatz)
    normalize();

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- flash attention: %d queries, %d keys (tiles of %d), d=%d, dv=%d -----\n", TQ,
           T_SIZE, BC, D_SIZE, DV_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(ot, (const float *)fa_golden, DV_SIZE * TQ, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
