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

// Author: Bowen Wang, ETH Zurich

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/hp-dqmatmul.c"


// Assume B is vector quantized
__fp16  *a;
__fp16  *b_cb0;
__fp16  *b_cb1;
uint8_t *b_idx0;
uint8_t *b_idx1;
__fp16  *c;


static inline int fp16_check(__fp16 *a, __fp16 *b, uint32_t M, uint32_t N) {
  const __fp16 threshold = 0.15;

  // Absolute value
  __fp16 comp     = 0.0;
  __fp16 comp_acc = 0.0;
  for (uint32_t m=0; m<M; m++){
    for (uint32_t n=0; n<N; n++){
      comp = b[m*N + n] - a[m*N + n];
      if (comp < 0) comp = -comp;
      if (comp > threshold) {
        comp_acc += comp;
        printf("[%d, %d] EXP - %4x, GOT - %4x \n", m, n, *(int16_t *)&a[m*N + n], *(int16_t *)&b[m*N + n]);
      }
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

  unsigned int m_start, m_end;
  unsigned int p_start, p_end;
  unsigned int kernel_size;

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a      =  (__fp16 *)snrt_l1alloc(dq_gemm_l.M     * dq_gemm_l.K     * sizeof(__fp16));
    b_cb0  =  (__fp16 *)snrt_l1alloc(dq_gemm_l.CB0_N * dq_gemm_l.CB0_D * sizeof(__fp16));
    b_cb1  =  (__fp16 *)snrt_l1alloc(dq_gemm_l.CB1_N * dq_gemm_l.CB1_D * sizeof(__fp16));
    b_idx0 = (uint8_t *)snrt_l1alloc(dq_gemm_l.K     * (dq_gemm_l.N / dq_gemm_l.CB0_D) * dq_gemm_l.CB0_IDX_WIDTH); 
    b_idx1 = (uint8_t *)snrt_l1alloc(dq_gemm_l.K     * (dq_gemm_l.N / dq_gemm_l.CB1_D) * dq_gemm_l.CB1_IDX_WIDTH);
    c      =  (__fp16 *)snrt_l1alloc(dq_gemm_l.M * dq_gemm_l.N * sizeof(__fp16));
  }

  if (cid==0) {
    printf("addr - c: 0x%8x\n", c);
  }

  // Reset timer
  timer = (unsigned int)-1;

  if (cid==0) {
    for (uint32_t i=0; i<dq_gemm_l.M * dq_gemm_l.N; i++){
      c[i] = 0;
    }
  }

  // Work over complete P dimension
  p_start = 0;
  p_end   = dq_gemm_l.N;
  m_start = (dq_gemm_l.M / num_cores) * cid;
  m_end   = (dq_gemm_l.M / num_cores) * (cid + 1);

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(a,      gemm_A_dram,      dq_gemm_l.M     * dq_gemm_l.K     * sizeof(__fp16));
    snrt_dma_start_1d(b_cb0,  gemm_B_cb0_dram,  dq_gemm_l.CB0_N * dq_gemm_l.CB0_D * sizeof(__fp16));
    snrt_dma_start_1d(b_cb1,  gemm_B_cb1_dram,  dq_gemm_l.CB1_N * dq_gemm_l.CB1_D * sizeof(__fp16));
    snrt_dma_start_1d(b_idx0, gemm_B_idx0_dram, dq_gemm_l.K     * (dq_gemm_l.N / dq_gemm_l.CB0_D) * dq_gemm_l.CB0_IDX_WIDTH);
    snrt_dma_start_1d(b_idx1, gemm_B_idx1_dram, dq_gemm_l.K     * (dq_gemm_l.N / dq_gemm_l.CB1_D) * dq_gemm_l.CB1_IDX_WIDTH);
    snrt_dma_start_1d(c,      gemm_C_dram,      dq_gemm_l.M     * dq_gemm_l.N     * sizeof(__fp16));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate matmul
  for (unsigned int i = 0; i < measure_iterations; ++i) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    dq_matmul_4xVL(c, a, b_cb0, b_cb1, b_idx0, b_idx1, m_start, m_end, dq_gemm_l.K, dq_gemm_l.N, p_start, p_end);

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    unsigned int timer_temp = timer_end - timer_start;
    if (cid == 0) {
      if (timer_temp < timer) {
        timer = timer_temp;
      }
    }
  }

  // Check and display results
  if (cid == 0) {
    long unsigned int performance =
        1000 * 2 * dq_gemm_l.M * dq_gemm_l.N * dq_gemm_l.K / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE * 4);

    printf("\n----- (%dx%d) hp fmatmul -----\n", dq_gemm_l.M, dq_gemm_l.N);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0) {

    if (fp16_check(gemm_golden, c, dq_gemm_l.M, dq_gemm_l.N)) {
      printf("WRONG!   \n");
    } else {
      printf("CORRECT! \n");
    }
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return 0;
}
