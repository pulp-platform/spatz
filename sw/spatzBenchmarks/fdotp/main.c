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

#if (PREC == 64)
#define T double
#elif (PREC == 32)
#define T float
#elif (PREC == 16)
#define T _Float16
#else
#define T double
#endif

T *a;
T *b;
T *result;

static inline int fp_check(const T a, const T b) {
  const T threshold = 0.001;

  // Absolute value
  T comp = a - b;
  if (comp < 0)
    comp = -comp;

  return comp > threshold;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  const unsigned int dim = dotp_l.M / num_cores;

  // Allocate the matrices
  if (cid == 0) {
    a = (T *)snrt_l1alloc(dotp_l.M * sizeof(T));
    b = (T *)snrt_l1alloc(dotp_l.M * sizeof(T));
    result = (T *)snrt_l1alloc(num_cores * sizeof(T));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, dotp_A_dram, dotp_l.M * sizeof(T));
    snrt_dma_start_1d(b, dotp_B_dram, dotp_l.M * sizeof(T));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate internal pointers
  T *a_int = a + dim * cid;
  T *b_int = b + dim * cid;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();

  // Calculate dotp
  T acc;
  if (sizeof(T) == 8)
    acc = fdotp_v64b(a_int, b_int, dim);
  else if (sizeof(T) == 4)
    acc = fdotp_v32b(a_int, b_int, dim);
  else 
    fdotp_v16b(a_int, b_int, dim);

  result[cid] = acc;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Final reduction
  if (cid == 0) {
    for (unsigned int i = 1; i < num_cores; ++i)
      acc += result[i];
    result[0] = acc;
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;

  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * dotp_l.M / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE * (8 / sizeof(T)));

    printf("\n----- (%d) dp fdotp -----\n", dotp_l.M);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0)
    if (fp_check(result[0], dotp_result)) {
      printf("Error: Result = %f, Golden = %f\n", result[0], dotp_result);
      return -1;
    }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}
