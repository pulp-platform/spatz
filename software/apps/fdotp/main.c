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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "data/data_dotp.h"
#include "kernel/fdotp.c"
#include "printf.h"
#ifdef MEMPOOL
#include "alloc.h"
#include "runtime.h"
#include "synchronization.h"
#endif

// Threshold for FP comparisons
#define THRESHOLD_64b 0.0000000001
#define THRESHOLD_32b 0.0001
#define THRESHOLD_16b 1

// Run also the scalar benchmark
#define SCALAR 1

// Check the vector results against golden vectors
#define CHECK 1

// Macro to check similarity between two fp-values, wrt a threshold
#define fp_check(a, b, threshold) ((((a) < (b)) ? (b) - (a) : (a) - (b)) < (threshold))

double *a;
double *b;
double *result;

// Initialize the matrices
void init_matrix(double *matrix, const double *src,
                 const unsigned int len) {
  for (unsigned int i = 0; i < len; ++i) {
    matrix[i] = src[i];
  }
}

int main() {
  const unsigned int num_cores = mempool_get_core_count();
  const unsigned int cores_per_group = num_cores / NUM_GROUPS;
  const unsigned int cid = mempool_get_core_id();

  const unsigned int active_groups = 1;
  const unsigned int active_cores = cores_per_group * active_groups;
  const unsigned int is_core_active = cid < active_cores;

  const unsigned int dim = dotp_l.M;

  unsigned int timer_start, timer_end, timer;

  // Initialize MemPool
  mempool_init(cid, num_cores);

  // Initialize multicore barrier
  mempool_barrier_init(cid);

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a = (double *)domain_malloc(get_alloc_tile(0), dim * sizeof(double));
    b = (double *)domain_malloc(get_alloc_tile(0), dim * sizeof(double));
    result = (double *)domain_malloc(get_alloc_tile(0), active_cores * sizeof(double));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // Block dimension of core
  const unsigned int dim_core = dim / active_cores;

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Initialize matrices
  if (cid == 0) {
    init_matrix(a, dotp_A_dram, dim);
    init_matrix(b, dotp_B_dram, dim);
  }

  double *a_int = a + dim_core * cid;
  double *b_int = b + dim_core * cid;

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Calculate dotp
  double acc;
  if (is_core_active) {
    // Start timer
    if (cid == 0)
      timer_start = mempool_get_timer();

    // Start dump
    if (cid == 0)
      start_kernel();

    // Calculate the result
    acc = fdotp_v64b(a_int, b_int, dim_core);
    if (cid != 0)
      result[cid] = acc;
  }

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Final reduction
  if (cid == 0) {
    for (unsigned int i = 1; i < active_cores; ++i)
      acc += result[i];
    result[0] = acc;
  }

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer
  if (cid == 0) {
    timer_end = mempool_get_timer();
    timer = timer_end - timer_start;
  }

  // Check and display results
  if (cid == 0) {
    unsigned int performance = 1000 * 2 * dim / timer;
    unsigned int utilization = performance / (2 * active_cores * N_FPU);

    printf("\n----- (%dx%d) dp fdotp -----\n", dim, dim);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %u OP/1000cycle (%u%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0) {
    if (!fp_check(result[0], dotp_result, THRESHOLD_64b)) {
      printf("Error core %d: result = %x, golden = %x\n", cid, *((unsigned int*)result), *((unsigned int*)&dotp_result));
      return -1;
    }
  }

  // Free the matrices
  if (cid == 0) {
    domain_free(get_alloc_tile(0), a);
    domain_free(get_alloc_tile(0), b);
    domain_free(get_alloc_tile(0), result);
  }

  // Wait for core 0 to finish displaying results
  mempool_barrier(num_cores);

  return 0;
}
