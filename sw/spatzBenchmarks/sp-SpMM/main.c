// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

// Compile with -DUSE_BASELINE to build the RVV-only baseline kernel
// (vluxei/vfmacc/vsuxei). Without the flag, builds the Ventaglio
// (vfx) kernel that uses the VTL bank.

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER // selected by CMake via -DDATAHEADER="data/data_spmm_<fmt>_M<M>_N<N>_PW<PW>.h"
#include "data/layer.h"
#include "kernel/sp-SpMM.c" // declares + defines both spmm_ventaglio and spmm_baseline

static float *a;
static float *w;
static uint32_t *nm_index;
static uint32_t *byte_offsets; // baseline-only scratch, harmless when unused
static float *res;
static float *golden;

// fp32 result verification with tolerance.
static int fp32_check(const float *ref, const float *got, uint32_t M,
                      uint32_t P) {
  const float threshold = 0.001f;
  float comp_acc = 0.0f;
  for (uint32_t i = 0; i < M; i++) {
    for (uint32_t p = 0; p < P; p++) {
      float d = got[i * P + p] - ref[i * P + p];
      if (d < 0)
        d = -d;
      if (d > threshold) {
        printf("[%u, %u] EXP - %8x, GOT - %8x\n", i, p,
               *(int32_t *)&ref[i * P + p], *(int32_t *)&got[i * P + p]);
        comp_acc += d;
      }
    }
  }
  printf("COMP - %8x\n", *(int32_t *)&comp_acc);
  return comp_acc > threshold;
}

int main(void) {
  const unsigned int cid = snrt_cluster_core_idx();

  if (cid == 0) {
    a = (float *)snrt_l1alloc(spmm_l.M * spmm_l.N * sizeof(float));
    w = (float *)snrt_l1alloc(spmm_l.N * spmm_l.P_W * sizeof(float));
    nm_index =
        (uint32_t *)snrt_l1alloc(spmm_l.NM_INDEX_WORDS * sizeof(uint32_t));
    byte_offsets = (uint32_t *)snrt_l1alloc(spmm_l.P_W * sizeof(uint32_t));
    res = (float *)snrt_l1alloc(spmm_l.M * spmm_l.P * sizeof(float));
    golden = (float *)snrt_l1alloc(spmm_l.M * spmm_l.P * sizeof(float));
  }

  if (cid == 0) {
    snrt_dma_start_1d(a, spmm_a_dram, spmm_l.M * spmm_l.N * sizeof(float));
    snrt_dma_start_1d(w, spmm_w_dram, spmm_l.N * spmm_l.P_W * sizeof(float));
    snrt_dma_start_1d(nm_index, spmm_nm_index_dram,
                      spmm_l.NM_INDEX_WORDS * sizeof(uint32_t));
    snrt_dma_start_1d(golden, spmm_golden_dram,
                      spmm_l.M * spmm_l.P * sizeof(float));
    snrt_dma_wait_all();

#ifdef USE_BASELINE
    // Baseline accumulates into res via vluxei/vsuxei; start from zero.
    for (uint32_t i = 0; i < spmm_l.M * spmm_l.P; i++) {
      res[i] = 0.0f;
    }
#else
    // vfx kernel: pre-fill with 0xCAFEBABE so any position the kernel
    // fails to write shows up as 0xCAFEBABE in the mismatch print.
    for (uint32_t i = 0; i < spmm_l.M * spmm_l.P; i++) {
      *(uint32_t *)&res[i] = 0xCAFEBABEu;
    }
#endif
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    start_kernel();
#ifdef USE_BASELINE
    spmm_baseline(res, a, w, nm_index, byte_offsets, spmm_l.M, spmm_l.N,
                  spmm_l.P, spmm_l.P_W, spmm_l.NM_INDEX_ROW_WORDS,
                  spmm_l.IDX_WIDTH, spmm_l.M_SPARSE, spmm_l.N_SPARSE);
#else
    spmm_ventaglio(res, a, w, nm_index, spmm_l.M, spmm_l.N, spmm_l.P,
                   spmm_l.P_W, spmm_l.NM_INDEX_ROW_WORDS, spmm_l.IDX_WIDTH,
                   spmm_l.M_SPARSE, spmm_l.N_SPARSE);
#endif
    stop_kernel();
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    if (fp32_check(golden, res, spmm_l.M, spmm_l.P))
      printf("WRONG!\n");
    else
      printf("CORRECT!\n");
#ifdef USE_BASELINE
    printf("\n----- (%dx%dx%d) SpMM - baseline (RVV vluxei/vsuxei) -----\n",
           spmm_l.M, spmm_l.N, spmm_l.P);
#else
    printf("\n----- (%dx%d) SpMM - vfx -----\n", spmm_l.N, spmm_l.P);
#endif
    printf("DONE\n");
  }

  snrt_cluster_hw_barrier();
  return 0;
}
