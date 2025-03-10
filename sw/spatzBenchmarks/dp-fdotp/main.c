// Copyright 2022 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/fdotp.c"

double *a;
double *b;
double *result;

static inline int fp_check(const double a, const double b) {
  const double threshold = 0.00001;

  // Absolute value
  double comp = a - b;
  if (comp < 0)
    comp = -comp;

  return comp > threshold;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  
  #if MEAS_1ITER == 1
  const int measure_iter = 1;
  #else
  const int measure_iter = 3;
  #endif

  #if USE_CACHE == 1
  uint32_t spm_size = 16;
  asm volatile("csrrsi x0, 0xb, 0x1");
  #else
  uint32_t spm_size = 120;
  #endif

  if (cid == 0) {
    // Init the cache
    l1d_init(spm_size);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Reset timer
  unsigned int timer = (unsigned int)-1;
  unsigned int timer_tmp, timer_iter1;

  const unsigned int dim = dotp_l.M / num_cores;

  // Allocate the matrices
  #if USE_CACHE == 1
  if (cid == 0) {
    result = (double *)snrt_l1alloc(num_cores * sizeof(double));
  }

  double *a_int = dotp_A_dram + dim * cid;
  double *b_int = dotp_B_dram + dim * cid;

  #else
  if (cid == 0) {
    a = (double *)snrt_l1alloc(dotp_l.M * sizeof(double));
    b = (double *)snrt_l1alloc(dotp_l.M * sizeof(double));
    result = (double *)snrt_l1alloc(num_cores * sizeof(double));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, dotp_A_dram, dotp_l.M * sizeof(double));
    snrt_dma_start_1d(b, dotp_B_dram, dotp_l.M * sizeof(double));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate internal pointers
  double *a_int = a + dim * cid;
  double *b_int = b + dim * cid;

  #endif


  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  for (int iter = 0; iter < measure_iter; iter ++) {
    // // Start dump
    if (cid == 0)
      start_kernel();

    // Start timer
    if (cid == 0)
      timer_tmp = benchmark_get_cycle();

    // Calculate dotp
    double acc;
    acc = fdotp_v64b_m8_unrl(a_int, b_int, dim);
    result[cid] = acc;

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End timer and check if new best runtime
    if (cid == 0) {
      timer_tmp = benchmark_get_cycle() - timer_tmp;
      timer = (timer < timer_tmp) ? timer : timer_tmp;
      if (iter == 0)
        timer_iter1 = timer;
    }

    // Final reduction
    if (cid == 0) {
      // timer_tmp = benchmark_get_cycle() - timer_tmp;
      for (unsigned int i = 1; i < num_cores; ++i)
        acc += result[i];
      result[0] = acc;
    }

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // // End timer and check if new best runtime
    // if (cid == 0) {
    //   timer_tmp = benchmark_get_cycle() - timer_tmp;
    //   timer = (timer < timer_tmp) ? timer : timer_tmp;
    //   if (iter == 0)
    //     timer_iter1 = timer;
    // }

    snrt_cluster_hw_barrier();
  }

  // Check and display results
  if (cid == 0) {
    write_cyc(timer);
    long unsigned int performance = 1000 * 2 * dotp_l.M / timer;
    long unsigned int utilization = performance / (2 * num_cores * 4);
  #ifdef PRINT_RESULT

    printf("\n----- (%d) dp fdotp -----\n", dotp_l.M);
    printf("The first execution took %u cycles.\n", timer_iter1);
    printf("The best execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  #endif
  }

  if (cid == 0)
    if (fp_check(result[0], dotp_result*measure_iter)) {
    #ifdef PRINT_RESULT
      printf("Error: Result = %f, Golden = %f\n", result[0], dotp_result*measure_iter);
    #endif
      return -1;
    }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();
  set_eoc();

  return 0;
}
