// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/hp-vqdotp.c"

#ifndef VQDOTP_NAME
#define VQDOTP_NAME "hp vqdotp rvv-fused"
#endif

#ifndef VQDOTP_SINGLE_CORE
#define VQDOTP_SINGLE_CORE 0
#endif

#ifndef VQDOTP_PRINT_MISMATCHES
#define VQDOTP_PRINT_MISMATCHES 1
#endif

#ifndef VQDOTP_USE_SW_BARRIER
#define VQDOTP_USE_SW_BARRIER 0
#endif

#if VQDOTP_USE_SW_BARRIER
#define VQDOTP_BARRIER() snrt_cluster_sw_barrier()
#else
#define VQDOTP_BARRIER() snrt_cluster_hw_barrier()
#endif

__fp16 *a;
__fp16 *b_cb0;
__fp16 *b_cb1;
uint8_t *b_idx0;
uint8_t *b_idx1;
__fp16 *b_scales;
__fp16 *c;
volatile int vqdotp_error;

#if VQDOTP_PROFILE_REDUCE
volatile unsigned int vqdotp_reduce_cycles;
volatile unsigned int vqdotp_reduce_count;
volatile unsigned int vqdotp_reduce_timer_overhead;
#endif

static inline void dma_copy_wait(void *dst, const void *src,
                                 const unsigned int size) {
  snrt_dma_start_1d(dst, src, size);
  snrt_dma_wait_all();
}

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
#if VQDOTP_PRINT_MISMATCHES
      printf("[%d] EXP - %4x, GOT - %4x\n", n, *(const int16_t *)&golden[n],
             *(const int16_t *)&got[n]);
#endif
    }
  }

  printf("COMP - %8x \n", *(int16_t *)&comp_acc);
  return comp_acc > threshold;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  const unsigned int worker_cores = VQDOTP_SINGLE_CORE ? 1 : num_cores;
  const unsigned int measure_iterations = 1;

  unsigned int timer_start, timer_end, timer;
  const unsigned int kblocks = vq_gemm_l.K / vq_gemm_l.CB0_D;
  const unsigned int n_start =
      (cid < worker_cores) ? (vq_gemm_l.N * cid) / worker_cores : 0;
  const unsigned int n_end =
      (cid < worker_cores) ? (vq_gemm_l.N * (cid + 1)) / worker_cores : 0;

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
#if !defined(VQ_LAYOUT) || !defined(VQ_LAYOUT_KBLK) ||                         \
    (VQ_LAYOUT != VQ_LAYOUT_KBLK)
  if (cid == 0)
    printf("hp-vqdotp requires VQ_LAYOUT=kblk data\n");
  return -6;
#endif
  if ((vq_gemm_l.K % vq_gemm_l.CB0_D) != 0)
    return -7;

  if (cid == 0) {
    a = (__fp16 *)snrt_l1alloc(vq_gemm_l.K * sizeof(__fp16));
    b_cb0 =
        (__fp16 *)snrt_l1alloc(vq_gemm_l.CB0_N * vq_gemm_l.CB0_D * sizeof(__fp16));
    b_cb1 =
        (__fp16 *)snrt_l1alloc(vq_gemm_l.CB1_N * vq_gemm_l.CB1_D * sizeof(__fp16));
    b_idx0 = (uint8_t *)snrt_l1alloc(vq_gemm_l.N * kblocks *
                                     vq_gemm_l.CB0_IDX_WIDTH);
    b_idx1 = (uint8_t *)snrt_l1alloc(vq_gemm_l.N * kblocks *
                                     vq_gemm_l.CB1_IDX_WIDTH);
    b_scales = (__fp16 *)snrt_l1alloc(vq_gemm_l.N * sizeof(__fp16));
    c = (__fp16 *)snrt_l1alloc(vq_gemm_l.N * sizeof(__fp16));
  }

  timer = (unsigned int)-1;

  VQDOTP_BARRIER();

  if (cid == 0) {
    dma_copy_wait(a, gemm_A_dram, vq_gemm_l.K * sizeof(__fp16));
    dma_copy_wait(b_cb0, gemm_B_cb0_dram,
                  vq_gemm_l.CB0_N * vq_gemm_l.CB0_D * sizeof(__fp16));
    dma_copy_wait(b_cb1, gemm_B_cb1_dram,
                  vq_gemm_l.CB1_N * vq_gemm_l.CB1_D * sizeof(__fp16));
    dma_copy_wait(b_idx0, gemm_B_idx0_dram,
                  vq_gemm_l.N * kblocks * vq_gemm_l.CB0_IDX_WIDTH);
    dma_copy_wait(b_idx1, gemm_B_idx1_dram,
                  vq_gemm_l.N * kblocks * vq_gemm_l.CB1_IDX_WIDTH);
    dma_copy_wait(b_scales, gemm_B_scales_dram, vq_gemm_l.N * sizeof(__fp16));
    for (unsigned int n = 0; n < vq_gemm_l.N; ++n)
      c[n] = 0.0;
  }

  VQDOTP_BARRIER();

#if VQDOTP_PROFILE_REDUCE
  if (cid == 0) {
    vqdotp_reduce_cycles = 0;
    vqdotp_reduce_count = 0;
    vqdotp_reduce_timer_overhead = 0;
  }
  VQDOTP_BARRIER();
#endif

  for (unsigned int i = 0; i < measure_iterations; ++i) {
    timer_start = benchmark_get_cycle();

    if (cid == 0)
      start_kernel();

#if VQDOTP_USE_VLXBLK
    vqdotp_vlxblk(c, a, b_cb0, b_cb1, b_idx0, b_idx1, b_scales, vq_gemm_l.K,
                  vq_gemm_l.N, n_start, n_end);
#else
    vqdotp_rvv(c, a, b_cb0, b_cb1, b_idx0, b_idx1, b_scales, vq_gemm_l.K,
               vq_gemm_l.N, n_start, n_end);
#endif

    VQDOTP_BARRIER();

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
        performance / (2 * worker_cores * SNRT_NFPU_PER_CORE * 4);

    printf("\n----- (%dx%d, D=%d) %s -----\n", vq_gemm_l.K, vq_gemm_l.N,
           vq_gemm_l.CB0_D, VQDOTP_NAME);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%% utilization).\n",
           performance, utilization);
#if VQDOTP_PROFILE_REDUCE
    printf("REDUCE cycles total: %u\n", vqdotp_reduce_cycles);
    printf("REDUCE timer overhead: %u\n", vqdotp_reduce_timer_overhead);
    printf("REDUCE count: %u\n", vqdotp_reduce_count);
    if (vqdotp_reduce_count != 0) {
      printf("REDUCE cycles/reduction: %u\n",
             vqdotp_reduce_cycles / vqdotp_reduce_count);
      printf("REDUCE adjusted cycles/reduction: %u\n",
             (vqdotp_reduce_cycles - vqdotp_reduce_timer_overhead) /
                 vqdotp_reduce_count);
    }
#endif

    if (fp16_check(gemm_golden, c, vq_gemm_l.N))
      vqdotp_error = -1;
    else
      printf("CORRECT! \n");
  }

  VQDOTP_BARRIER();

  return vqdotp_error;
}
