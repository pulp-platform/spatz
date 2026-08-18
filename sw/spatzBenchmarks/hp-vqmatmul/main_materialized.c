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
#include <stdio.h>

#include DATAHEADER
#include "kernel/hp-vqmatmul-materialized.c"

__fp16 *a;
__fp16 *b_cb0;
__fp16 *b_cb1;
uint8_t *b_idx0;
uint8_t *b_idx1;
__fp16 *b_scales;
__fp16 *b;
__fp16 *c;

// Dequantization variant selection (3-way):
//   0 = rvv    (unit-stride vle16 per block, one m1 register per block)
//   1 = vlxblk (custom indexed block loads)
//   2 = vle    (scalar loop of plain vle16 loads; consecutive destination
//               registers pack full-register blocks, vslideup chains pack
//               sub-register blocks -- large-block ablation)
// The legacy VQ_MATERIALIZED_USE_VLXBLK=0/1 definition maps onto the first
// two variants for backwards compatibility.
#ifndef VQ_DEQUANT_VARIANT
#ifdef VQ_MATERIALIZED_USE_VLXBLK
#if VQ_MATERIALIZED_USE_VLXBLK
#define VQ_DEQUANT_VARIANT 1
#else
#define VQ_DEQUANT_VARIANT 0
#endif
#else
#define VQ_DEQUANT_VARIANT 1
#endif
#endif

#ifdef VQ_MATERIALIZED_IS_GEMV
#define VQ_MATERIALIZED_OP_NAME "hp vqgemv"
#else
#define VQ_MATERIALIZED_OP_NAME "hp vqgemm"
#endif

#if VQ_DEQUANT_VARIANT == 1
#define VQ_MATERIALIZED_NAME VQ_MATERIALIZED_OP_NAME " vlxblk-materialized"
#elif VQ_DEQUANT_VARIANT == 2
#define VQ_MATERIALIZED_NAME VQ_MATERIALIZED_OP_NAME " vle-materialized"
#else
#define VQ_MATERIALIZED_NAME VQ_MATERIALIZED_OP_NAME " rvv-materialized"
#endif

