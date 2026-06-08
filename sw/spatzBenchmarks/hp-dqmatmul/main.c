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

// ---- Select dequant kernel variant --------------------------------------
// DQ_CASE: 0 = blk32   (2 codebooks, EI8,  CB_D=32) [default / existing]
//          1 = blk4    (1 codebook,  EI8,  CB_D=4 ) [case 1]
//          2 = blk8ei16(2 codebooks, EI16, CB_D=8 ) [case 2]
#ifndef DQ_CASE
#define DQ_CASE 0
#endif

// CB_IN_DRAM: in the no-cache (SPM) build, keep the codebook(s) resident in
// DRAM instead of copying them into L1 SPM. Needed when a codebook is too large
// to fit in L1 (e.g. the 2^16-entry EI16 case = 1 MiB per codebook). A, indices
// and C are still DMA'd into SPM; the codebook is gathered directly from DRAM
// (build with SNRT_LINK=dram). In cache mode (USE_CACHE=1) this knob is unused.
#if DQ_CASE == 1
// #include "kernel/hp-dqmatmul.c"
#include "kernel/hp-dqmatmul-blk4.c"
#define NUM_CB 1
#define IDX_T  uint8_t
#define CB_IN_DRAM 0
#define RUN_KERNEL_NOCACHE()                                                    \
  dq_matmul_4xVL_blk4(c, a, b_cb0, b_idx0, m_start, m_end, dq_gemm_l.K,         \
                      dq_gemm_l.N, p_start, p_end)
#define RUN_KERNEL_CACHE()                                                      \
  dq_matmul_4xVL_dp_blk4(c, a, b_cb0, b_idx0, m_start, m_end, dq_gemm_l.K,      \
                         dq_gemm_l.N, p_start, p_end)
#elif DQ_CASE == 2
#include "kernel/hp-dqmatmul-blk8ei16.c"
#define NUM_CB 2
#define IDX_T  uint16_t
#define CB_IN_DRAM 1
#define RUN_KERNEL_NOCACHE()                                                    \
  dq_matmul_4xVL_blk8ei16(c, a, b_cb0, b_cb1, b_idx0, b_idx1, m_start, m_end,   \
                          dq_gemm_l.K, dq_gemm_l.N, p_start, p_end)
#define RUN_KERNEL_CACHE()                                                      \
  dq_matmul_4xVL_dp_blk8ei16(c, a, b_cb0, b_cb1, b_idx0, b_idx1, m_start,       \
                             m_end, dq_gemm_l.K, dq_gemm_l.N, p_start, p_end)
#else
#include "kernel/hp-dqmatmul-blk32.c"
#define NUM_CB 2
#define IDX_T  uint8_t
#define CB_IN_DRAM 0
#define RUN_KERNEL_NOCACHE()                                                    \
  dq_matmul_4xVL_blk32(c, a, b_cb0, b_cb1, b_idx0, b_idx1, m_start, m_end,      \
                       dq_gemm_l.K, dq_gemm_l.N, p_start, p_end)
#define RUN_KERNEL_CACHE()                                                      \
  dq_matmul_4xVL_dp_blk32(c, a, b_cb0, b_cb1, b_idx0, b_idx1, m_start, m_end,   \
                          dq_gemm_l.K, dq_gemm_l.N, p_start, p_end)
#endif

#ifndef NFPU_PER_CORE
#define NFPU_PER_CORE 4
#endif


