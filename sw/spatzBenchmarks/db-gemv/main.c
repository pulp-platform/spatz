// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Author: Navaneeth Kunhi Purayil, ETH Zurich <nkunhi@iis.ee.ethz.ch>
//         Diyou Shen,              ETH Zurich

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/db-gemv.c"

#if   (PREC == 64)
#define T double
#elif (PREC == 32)
#define T float
#elif (PREC == 16)
#define T __fp16
#else
#define T double
#endif

static void *l1_buf;
static T    *l1_out[2];   // per-core output accumulators

static inline int fp_check(const T *a, const T *b) {
  double comp = (double)*a - (double)*b;
  if (comp < 0) comp = -comp;
  return comp > 0.001;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid       = snrt_cluster_core_idx();
  const unsigned int num_fpu   = sizeof(T) == 8 ? 4 : sizeof(T) == 4 ? 8 : 16;
  const unsigned int M         = gemv_l.M;
  const unsigned int N         = gemv_l.N;
  const unsigned int M_core    = M / num_cores;
  const unsigned int ELEM      = sizeof(T);

  // L1 budget: 120KB usable, 8KB reserved for stack
  const unsigned int L1_BUDGET = (120 - 8) * 1024;

  if (cid == 0)
    l1d_spm_config(120);
  snrt_cluster_hw_barrier();

  // -------------------------------------------------------------------------
  // Pre-compute params (both cores, no sync needed — pure arithmetic)
  // -------------------------------------------------------------------------
  db_gemv_params_t p;
  db_gemv_params_init(&p, M, M_core, N, num_cores, cid, ELEM, L1_BUDGET);

  // -------------------------------------------------------------------------
  // L1 allocation (Core 0 only)
  // -------------------------------------------------------------------------
  if (cid == 0) {
    const unsigned int buf_bytes = db_gemv_l1_buf_bytes(&p);

    l1_buf    = snrt_l1alloc(buf_bytes);
    l1_out[0] = (T *)snrt_l1alloc(p.out_bytes);
    l1_out[1] = (T *)snrt_l1alloc(p.out_bytes);

    if (!l1_buf || !l1_out[0] || !l1_out[1]) {
      printf("ERROR: L1 alloc failed "
             "(buf=%p out0=%p out1=%p chunk_cols=%u)\n",
             l1_buf, (void*)l1_out[0], (void*)l1_out[1], p.chunk_cols);
      snrt_cluster_hw_barrier();
      set_eoc();
      return -1;
    }

    printf("chunk_cols=%u  buf=%u KB\n",
           p.chunk_cols, buf_bytes / 1024);
  }
  snrt_cluster_hw_barrier();

  // Result written directly to gemv_result in DRAM
  T *my_c   = gemv_result + M_core * cid;
  T *my_out = l1_out[cid];

  #if MEAS_1ITER == 1
  const int measure_iter = 1;
  #else
  const int measure_iter = 2;
  #endif

  unsigned int timer      = 0;
  unsigned int timer_best = (unsigned int)-1;
  unsigned int timer_1iter = 0;

  for (int iter = 0; iter < measure_iter; iter++) {

    if (cid == 0 && iter == measure_iter - 1)
      start_kernel();

    if (cid == 0)
      timer = benchmark_get_cycle();

    if (ELEM == 8)
      db_gemv_v64b((const double *)gemv_A_dram, (const double *)gemv_B_dram,
                   (double *)my_c, M_core, N, cid,
                   l1_buf, (double *)my_out, &p);
    else if (ELEM == 4)
      db_gemv_v32b((const float *)gemv_A_dram, (const float *)gemv_B_dram,
                   (float *)my_c, M_core, N, cid,
                   l1_buf, (float *)my_out, &p);
    else
      db_gemv_v16b((const __fp16 *)gemv_A_dram, (const __fp16 *)gemv_B_dram,
                   (__fp16 *)my_c, M_core, N, cid,
                   l1_buf, (__fp16 *)my_out, &p);

    snrt_cluster_hw_barrier();

    if (cid == 0 && iter == measure_iter - 1)
      stop_kernel();

    if (cid == 0) {
      timer = benchmark_get_cycle() - timer;
      if (iter == 0) {
        timer_1iter = timer;
        // for (unsigned int i = 0; i < M; i++) {
        //   if (fp_check(&gemv_result[i], &gemv_result_golden[i]))
        //     printf("Error: ID: %u  Result=%f  Golden=%f\n",
        //            i, (double)gemv_result[i], (double)gemv_result_golden[i]);
        // }
      } else {
        timer_best = (timer_best > timer) ? timer : timer_best;
      }
    }

    snrt_cluster_hw_barrier();
  }

  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * M * N / timer_best;
    long unsigned int utilization = performance / (2 * num_cores * num_fpu);

    printf("\n----- (%u x %u) x (%u x 1) db-gemv -----\n", M, N, N);
    printf("chunk_cols      = %u\n",   p.chunk_cols);
    printf("First iter      = %u cycles\n", timer_1iter);
    printf("Best iter       = %u cycles\n", timer_best);
    printf("Performance     = %ld OP/1000cycle (%ld%%o utilization)\n",
           performance, utilization);
  }

  snrt_cluster_hw_barrier();
  set_eoc();
  return 0;
}
