// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Fatih Özdemir, ETH Zurich
#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/hp-vqgemv.c"

__fp16 *a;
__fp16 *b_cb0;
__fp16 *b_cb1;
uint8_t *b_idx0;
uint8_t *b_idx1;
__fp16 *b_scales;
__fp16 *c;

#ifndef VQGEMV_USE_VLXBLK
#define VQGEMV_USE_VLXBLK 1
#endif

#if VQGEMV_USE_VLXBLK
#define VQGEMV_NAME "hp vqgemv vlxblk-fused"
#else
#define VQGEMV_NAME "hp vqgemv rvv-fused"
#endif

static inline int fp16_check(const __fp16 *golden, const __fp16 *got,
                             const unsigned int N) {
  const __fp16 threshold = 0.15;
  __fp16 comp_acc = 0.0;

  for (unsigned int n = 0; n < N; ++n) {
    __fp16 comp = got[n] - golden[n];
    if (comp < 0)
      comp = -comp;
    if (comp > threshold) {
      comp_acc += comp;
      printf("[%d] EXP - %4x, GOT - %4x\n", n, *(const int16_t *)&golden[n],
             *(const int16_t *)&got[n]);
    }
  }

  printf("COMP - %8x \n", *(int16_t *)&comp_acc);
  return comp_acc > threshold;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  const unsigned int measure_iterations = 1;

  unsigned int timer_start, timer_end, timer;
  const unsigned int groups = vq_gemm_l.N / vq_gemm_l.CB0_D;
  const unsigned int group_start = (groups * cid) / num_cores;
  const unsigned int group_end = (groups * (cid + 1)) / num_cores;

  if (vq_gemm_l.M != 1)
    return -3;
  if (vq_gemm_l.CB0_D != vq_gemm_l.CB1_D)
    return -4;
  if (vq_gemm_l.CB0_D != VQ_BLOCK_LEN) {
    if (cid == 0)
      printf("VQ_BLOCK_LEN=%d does not match data CB_D=%d\n", VQ_BLOCK_LEN,
             vq_gemm_l.CB0_D);
    return -5;
  }

  if (cid == 0) {
    a = (__fp16 *)snrt_l1alloc(vq_gemm_l.K * sizeof(__fp16));
    b_cb0 =
        (__fp16 *)snrt_l1alloc(vq_gemm_l.CB0_N * vq_gemm_l.CB0_D * sizeof(__fp16));
    b_cb1 =
        (__fp16 *)snrt_l1alloc(vq_gemm_l.CB1_N * vq_gemm_l.CB1_D * sizeof(__fp16));
    b_idx0 = (uint8_t *)snrt_l1alloc(vq_gemm_l.K * groups *
                                     vq_gemm_l.CB0_IDX_WIDTH);
    b_idx1 = (uint8_t *)snrt_l1alloc(vq_gemm_l.K * groups *
                                     vq_gemm_l.CB1_IDX_WIDTH);
    b_scales = (__fp16 *)snrt_l1alloc(vq_gemm_l.K * sizeof(__fp16));
    c = (__fp16 *)snrt_l1alloc(vq_gemm_l.N * sizeof(__fp16));
  }

  timer = (unsigned int)-1;

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    snrt_dma_start_1d(a, gemm_A_dram, vq_gemm_l.K * sizeof(__fp16));
    snrt_dma_start_1d(b_cb0, gemm_B_cb0_dram,
                      vq_gemm_l.CB0_N * vq_gemm_l.CB0_D * sizeof(__fp16));
    snrt_dma_start_1d(b_cb1, gemm_B_cb1_dram,
                      vq_gemm_l.CB1_N * vq_gemm_l.CB1_D * sizeof(__fp16));
    snrt_dma_start_1d(b_idx0, gemm_B_idx0_dram,
                      vq_gemm_l.K * groups * vq_gemm_l.CB0_IDX_WIDTH);
    snrt_dma_start_1d(b_idx1, gemm_B_idx1_dram,
                      vq_gemm_l.K * groups * vq_gemm_l.CB1_IDX_WIDTH);
    snrt_dma_start_1d(b_scales, gemm_B_scales_dram,
                      vq_gemm_l.K * sizeof(__fp16));
    snrt_dma_start_1d(c, gemm_C_dram, vq_gemm_l.N * sizeof(__fp16));
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  for (unsigned int i = 0; i < measure_iterations; ++i) {
    timer_start = benchmark_get_cycle();

    if (cid == 0)
      start_kernel();

#if VQGEMV_USE_VLXBLK
    vq_gemv_vlxblk(c, a, b_cb0, b_cb1, b_idx0, b_idx1, b_scales, vq_gemm_l.K,
                   vq_gemm_l.N, group_start, group_end);
#else
    vq_gemv_rvv(c, a, b_cb0, b_cb1, b_idx0, b_idx1, b_scales, vq_gemm_l.K,
                vq_gemm_l.N, group_start, group_end);
#endif

    snrt_cluster_hw_barrier();

    if (cid == 0)
      stop_kernel();

    timer_end = benchmark_get_cycle();
    unsigned int timer_temp = timer_end - timer_start;
    if ((cid == 0) && (timer_temp < timer))
      timer = timer_temp;
  }

  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * vq_gemm_l.N * vq_gemm_l.K / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE * 4);

    printf("\n----- (%dx%d, D=%d) %s -----\n", vq_gemm_l.K, vq_gemm_l.N,
           vq_gemm_l.CB0_D, VQGEMV_NAME);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%% utilization).\n",
           performance, utilization);

    if (fp16_check(gemm_golden, c, vq_gemm_l.N))
      return -1;
  }

  snrt_cluster_hw_barrier();

  return 0;
}
