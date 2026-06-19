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

#include DATAHEADER // selected by CMake via -DDATAHEADER="data/data_spmv_<fmt>_N<N>_PW<PW>.h"
#include "data/layer.h"
#include "kernel/sp-SpMV.c" // declares + defines both spmv_ventaglio and spmv_baseline

static float *a;
static float *w;
static uint32_t *nm_index;
static uint32_t *byte_offsets; // baseline-only scratch, harmless when unused
static float *res;
static float *golden;

// fp32 result verification with tolerance.
static int fp32_check(const float *ref, const float *got, uint32_t P) {
  const float threshold = 0.001f;
  float comp_acc = 0.0f;
  for (uint32_t i = 0; i < P; i++) {
    float d = got[i] - ref[i];
    if (d < 0)
      d = -d;
    if (d > threshold) {
      printf("[%u] EXP - %8x, GOT - %8x\n", i, *(int32_t *)&ref[i],
             *(int32_t *)&got[i]);
      comp_acc += d;
    }
  }
  return comp_acc > threshold;
}

int main(void) {
  const unsigned int cid = snrt_cluster_core_idx();

  if (cid == 0) {
    a = (float *)snrt_l1alloc(spmv_l.N * sizeof(float));
    w = (float *)snrt_l1alloc(spmv_l.N * spmv_l.P_W * sizeof(float));
    nm_index =
        (uint32_t *)snrt_l1alloc(spmv_l.NM_INDEX_WORDS * sizeof(uint32_t));
    byte_offsets = (uint32_t *)snrt_l1alloc(spmv_l.P_W * sizeof(uint32_t));
    res = (float *)snrt_l1alloc(spmv_l.P * sizeof(float));
    golden = (float *)snrt_l1alloc(spmv_l.P * sizeof(float));
  }

  if (cid == 0) {
    snrt_dma_start_1d(a, spmv_a_dram, spmv_l.N * sizeof(float));
    snrt_dma_start_1d(w, spmv_w_dram, spmv_l.N * spmv_l.P_W * sizeof(float));
    snrt_dma_start_1d(nm_index, spmv_nm_index_dram,
                      spmv_l.NM_INDEX_WORDS * sizeof(uint32_t));
    snrt_dma_start_1d(golden, spmv_golden_dram, spmv_l.P * sizeof(float));
    snrt_dma_wait_all();

#ifdef USE_BASELINE
    // Baseline accumulates into res via vluxei/vsuxei; start from zero.
    for (uint32_t i = 0; i < spmv_l.P; i++) {
      res[i] = 0.0f;
    }
#endif
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    start_kernel();
#ifdef USE_BASELINE
    spmv_baseline(res, a, w, nm_index, byte_offsets, spmv_l.N, spmv_l.P_W,
                  spmv_l.NM_INDEX_ROW_WORDS, spmv_l.IDX_WIDTH, spmv_l.M_SPARSE,
                  spmv_l.N_SPARSE);
#else
    spmv_ventaglio(res, a, w, nm_index, spmv_l.N, spmv_l.P_W,
                   spmv_l.NM_INDEX_ROW_WORDS, spmv_l.IDX_WIDTH, spmv_l.M_SPARSE,
                   spmv_l.N_SPARSE);
#endif
    stop_kernel();
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    if (fp32_check(golden, res, spmv_l.P))
      printf("WRONG!\n");
    else
      printf("CORRECT!\n");
#ifdef USE_BASELINE
    printf("\n----- (%dx%d) SpMV - baseline (RVV vluxei/vsuxei) -----\n",
           spmv_l.N, spmv_l.P);
#else
    printf("\n----- (%dx%d) SpMV - vfx -----\n", spmv_l.N, spmv_l.P);
#endif
    printf("DONE\n");
  }

  snrt_cluster_hw_barrier();
  return 0;
}