// Assume B is vector quantized
__fp16 *a;
__fp16 *b_cb0;
IDX_T  *b_idx0;
#if NUM_CB == 2
__fp16 *b_cb1;
IDX_T  *b_idx1;
#endif
__fp16 *c;


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

  #if USE_CACHE == 0
  uint32_t spm_size = 120;
  #else 
  uint32_t spm_size = 4;
  #endif

  if (cid == 0) {
    // Init the cache
    l1d_init(spm_size);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  #if USE_CACHE == 0
  // Allocate the matrices in the local tile
  if (cid == 0) {
    a      =  (__fp16 *)snrt_l1alloc(dq_gemm_l.M     * dq_gemm_l.K     * sizeof(__fp16));
    b_idx0 = (IDX_T  *)snrt_l1alloc(dq_gemm_l.K     * (dq_gemm_l.N / dq_gemm_l.CB0_D) * dq_gemm_l.CB0_IDX_WIDTH);
    #if NUM_CB == 2
    b_idx1 = (IDX_T  *)snrt_l1alloc(dq_gemm_l.K     * (dq_gemm_l.N / dq_gemm_l.CB1_D) * dq_gemm_l.CB1_IDX_WIDTH);
    #endif
    c      =  (__fp16 *)snrt_l1alloc(dq_gemm_l.M * dq_gemm_l.N * sizeof(__fp16));
    #if CB_IN_DRAM
    // Codebook(s) too large for L1: keep them resident in DRAM (no SPM copy).
    b_cb0  = gemm_B_cb0_dram;
    #if NUM_CB == 2
    b_cb1  = gemm_B_cb1_dram;
    #endif
    #else
    b_cb0  =  (__fp16 *)snrt_l1alloc(dq_gemm_l.CB0_N * dq_gemm_l.CB0_D * sizeof(__fp16));
    #if NUM_CB == 2
    b_cb1  =  (__fp16 *)snrt_l1alloc(dq_gemm_l.CB1_N * dq_gemm_l.CB1_D * sizeof(__fp16));
    #endif
    #endif
  }

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(a,      gemm_A_dram,      dq_gemm_l.M     * dq_gemm_l.K     * sizeof(__fp16));
    snrt_dma_start_1d(b_idx0, gemm_B_idx0_dram, dq_gemm_l.K     * (dq_gemm_l.N / dq_gemm_l.CB0_D) * dq_gemm_l.CB0_IDX_WIDTH);
    #if NUM_CB == 2
    snrt_dma_start_1d(b_idx1, gemm_B_idx1_dram, dq_gemm_l.K     * (dq_gemm_l.N / dq_gemm_l.CB1_D) * dq_gemm_l.CB1_IDX_WIDTH);
    #endif
    #if !CB_IN_DRAM
    snrt_dma_start_1d(b_cb0,  gemm_B_cb0_dram,  dq_gemm_l.CB0_N * dq_gemm_l.CB0_D * sizeof(__fp16));
    #if NUM_CB == 2
    snrt_dma_start_1d(b_cb1,  gemm_B_cb1_dram,  dq_gemm_l.CB1_N * dq_gemm_l.CB1_D * sizeof(__fp16));
    #endif
    #endif
    snrt_dma_start_1d(c,      gemm_C_dram,      dq_gemm_l.M     * dq_gemm_l.N     * sizeof(__fp16));
    snrt_dma_wait_all();
  }

  #else
  a      = gemm_A_dram;
  b_cb0  = gemm_B_cb0_dram;
  b_idx0 = gemm_B_idx0_dram;
  #if NUM_CB == 2
  b_cb1  = gemm_B_cb1_dram;
  b_idx1 = gemm_B_idx1_dram;
  #endif
  c      = gemm_C_dram;
  #endif

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // if (cid==0) {
  //   printf("addr - c: 0x%8x\n", c);
  // }

  // Reset timer
  timer = (unsigned int)-1;

  // Work over complete P dimension
  p_start = 0;
  p_end   = dq_gemm_l.N;
  m_start = (dq_gemm_l.M / num_cores) * cid;
  m_end   = (dq_gemm_l.M / num_cores) * (cid + 1);


  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate matmul
  for (unsigned int i = 0; i < measure_iterations; ++i) {
    // Start timer

    // Start dump
    if (cid == 0) {
      start_kernel();
      timer_start = benchmark_get_cycle();
    }
    #if USE_CACHE == 0
    RUN_KERNEL_NOCACHE();
    #else
    RUN_KERNEL_CACHE();
    #endif

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0) {
      timer_end = benchmark_get_cycle();
      stop_kernel();
    }

    // End timer and check if new best runtime
    if (cid == 0) {
      unsigned int timer_temp = timer_end - timer_start;
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
        performance / (2 * num_cores * NFPU_PER_CORE * 4);

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