static inline int fp16_check(const __fp16 *a, const __fp16 *b, uint32_t M,
                             uint32_t N) {
  const __fp16 threshold = 0.15;

  __fp16 comp = 0.0;
  __fp16 comp_acc = 0.0;
  for (uint32_t m = 0; m < M; m++) {
    for (uint32_t n = 0; n < N; n++) {
      comp = b[m * N + n] - a[m * N + n];
      if (comp < 0)
        comp = -comp;
      if (comp > threshold) {
        comp_acc += comp;
        printf("[%d, %d] EXP - %4x, GOT - %4x \n", m, n,
               *(int16_t *)&a[m * N + n], *(int16_t *)&b[m * N + n]);
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
  unsigned int timer_dequant_temp, timer_dequant;

#ifdef VQ_MATERIALIZED_IS_GEMV
  const unsigned int p_start = (vq_gemm_l.N * cid) / num_cores;
  const unsigned int p_end = (vq_gemm_l.N * (cid + 1)) / num_cores;
  const unsigned int m_start = 0;
  const unsigned int m_end = 1;
#else
  const unsigned int p_start = 0;
  const unsigned int p_end = vq_gemm_l.N;
  const unsigned int m_start = (vq_gemm_l.M * cid) / num_cores;
  const unsigned int m_end = (vq_gemm_l.M * (cid + 1)) / num_cores;
#endif
  const unsigned int k_start = (vq_gemm_l.K * cid) / num_cores;
  const unsigned int k_end = (vq_gemm_l.K * (cid + 1)) / num_cores;

  if (vq_gemm_l.CB0_D != vq_gemm_l.CB1_D)
    return -4;
  if (vq_gemm_l.CB0_D != VQ_BLOCK_LEN) {
    if (cid == 0)
      printf("VQ_BLOCK_LEN=%d does not match data CB_D=%d\n", VQ_BLOCK_LEN,
             vq_gemm_l.CB0_D);
    return -5;
  }

  if (cid == 0) {
    a = (__fp16 *)snrt_l1alloc(vq_gemm_l.M * vq_gemm_l.K * sizeof(__fp16));
    b_cb0 =
        (__fp16 *)snrt_l1alloc(vq_gemm_l.CB0_N * vq_gemm_l.CB0_D * sizeof(__fp16));
    b_cb1 =
        (__fp16 *)snrt_l1alloc(vq_gemm_l.CB1_N * vq_gemm_l.CB1_D * sizeof(__fp16));
    b_idx0 = (uint8_t *)snrt_l1alloc(vq_gemm_l.K * (vq_gemm_l.N / vq_gemm_l.CB0_D) *
                                     vq_gemm_l.CB0_IDX_WIDTH);
    b_idx1 = (uint8_t *)snrt_l1alloc(vq_gemm_l.K * (vq_gemm_l.N / vq_gemm_l.CB1_D) *
                                     vq_gemm_l.CB1_IDX_WIDTH);
    b_scales = (__fp16 *)snrt_l1alloc(vq_gemm_l.K * sizeof(__fp16));
    b = (__fp16 *)snrt_l1alloc(vq_gemm_l.K * vq_gemm_l.N * sizeof(__fp16));
    c = (__fp16 *)snrt_l1alloc(vq_gemm_l.M * vq_gemm_l.N * sizeof(__fp16));
  }

  timer = (unsigned int)-1;
  timer_dequant = (unsigned int)-1;

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    snrt_dma_start_1d(a, gemm_A_dram, vq_gemm_l.M * vq_gemm_l.K * sizeof(__fp16));
    snrt_dma_start_1d(b_cb0, gemm_B_cb0_dram,
                      vq_gemm_l.CB0_N * vq_gemm_l.CB0_D * sizeof(__fp16));
    snrt_dma_start_1d(b_cb1, gemm_B_cb1_dram,
                      vq_gemm_l.CB1_N * vq_gemm_l.CB1_D * sizeof(__fp16));
    snrt_dma_start_1d(
        b_idx0, gemm_B_idx0_dram,
        vq_gemm_l.K * (vq_gemm_l.N / vq_gemm_l.CB0_D) * vq_gemm_l.CB0_IDX_WIDTH);
    snrt_dma_start_1d(
        b_idx1, gemm_B_idx1_dram,
        vq_gemm_l.K * (vq_gemm_l.N / vq_gemm_l.CB1_D) * vq_gemm_l.CB1_IDX_WIDTH);
    snrt_dma_start_1d(b_scales, gemm_B_scales_dram,
                      vq_gemm_l.K * sizeof(__fp16));
    snrt_dma_start_1d(c, gemm_C_dram, vq_gemm_l.M * vq_gemm_l.N * sizeof(__fp16));
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  for (unsigned int i = 0; i < measure_iterations; ++i) {
    timer_start = benchmark_get_cycle();

    if (cid == 0)
      start_kernel();

#if VQ_DEQUANT_VARIANT == 1
    vq_dequantize_vlxblk_materialized(b, b_cb0, b_cb1, b_idx0, b_idx1, b_scales,
                                      k_start, k_end, vq_gemm_l.N);
#elif VQ_DEQUANT_VARIANT == 2
    vq_dequantize_vle_materialized(b, b_cb0, b_cb1, b_idx0, b_idx1, b_scales,
                                   k_start, k_end, vq_gemm_l.N);
#else
    vq_dequantize_rvv_materialized(b, b_cb0, b_cb1, b_idx0, b_idx1,
                                        b_scales, k_start, k_end, vq_gemm_l.N);
#endif

    snrt_cluster_hw_barrier();

    timer_dequant_temp = benchmark_get_cycle() - timer_start;

#ifdef VQ_MATERIALIZED_IS_GEMV
    vq_dense_gemv_materialized(c, a, b, vq_gemm_l.K, vq_gemm_l.N, p_start,
                               p_end);
#else
    vq_dense_matmul_materialized(c, a, b, m_start, m_end, vq_gemm_l.K,
                                 vq_gemm_l.N, p_start, p_end);
#endif

    snrt_cluster_hw_barrier();

    if (cid == 0)
      stop_kernel();

    timer_end = benchmark_get_cycle();
    unsigned int timer_temp = timer_end - timer_start;
    if ((cid == 0) && (timer_temp < timer)) {
      timer = timer_temp;
      timer_dequant = timer_dequant_temp;
    }
  }

  if (cid == 0) {
    long unsigned int performance =
        1000 * 2 * vq_gemm_l.M * vq_gemm_l.N * vq_gemm_l.K / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE * 4);

#ifdef VQ_MATERIALIZED_IS_GEMV
    printf("\n----- (%dx%d, D=%d) %s -----\n", vq_gemm_l.K, vq_gemm_l.N,
           vq_gemm_l.CB0_D, VQ_MATERIALIZED_NAME);
#else
    printf("\n----- (%dx%d, D=%d) %s -----\n", vq_gemm_l.M, vq_gemm_l.N,
           vq_gemm_l.CB0_D, VQ_MATERIALIZED_NAME);
#endif
    printf("The execution took %u cycles.\n", timer);
    printf("The dequantization took %u cycles (matmul: %u cycles).\n",
           timer_dequant, timer - timer_dequant);
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

  snrt_cluster_hw_barrier();

  return 0;
}
