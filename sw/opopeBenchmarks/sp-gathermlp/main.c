// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Embedding-bag / neighbour aggregation followed by an MLP layer (DLRM, or GCN layer):
//   prow[b][d] = scale[b] * sum_l table[idx[b][l]][d]   (Spatz: scalar row address + unit-stride
//                                                         row loads; no indexed/strided vector ops,
//                                                         which are unreliable on this cluster)
//   pool = prow^T                                        (O-POPE job 1: GeMM by the identity)
//   Z[o][b] = relu(bias[o] + sum_d W1[d][o] * pool[d][b])  (O-POPE job 2, then Spatz ReLU)
// Layout: the MLP GeMM wants the pooled activations feature-major pool[D][B] (W operand, K = B),
// but the gather naturally produces one row per bag, so the transposition is done by O-POPE (the
// row-per-bag matrix is exactly the transposed X operand of a GeMM with the identity).
// The weights W1[D][Dout] ("in x out") are stored directly as the transposed X operand and the
// output is feature-major Z[Dout][B] (M = Dout, N = D, K = B).

#include "opope_app.h"

#include DATAHEADER

float *table;
uint32_t *idx;
float *scale;
float *w1t;
float *bias;
float *prow;   // pooled embeddings, one row per bag: [B][D]
float *ident;  // identity [B][B], used to transpose prow with O-POPE
float *pool;   // pooled embeddings, feature-major: [D][B]
float *z;

// prow[b][0..D) = scale[b] * sum_l table[idx[b][l]][0..D)   (unit-stride row loads)
static void gather_pool(void) {
  unsigned int vl;
  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(D_SIZE));  // D <= 128
  for (unsigned int b = 0; b < B_SIZE; b++) {
    asm volatile("vmv.v.i v0, 0");
    for (unsigned int l = 0; l < L_SIZE; l++) {
      const float *row = table + idx[b * L_SIZE + l] * D_SIZE;
      asm volatile("vle32.v v8, (%0)" ::"r"(row) : "memory");
      asm volatile("vfadd.vv v0, v0, v8");
    }
#if MODE_GCN
    asm volatile("vfmul.vf v0, v0, %0" ::"f"(scale[b]));
#endif
    asm volatile("vse32.v v0, (%0)" ::"r"(prow + b * D_SIZE) : "memory");
  }
}

// ident = identity matrix [n][n]
static void make_identity(float *m, unsigned int n) {
  opope_vzero(m, n * n);
  for (unsigned int i = 0; i < n; i++) m[i * n + i] = 1.0f;
}

// Y[o][0..B) = bias[o]
static void bias_rows(float *y, const float *b, unsigned int rows, unsigned int len) {
  unsigned int vl;
  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(len));
  for (unsigned int o = 0; o < rows; o++) {
    asm volatile("vfmv.v.f v0, %0" ::"f"(b[o]));
    asm volatile("vse32.v v0, (%0)" ::"r"(y + o * len) : "memory");
  }
}

// zz[0..n) = max(zz, 0)
static void relu(float *zz, unsigned int n) {
  unsigned int vl;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vle32.v v0, (%0)" ::"r"(zz) : "memory");
    asm volatile("vfmax.vf v0, v0, %0" ::"f"(0.0f));
    asm volatile("vse32.v v0, (%0)" ::"r"(zz) : "memory");
    zz += vl;
    n -= vl;
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(table, float, R_SIZE * D_SIZE);
    OPOPE_L1_ALLOC(idx, uint32_t, B_SIZE * L_SIZE);
    OPOPE_L1_ALLOC(scale, float, B_SIZE);
    OPOPE_L1_ALLOC(w1t, float, D_SIZE * DOUT_SIZE);
    OPOPE_L1_ALLOC(bias, float, DOUT_SIZE);
    OPOPE_L1_ALLOC(prow, float, B_SIZE * D_SIZE);
    OPOPE_L1_ALLOC(ident, float, B_SIZE * B_SIZE);
    OPOPE_L1_ALLOC(pool, float, D_SIZE * B_SIZE);
    OPOPE_L1_ALLOC(z, float, DOUT_SIZE * B_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(table, gm_table_dram, R_SIZE * D_SIZE * sizeof(float));
    snrt_dma_start_1d(idx, gm_idx_dram, B_SIZE * L_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(scale, gm_scale_dram, B_SIZE * sizeof(float));
    snrt_dma_start_1d(w1t, gm_w1t_dram, D_SIZE * DOUT_SIZE * sizeof(float));
    snrt_dma_start_1d(bias, gm_bias_dram, DOUT_SIZE * sizeof(float));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // Pre-processing (Spatz): gather + pool (row per bag), identity, Y = 0 / bias
    gather_pool();
    make_identity(ident, B_SIZE);
    opope_vzero(pool, D_SIZE * B_SIZE);
    bias_rows(z, bias, DOUT_SIZE, B_SIZE);

    // O-POPE job 1: layout change prow[B][D] -> pool[D][B] (prow is read as the transposed X
    // operand, N=B, M=D; multiplying by the identity transposes it exactly)
    opope_gemm(prow, ident, pool, D_SIZE, B_SIZE, B_SIZE);

    // O-POPE job 2: Z[Dout][B] = Y + W1^T * pool
    opope_gemm(w1t, pool, z, DOUT_SIZE, D_SIZE, B_SIZE);

    // Post-processing (Spatz): ReLU
    relu(z, DOUT_SIZE * B_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- %s: %d bags x %d lookups, D=%d -> MLP %dx%dx%d -----\n",
           MODE_GCN ? "GCN layer" : "DLRM embedding-bag + MLP", B_SIZE, L_SIZE, D_SIZE, DOUT_SIZE,
           D_SIZE, B_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)gm_golden, DOUT_SIZE * B_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
