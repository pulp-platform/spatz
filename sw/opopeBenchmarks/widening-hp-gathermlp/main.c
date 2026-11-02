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
// [D/2][BP], word (p,b) = {pooled[b][2p], pooled[b][2p+1]}). The table rows are stored in natural feature
// order, so the pooled row (64 halfwords) already is the 32 packed words of column b: one strided 32-bit store per
// bag (stride BP*4 bytes) writes it straight into the operand. The pitch BP is odd on purpose (127 bags): the 32 words of
// a store then fall in 16 different TCDM banks (with a pitch that is a multiple of 16 words they all hit one bank
// and the store is 3x slower). The row byte offsets come from the header, and the offsets of the next bag are loaded
// before the vector accesses of the current bag are issued (a scalar load cannot be issued while vector memory
// accesses are in flight, scalar ALU work can).

#include "opope_tf16.h"

#include DATAHEADER

_Static_assert(L_SIZE == 8, "gather_pool is unrolled for 8 lookups per bag");
_Static_assert(D_SIZE == 64, "the row byte offset is index << 7");

uint16_t *table;
uint32_t *idx;   // four 8-bit row indices per word [B][L/4]
uint32_t *pool;  // packed W operand [D/2][BP]
uint32_t *w1;    // packed X^T operand [D/2][Dout]
float *z;        // [Dout][BP]

// pool word column b <- sum over the lookups of the FP16 rows. VLEN = 512: a pooled row (D = 64 halfwords) is an LMUL=2 group
// (e16) = 32 words (e32). Software pipeline over the bags with two register sets (v0..v15 and v16..v30, one LMUL=2 group per
// lookup): the 8 row loads of bag b+1 are issued interleaved with the adds of bag b (a load, an add, a load, ...), so the vector
// load/store unit and the FP unit work at the same time, and the strided store of bag b is not synchronised. The two index words
// of bag b+1 (four 8-bit row indices per word) are read at the start of the iteration of bag b: the scalar load waits for the
// vector memory accesses in flight (the loads of bag b, which its adds need anyway, and the store of bag b-1), and then the loads
// of bag b+1 and the adds of bag b run together.
#define GP_LOAD(set, l, off) asm volatile("vle16.v v" #set ", (%0)" ::"r"(tb + (off)) : "memory")
#define GP_ADD(acc, r) asm volatile("vfadd.vv v" #acc ", v" #acc ", v" #r)
#define GP_STORE(acc, b)                                                                                                  \
  do {                                                                                                                  \
    asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(D_SIZE / 2));                                       \
    asm volatile("vsse32.v v" #acc ", (%0), %1" ::"r"(pool + (b)), "r"(stride) : "memory");                              \
    asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(D_SIZE));                                            \
  } while (0)
#define OFF(w, k) ((((w) >> (8 * (k))) & 0xffu) << 7)

