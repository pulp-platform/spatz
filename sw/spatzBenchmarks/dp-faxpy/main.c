// Copyright 2023 ETH Zurich and University of Bologna.
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

// Author: Matheus Cavalcante, ETH Zürich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/faxpy.c"

#define CHECK

double *a;
double *x;
double *y;

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

  #if MEAS_1ITER== 1
  uint32_t measure_iter = 1;
  #else
  uint32_t measure_iter = 3;
  #endif

  #if USE_CACHE == 1

  uint32_t spm_size = 16;
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
  uint32_t timer_tmp, timer_iter1;

  const unsigned int dim = axpy_l.M;
  const unsigned int dim_core = dim / num_cores;

  #if USE_CACHE == 0

  // Allocate the matrices
  if (cid == 0) {
    a = (double *)snrt_l1alloc(sizeof(double));
    x = (double *)snrt_l1alloc(dim * sizeof(double));
    y = (double *)snrt_l1alloc(dim * sizeof(double));
  }

  // Initialize the matrices
  if (cid == 0) {
    a = (double *) &axpy_alpha_dram;
    snrt_dma_start_1d(x, axpy_X_dram, dim * sizeof(double));
    snrt_dma_start_1d(y, axpy_Y_dram, dim * sizeof(double));
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  double *x_int = x + dim_core * cid;
  double *y_int = y + dim_core * cid;

  #else
  double *x_int = axpy_X_dram + dim_core * cid;
  double *y_int = axpy_Y_dram + dim_core * cid;
  a = (double *) &axpy_alpha_dram;
  #endif

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  for (int iter = 0; iter < measure_iter; iter ++) {
    // Start dump
    if (cid == 0)
      start_kernel();

    timer_tmp = benchmark_get_cycle();

    // Call AXPY
    faxpy_v64b(*a, x_int, y_int, dim_core);

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End timer and check if new best runtime
    if (cid == 0) {
      timer_tmp = benchmark_get_cycle() - timer_tmp;
      timer = (timer < timer_tmp) ? timer : timer_tmp;
      if (iter == 0)
        timer_iter1 = timer;

      #ifdef PRINT_RESULT
      printf("Iteration %u: %u cycles\n", iter, timer_tmp);
      #endif

      stop_kernel();
    }

    #ifdef CHECK
    if (cid == 0 & iter == 0) {
      // since y will be used to store data, we can only check in first iteration
      for (unsigned int i = 0; i < dim; i++) {
        #if USE_CACHE == 0
        if (fp_check(y[i], axpy_GR_dram[i]))
        #else
        if (fp_check(axpy_Y_dram[i], axpy_GR_dram[i]))
        #endif
        {
        #ifdef PRINT_RESULT
          #if USE_CACHE == 0
          printf("Error: Index %d -> Result = %f, Expected = %f\n", i,
                 (float)y[i], (float)axpy_GR_dram[i]);
          #else
          printf("Error: Index %d -> Result = %f, Expected = %f\n", i,
                 (float)axpy_Y_dram[i], (float)axpy_GR_dram[i]);
          #endif
        #endif
          return 1;
        }
      }
    }
    #endif

    snrt_cluster_hw_barrier();
  }

  // Check and display results
  if (cid == 0) {
    // timer = timer_end - timer_start;
    write_cyc(timer);
    long unsigned int performance = 1000 * 2 * dim / timer;
    long unsigned int utilization = performance / (2 * num_cores * 4);
  #ifdef PRINT_RESULT
    printf("\n----- (%d) axpy -----\n", dim);
    printf("The execution took %u cycles in 1st iter.\n", timer_iter1);
    printf("The execution took %u cycles for best.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  #endif
  }

  snrt_cluster_hw_barrier();

  set_eoc();

  return 0;
}
