// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Flash-attention head: O = softmax(Q K^T / sqrt(d)) V, tiled over the keys with an online softmax,
// FP16 Q/K/V, FP32 statistics and output. Per key tile (Bc keys), with the scores kept TRANSPOSED
// (S^T[Bc][Tq]) so that the softmax statistics are vectors across the queries (no transposes):
//   O-POPE  : S^T = K_t * Q^T              (FP16 -> FP32; M=Bc, N=d, K=Tq; Q, K tiles pre-packed offline)
//   Spatz   : m_new = max(m, colmax(S^T)); alpha = exp((m - m_new)/sqrt(d));
//             P^T = exp((S^T - m_new)/sqrt(d)) in FP32 (non-linear); l = l*alpha + colsum(P^T);
//             O^T *= alpha; P^T is narrowed to FP16 (vfncvt) and written pair-packed into the W operand
//             of the next GeMM with two stride-4 16-bit stores per row pair (rows 2r, 2r+1 -> lo, hi)
//   O-POPE  : O^T += V_t^T * P^T           (FP16 -> FP32; M=dv, N=Bc, K=Tq; Y = O^T rescaled, FP32)
// and at the end O^T *= 1/l (no divider on this cluster: bit-trick + Newton-Raphson reciprocal).

#include "opope_transformer.h"

#include DATAHEADER

uint32_t *qw;
uint32_t *kw;
uint32_t *vw;
float *st;
uint32_t *pw;
float *ot;
float *mrow;
float *lrow;

// Online-softmax update of one score tile (st: S^T[BC][TQ] FP32 -> P^T FP16 packed in pw) and rescale of O^T.
// TQ = 64: a row of queries is exactly one LMUL=4 e32 vector. Registers (LMUL=4 groups): v0 scratch/max,
// v4 row, v8 exp scratch, v12 narrowed P (e16 LMUL=2), v16 m_old, v20 alpha, v24 m_new, v28 column sum.
static void softmax_update(void) {
  unsigned int vl = tf_vsetvl(TQ);

  // column max of the raw scores
  asm volatile("vfmv.v.f v0, %0" ::"f"(-1e30f));
  for (unsigned int r = 0; r < BC; r++) {
    asm volatile("vle32.v v4, (%0)" ::"r"(st + r * TQ) : "memory");
    asm volatile("vfmax.vv v0, v0, v4");
  }
  // m_new (v24), alpha = exp((m_old - m_new) * scale) (v20)
  asm volatile("vle32.v v16, (%0)" ::"r"(mrow) : "memory");
  asm volatile("vfmax.vv v24, v16, v0");
  asm volatile("vse32.v v24, (%0)" ::"r"(mrow) : "memory");
  asm volatile("vfsub.vv v0, v16, v24");
  asm volatile("vfmul.vf v0, v0, %0" ::"f"(SCALE_F));
  TF_EXP("v0", "v8");
  asm volatile("vmv.v.v v20, v0");

  // P^T = exp((S^T - m_new) * scale), column sums (v28), narrowed + packed into pw
  asm volatile("vmv.v.i v28, 0");
  for (unsigned int r = 0; r < BC; r++) {
    asm volatile("vle32.v v4, (%0)" ::"r"(st + r * TQ) : "memory");
    asm volatile("vfsub.vv v4, v4, v24");
    asm volatile("vfmul.vf v4, v4, %0" ::"f"(SCALE_F));
    TF_EXP("v4", "v8");
    asm volatile("vfadd.vv v28, v28, v4");
    asm volatile("vsetvli zero, %0, e16, m2, ta, ma" ::"r"(vl));
    asm volatile("vfncvt.f.f.w v12, v4");
    asm volatile("vsse16.v v12, (%0), %1"
                 ::"r"((uint16_t *)(pw + (r >> 1) * TQ) + (r & 1)), "r"(4)
                 : "memory");
    opope_vsync(pw);  // one strided access in flight at a time
    asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(vl));
  }

  // l = l * alpha + colsum
  asm volatile("vle32.v v0, (%0)" ::"r"(lrow) : "memory");
  asm volatile("vfmul.vv v0, v0, v20");
  asm volatile("vfadd.vv v0, v0, v28");
  asm volatile("vse32.v v0, (%0)" ::"r"(lrow) : "memory");

  // O^T *= alpha
  for (unsigned int j = 0; j < DV_SIZE; j++) {
    asm volatile("vle32.v v0, (%0)" ::"r"(ot + j * TQ) : "memory");
    asm volatile("vfmul.vv v0, v0, v20");
    asm volatile("vse32.v v0, (%0)" ::"r"(ot + j * TQ) : "memory");
  }
}

// O^T[j][q] *= 1/l[q] (1 <= l <= T)
static void normalize(void) {
  tf_vsetvl(TQ);
  asm volatile("vle32.v v8, (%0)" ::"r"(lrow) : "memory");
  TF_RECIP("v8", "v12", "v16");
  for (unsigned int j = 0; j < DV_SIZE; j++) {
    asm volatile("vle32.v v0, (%0)" ::"r"(ot + j * TQ) : "memory");
    asm volatile("vfmul.vv v0, v0, v12");
    asm volatile("vse32.v v0, (%0)" ::"r"(ot + j * TQ) : "memory");
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
    OPOPE_L1_ALLOC(qw, uint32_t, (D_SIZE / 2) * TQ);
    OPOPE_L1_ALLOC(kw, uint32_t, NT * (D_SIZE / 2) * BC);
    OPOPE_L1_ALLOC(vw, uint32_t, NT * (BC / 2) * DV_SIZE);
    OPOPE_L1_ALLOC(st, float, BC * TQ);
    OPOPE_L1_ALLOC(pw, uint32_t, (BC / 2) * TQ);
    OPOPE_L1_ALLOC(ot, float, DV_SIZE * TQ);
    OPOPE_L1_ALLOC(mrow, float, TQ);
    OPOPE_L1_ALLOC(lrow, float, TQ);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(qw, fa_qw_dram, (D_SIZE / 2) * TQ * sizeof(uint32_t));
    snrt_dma_start_1d(kw, fa_kw_dram, NT * (D_SIZE / 2) * BC * sizeof(uint32_t));
    snrt_dma_start_1d(vw, fa_vw_dram, NT * (BC / 2) * DV_SIZE * sizeof(uint32_t));
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
      opope_gemm_fp16(kw + t * (D_SIZE / 2) * BC, qw, st, BC, D_SIZE, TQ);

      // Online softmax (Spatz): P^T FP16 packed in pw, O^T rescaled
      softmax_update();

      // O^T += V_t^T * P^T
      opope_gemm_fp16(vw + t * (BC / 2) * DV_SIZE, pw, ot, DV_SIZE, BC, TQ);
    }

    // Final normalisation (Spatz)
    normalize();

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- widening flash attention: %d queries, %d keys (tiles of %d), d=%d, dv=%d -----\n",
           TQ, T_SIZE, BC, D_SIZE, DV_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(ot, (const float *)fa_golden, DV_SIZE * TQ, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
