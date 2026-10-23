// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Embedding-bag / neighbour aggregation followed by an MLP layer (DLRM, or GCN layer), FP16 inputs and
// FP32 output:
//   pooled[b][d] = scale * sum_l table[idx[b][l]][d]     (Spatz, FP16: linear, no widening needed;
//                                                         scalar row address + unit-stride row loads,
//                                                         no indexed vector ops)
//   Z[o][b] = relu(bias[o] + sum_d W1[d][o] * pooled[b][d])   (O-POPE widening GeMM, then FP32 ReLU)
// The GeMM wants the pooled activations feature-major and packed two features per word (W operand
// [D/2][B], word (p,b) = {pooled[b][2p], pooled[b][2p+1]}). The table rows are stored offline as
// [even features | odd features], so a pooled row is two register groups (even / odd half) that are
// written with two strided 16-bit stores (stride 4*B bytes) straight into the packed operand: the
// layout change costs two stores per bag and needs neither a transposition GeMM nor extra packing.

#include "opope_transformer.h"

#include DATAHEADER

uint16_t *table;
uint32_t *idx;
uint32_t *pool;  // packed W operand [D/2][B]
uint32_t *w1;    // packed X^T operand [D/2][Dout]
float *bias;
float *z;

// pool word column b <- sum over the lookups of the FP16 rows. VLEN = 512: a pooled row (D = 64
// halfwords) is the LMUL=2 group v0-v1, i.e. v0 = even features, v1 = odd features
static void gather_pool(void) {
  unsigned int vl;
  const unsigned int stride = B_SIZE * sizeof(uint32_t);
  for (unsigned int b = 0; b < B_SIZE; b++) {
    asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(D_SIZE));  // D = 64
    asm volatile("vmv.v.i v0, 0");
    for (unsigned int l = 0; l < L_SIZE; l++) {
      const uint16_t *row = table + idx[b * L_SIZE + l] * D_SIZE;
      asm volatile("vle16.v v4, (%0)" ::"r"(row) : "memory");
      asm volatile("vfadd.vv v0, v0, v4");
    }
#if MODE_GCN
    {
      uint16_t sbits = SCALE_F16_BITS;
      _Float16 s = *(_Float16 *)&sbits;
      asm volatile("vfmul.vf v0, v0, %0" ::"f"(s));
    }
#endif
    asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(D_SIZE / 2));
    asm volatile("vsse16.v v0, (%0), %1" ::"r"((uint16_t *)(pool + b)), "r"(stride) : "memory");
    opope_vsync(pool);  // one strided access in flight at a time
    asm volatile("vsse16.v v1, (%0), %1" ::"r"((uint16_t *)(pool + b) + 1), "r"(stride) : "memory");
    opope_vsync(pool);
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(table, uint16_t, R_SIZE * D_SIZE);
    OPOPE_L1_ALLOC(idx, uint32_t, B_SIZE * L_SIZE);
    OPOPE_L1_ALLOC(pool, uint32_t, (D_SIZE / 2) * B_SIZE);
    OPOPE_L1_ALLOC(w1, uint32_t, (D_SIZE / 2) * DOUT_SIZE);
    OPOPE_L1_ALLOC(bias, float, DOUT_SIZE);
    OPOPE_L1_ALLOC(z, float, DOUT_SIZE * B_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(table, gm_table_dram, R_SIZE * D_SIZE * sizeof(uint16_t));
    snrt_dma_start_1d(idx, gm_idx_dram, B_SIZE * L_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(w1, gm_w1_dram, (D_SIZE / 2) * DOUT_SIZE * sizeof(uint32_t));
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

    // Pre-processing (Spatz): gather + pool straight into the packed operand, Y = bias
    gather_pool();
    tf_fill_rows(z, bias, DOUT_SIZE, B_SIZE);

    // GeMM (O-POPE, FP16 -> FP32): Z[Dout][B] = Y + W1^T * pooled^T
    opope_gemm_fp16(w1, pool, z, DOUT_SIZE, D_SIZE, B_SIZE);

    // Post-processing (Spatz, FP32): ReLU
    tf_relu(z, DOUT_SIZE * B_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- widening %s: %d bags x %d lookups, D=%d -> MLP %dx%dx%d -----\n",
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
