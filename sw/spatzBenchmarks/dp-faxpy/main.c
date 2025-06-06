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
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/faxpy.c"

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

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  const unsigned int dim = axpy_l.M;
  const unsigned int dim_core = dim / num_cores;

  // Allocate the matrices
  if (cid == 0) {
    a = (double *)snrt_l1alloc(sizeof(double));
    x = (double *)snrt_l1alloc(dim * sizeof(double));
    y = (double *)snrt_l1alloc(dim * sizeof(double));
  }

  // Initialize the matrices
  if (cid == 0) {
    *a = axpy_alpha_dram;

    snrt_dma_start_1d(x, axpy_X_dram, dim * sizeof(double));
    snrt_dma_start_1d(y, axpy_Y_dram, dim * sizeof(double));
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate internal pointers
  double *x_int = x + dim_core * cid;
  double *y_int = y + dim_core * cid;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();

  // Call AXPY
  faxpy_v64b(*a, x_int, y_int, dim_core);

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;

  // End dump
  if (cid == 0)
    stop_kernel();

  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * dim / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    PRINTF("\n----- (%d) axpy -----\n", dim);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0) {
    for (unsigned int i = 0; i < dim; i++) {
      if (fp_check(y[i], axpy_GR_dram[i])) {
        PRINTF("Error: Index %d -> Result = %f, Expected = %f\n", i,
               (float)y[i], (float)axpy_GR_dram[i]);
      }
    }
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}
