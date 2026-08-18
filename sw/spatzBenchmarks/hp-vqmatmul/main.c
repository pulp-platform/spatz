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
#include "../hp-vqgemv/kernel/hp-vqgemv.c"

#ifndef VQGEMM_USE_VLXBLK_FUSED
#define VQGEMM_USE_VLXBLK_FUSED 1
#endif

#if VQGEMM_USE_VLXBLK_FUSED
#define VQGEMM_NAME "hp vqgemm vlxblk-fused"
#else
#define VQGEMM_NAME "hp vqgemm rvv-fused"
#endif

// Assume B is vector quantized
__fp16  *a;
__fp16  *b_cb0;
__fp16  *b_cb1;
uint8_t *b_idx0;
uint8_t *b_idx1;
__fp16  *b_scales;
__fp16  *c;


static inline int fp16_check(const __fp16 *a, const __fp16 *b, uint32_t M,
                             uint32_t N) {
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

static inline void
vq_gemm_vlxblk_fused(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                     const __fp16 *b_cb1, const uint8_t *b_idx0,
                     const uint8_t *b_idx1, const __fp16 *b_scales,
                     const unsigned int m_start, const unsigned int m_end,
                     const unsigned int K, const unsigned int N,
                     const unsigned int groups) {
  for (unsigned int m = m_start; m < m_end; ++m) {
    vq_gemv_vlxblk(c + m * N, a + m * K, b_cb0, b_cb1, b_idx0, b_idx1,
                   b_scales, K, N, 0, groups);
  }
}

static inline void
vq_gemm_rvv_fused(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                  const __fp16 *b_cb1, const uint8_t *b_idx0,
                  const uint8_t *b_idx1, const __fp16 *b_scales,
                  const unsigned int m_start, const unsigned int m_end,
                  const unsigned int K, const unsigned int N,
                  const unsigned int groups) {
  for (unsigned int m = m_start; m < m_end; ++m) {
    vq_gemv_rvv(c + m * N, a + m * K, b_cb0, b_cb1, b_idx0, b_idx1, b_scales,
                K, N, 0, groups);
  }
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int measure_iterations = 1;

  unsigned int timer_start, timer_end, timer;

  unsigned int m_start, m_end;
  if (vq_gemm_l.CB0_D != vq_gemm_l.CB1_D)
    return -4;
  if (vq_gemm_l.CB0_D != VQ_BLOCK_LEN) {
    if (cid == 0)
      printf("VQ_BLOCK_LEN=%d does not match data CB_D=%d\n", VQ_BLOCK_LEN,
             vq_gemm_l.CB0_D);
    return -5;
  }

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a      =  (__fp16 *)snrt_l1alloc(vq_gemm_l.M     * vq_gemm_l.K     * sizeof(__fp16));
    b_cb0  =  (__fp16 *)snrt_l1alloc(vq_gemm_l.CB0_N * vq_gemm_l.CB0_D * sizeof(__fp16));
    b_cb1  =  (__fp16 *)snrt_l1alloc(vq_gemm_l.CB1_N * vq_gemm_l.CB1_D * sizeof(__fp16));
    b_idx0 = (uint8_t *)snrt_l1alloc(vq_gemm_l.K     * (vq_gemm_l.N / vq_gemm_l.CB0_D) * vq_gemm_l.CB0_IDX_WIDTH);
    b_idx1 = (uint8_t *)snrt_l1alloc(vq_gemm_l.K     * (vq_gemm_l.N / vq_gemm_l.CB1_D) * vq_gemm_l.CB1_IDX_WIDTH);
    b_scales = (__fp16 *)snrt_l1alloc(vq_gemm_l.K * sizeof(__fp16));
    c      =  (__fp16 *)snrt_l1alloc(vq_gemm_l.M * vq_gemm_l.N * sizeof(__fp16));
  }

  if (cid==0) {
    printf("addr - c: %p\n", (void *)c);
  }

  // Reset timer
  timer = (unsigned int)-1;

  if (cid==0) {
    for (uint32_t i=0; i<vq_gemm_l.M * vq_gemm_l.N; i++){
      c[i] = 0;
    }
  }

  m_start = (vq_gemm_l.M / num_cores) * cid;
  m_end   = (vq_gemm_l.M / num_cores) * (cid + 1);

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(a,      gemm_A_dram,      vq_gemm_l.M     * vq_gemm_l.K     * sizeof(__fp16));
    snrt_dma_start_1d(b_cb0,  gemm_B_cb0_dram,  vq_gemm_l.CB0_N * vq_gemm_l.CB0_D * sizeof(__fp16));
    snrt_dma_start_1d(b_cb1,  gemm_B_cb1_dram,  vq_gemm_l.CB1_N * vq_gemm_l.CB1_D * sizeof(__fp16));
    snrt_dma_start_1d(b_idx0, gemm_B_idx0_dram, vq_gemm_l.K     * (vq_gemm_l.N / vq_gemm_l.CB0_D) * vq_gemm_l.CB0_IDX_WIDTH);
    snrt_dma_start_1d(b_idx1, gemm_B_idx1_dram, vq_gemm_l.K     * (vq_gemm_l.N / vq_gemm_l.CB1_D) * vq_gemm_l.CB1_IDX_WIDTH);
    snrt_dma_start_1d(b_scales, gemm_B_scales_dram, vq_gemm_l.K * sizeof(__fp16));
    snrt_dma_start_1d(c,      gemm_C_dram,      vq_gemm_l.M     * vq_gemm_l.N     * sizeof(__fp16));
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

    const unsigned int groups = vq_gemm_l.N / vq_gemm_l.CB0_D;
#if VQGEMM_USE_VLXBLK_FUSED
    vq_gemm_vlxblk_fused(c, a, b_cb0, b_cb1, b_idx0, b_idx1, b_scales,
                         m_start, m_end, vq_gemm_l.K, vq_gemm_l.N, groups);
#else
    vq_gemm_rvv_fused(c, a, b_cb0, b_cb1, b_idx0, b_idx1, b_scales, m_start,
                      m_end, vq_gemm_l.K, vq_gemm_l.N, groups);
#endif

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
        1000 * 2 * vq_gemm_l.M * vq_gemm_l.N * vq_gemm_l.K / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE * 4);

    printf("\n----- (%dx%d, D=%d) %s -----\n", vq_gemm_l.M, vq_gemm_l.N,
           vq_gemm_l.CB0_D, VQGEMM_NAME);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%% utilization).\n",
           performance, utilization);
  }

  if (cid == 0) {

    if (fp16_check(gemm_golden, c, vq_gemm_l.M, vq_gemm_l.N)) {
      printf("WRONG!   \n");
    } else {
      printf("CORRECT! \n");
    }
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return 0;
}
