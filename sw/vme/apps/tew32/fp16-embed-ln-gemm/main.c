// Copyright 2025-2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// P10: embedding -> LayerNorm -> GeMM, VME (Spatz VFU + OPE). Derived from
// baseline_spatz/sw/opopeBenchmarks/widening-hp-tf-p10-embed-ln-gemm/main.c: same input values and placement
// outside the measured region, same check after it; only the measured region differs.
// The embedding lookup is a GeMM: emb^T[C][T] = wpe^T (Y) + wte^T[C][V] * onehot[V][T] (one-hot built in the
// measured region); LayerNorm statistics -> FP16 operand; z = bias' + W'^T * LN(emb^T).

#include "opope_tf16.h"

#include DATAHEADER

#include "kernel/p10_gemms.c"
#include "kernel/ln_pack.c"

static uint32_t *tok;
static uint32_t *wte;
static uint32_t *onehot;
static float *emb;
static uint32_t *lnpk;
static uint32_t *wpk;
static uint32_t *bv;
static float *z;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  // Allocate everything in the local tile
  if (cid == 0) {
    OPOPE_L1_ALLOC(tok, uint32_t, T_SIZE);
    OPOPE_L1_ALLOC(wte, uint32_t, sizeof(d_wte_a) / 4);
    OPOPE_L1_ALLOC(onehot, uint32_t, (V_SIZE / 2) * T_SIZE + 64);
    OPOPE_L1_ALLOC(emb, float, C_SIZE * T_SIZE);
    OPOPE_L1_ALLOC(lnpk, uint32_t, (C_SIZE / 2) * T_SIZE + 64);
    OPOPE_L1_ALLOC(wpk, uint32_t, sizeof(d_w_a) / 4);
    OPOPE_L1_ALLOC(bv, uint32_t, sizeof(d_bv) / 4);
    OPOPE_L1_ALLOC(z, float, COUT_SIZE * T_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data (placement of the inputs, outside of the measured region)
  if (cid == 0) {
    TF_DMA(tok, d_tok);
    TF_DMA(wte, d_wte_a);
    TF_DMA(emb, d_wpet);  // Y of the embedding GeMM = positional embeddings (feature-major)
    TF_DMA(wpk, d_w_a);
    TF_DMA(bv, d_bv);     // bias' (one extra K-pair of the projection GeMM)
    snrt_dma_wait_all();
    for (unsigned int i = 0; i < (V_SIZE / 2) * T_SIZE; i++) onehot[i] = 0;
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // one-hot B panels [T/16][V/2][16]: token t, vocabulary entry v -> K-pair v/2, half (v even: high)
    // halfword of token t, entry v: byte (t/16)*V/2*64 + (t%16)*4 + 2 + (v>>1)*64 - (v&1)*2 (8 instructions/token)
    {
      const uint32_t *tp = tok;
      uint8_t *base = (uint8_t *)onehot + 2;
      for (unsigned int tile = 0; tile < T_SIZE / 16; tile++, base += (V_SIZE / 2) * 64) {
        uint8_t *b = base;
        for (unsigned int k = 0; k < 4; k++, tp += 4, b += 16) {
          asm volatile(
              "lw t0, 0(%0)\n lw t1, 4(%0)\n lw t2, 8(%0)\n lw t3, 12(%0)\n"
              "srli t4, t0, 1\n slli t4, t4, 6\n andi t0, t0, 1\n slli t0, t0, 1\n sub t4, t4, t0\n add t4, t4, %1\n sh %2, 0(t4)\n"
              "srli t4, t1, 1\n slli t4, t4, 6\n andi t1, t1, 1\n slli t1, t1, 1\n sub t4, t4, t1\n add t4, t4, %1\n sh %2, 4(t4)\n"
              "srli t4, t2, 1\n slli t4, t4, 6\n andi t2, t2, 1\n slli t2, t2, 1\n sub t4, t4, t2\n add t4, t4, %1\n sh %2, 8(t4)\n"
              "srli t4, t3, 1\n slli t4, t4, 6\n andi t3, t3, 1\n slli t3, t3, 1\n sub t4, t4, t3\n add t4, t4, %1\n sh %2, 12(t4)\n"
              :: "r"(tp), "r"(b), "r"(0x3C00u) : "t0", "t1", "t2", "t3", "t4", "memory");
        }
      }
    }
    p10_emb_gemm(wte, onehot, emb, emb);
    vme_ln_pack(lnpk, emb);
    p10_prj_gemm(wpk, lnpk, z, bv);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    PRINTF("\n----- P10: embedding -> LayerNorm -> GeMM, VME -----\n");
    PRINTF("The execution took %u cycles.\n", timer);

    errors = opope_check_fp32(z, (const float *)d_golden, COUT_SIZE * T_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
    else PRINTF("PASS\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