static void gather_pool(void) {
  unsigned int vl;
  const unsigned int stride = BP_SIZE * sizeof(uint32_t);
  const char *const tb = (const char *)table;
  asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(D_SIZE));
  // prologue: the loads of bag 0 (set A)
  {
    const unsigned int w0 = idx[0], w1 = idx[1];
    GP_LOAD(0, 0, OFF(w0, 0)); GP_LOAD(2, 1, OFF(w0, 1)); GP_LOAD(4, 2, OFF(w0, 2)); GP_LOAD(6, 3, OFF(w0, 3));
    GP_LOAD(8, 4, OFF(w1, 0)); GP_LOAD(10, 5, OFF(w1, 1)); GP_LOAD(12, 6, OFF(w1, 2)); GP_LOAD(14, 7, OFF(w1, 3));
  }
  for (unsigned int b = 0; b < B_SIZE; b++) {
    const unsigned int nb = (b + 1 < B_SIZE) ? b + 1 : b;
    const unsigned int w0 = idx[nb * 2], w1 = idx[nb * 2 + 1];
    if ((b & 1) == 0) {  // bag b accumulates in set A (v0..v14), the loads of bag b+1 go to set B (v16..v30)
      GP_LOAD(16, 0, OFF(w0, 0)); GP_ADD(0, 2);
      GP_LOAD(18, 1, OFF(w0, 1)); GP_ADD(0, 4);
      GP_LOAD(20, 2, OFF(w0, 2)); GP_ADD(0, 6);
      GP_LOAD(22, 3, OFF(w0, 3)); GP_ADD(0, 8);
      GP_LOAD(24, 4, OFF(w1, 0)); GP_ADD(0, 10);
      GP_LOAD(26, 5, OFF(w1, 1)); GP_ADD(0, 12);
      GP_LOAD(28, 6, OFF(w1, 2)); GP_ADD(0, 14);
#if MODE_GCN
      { uint16_t sbits = SCALE_F16_BITS; _Float16 s = *(_Float16 *)&sbits; asm volatile("vfmul.vf v0, v0, %0" ::"f"(s)); }
#endif
      GP_LOAD(30, 7, OFF(w1, 3));
      GP_STORE(0, b);
    } else {
      GP_LOAD(0, 0, OFF(w0, 0)); GP_ADD(16, 18);
      GP_LOAD(2, 1, OFF(w0, 1)); GP_ADD(16, 20);
      GP_LOAD(4, 2, OFF(w0, 2)); GP_ADD(16, 22);
      GP_LOAD(6, 3, OFF(w0, 3)); GP_ADD(16, 24);
      GP_LOAD(8, 4, OFF(w1, 0)); GP_ADD(16, 26);
      GP_LOAD(10, 5, OFF(w1, 1)); GP_ADD(16, 28);
      GP_LOAD(12, 6, OFF(w1, 2)); GP_ADD(16, 30);
#if MODE_GCN
      { uint16_t sbits = SCALE_F16_BITS; _Float16 s = *(_Float16 *)&sbits; asm volatile("vfmul.vf v16, v16, %0" ::"f"(s)); }
#endif
      GP_LOAD(14, 7, OFF(w1, 3));
      GP_STORE(16, b);
    }
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(table, uint16_t, R_SIZE * D_SIZE);
    OPOPE_L1_ALLOC(idx, uint32_t, B_SIZE * (L_SIZE / 4));
    OPOPE_L1_ALLOC(pool, uint32_t, (D_SIZE / 2) * BP_SIZE);
    OPOPE_L1_ALLOC(w1, uint32_t, (D_SIZE / 2) * DOUT_SIZE);
    OPOPE_L1_ALLOC(z, float, DOUT_SIZE * BP_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(table, gm_table_dram, R_SIZE * D_SIZE * sizeof(uint16_t));
    snrt_dma_start_1d(idx, gm_idx_dram, B_SIZE * (L_SIZE / 4) * sizeof(uint32_t));
    snrt_dma_start_1d(w1, gm_w1_dram, (D_SIZE / 2) * DOUT_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(z, gm_zinit_dram, DOUT_SIZE * BP_SIZE * sizeof(float));  // Y = bias broadcast
    opope_dma_zero(pool, (D_SIZE / 2) * BP_SIZE * sizeof(uint32_t));           // dummy bag column
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

    // GeMM (O-POPE, FP16 -> FP32): Z[Dout][B] = Y + W1^T * pooled^T
    opope_gemm_fp16(w1, pool, z, DOUT_SIZE, D_SIZE, BP_SIZE);

    // Post-processing (Spatz, FP32): ReLU
    tf16_relu(z, DOUT_SIZE * BP_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- widening %s: %d bags x %d lookups, D=%d -> MLP %dx%dx%d -----\n",
           MODE_GCN ? "GCN layer" : "DLRM embedding-bag + MLP", B_SIZE, L_SIZE, D_SIZE, DOUT_SIZE,
           D_SIZE, B_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);

    for (unsigned int o = 0; o < DOUT_SIZE; o++)
      errors += opope_check_fp32(z + o * BP_SIZE, (const float *)gm_golden + o * B_SIZE, B_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
