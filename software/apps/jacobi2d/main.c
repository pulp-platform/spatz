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

// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "data/data_jacobi2d.h"
#include "kernel/jacobi2d.c"
#include "printf.h"
#ifdef MEMPOOL
#include "alloc.h"
#include "runtime.h"
#include "synchronization.h"
#endif

// Perform a final check with the golden model
#define CHECK
// Print per-core info about per-core variables
#define VERBOSE
// Execute from DRAM. Currently, it's the only working solution
#define DRAM

#ifndef TSTEPS
#define TSTEPS 1
#endif

// Trick to enable next printf functions. Without this, the first printf is not executed.
#define ENABLE_PRINTF if (!cid) printf("Work-around printf to enable next printf functions\n");

// Threshold for FP comparisons
#define THRESHOLD 0.0000000001

// Macro to check similarity between two fp-values, wrt a threshold
#define fp_check(a, b, threshold) ((((a - b) < 0) ? b - a : a - b) < threshold)

// Matrices
double *a;
double *b;

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

  const unsigned int r   = jacobi2d_l.R;
  const unsigned int c   = jacobi2d_l.C;
  const unsigned int dim = r * c;

  unsigned int timer_start, timer_end, timer;

  // Initialize MemPool
  mempool_init(cid, num_cores);

  // Initialize multicore barrier
  mempool_barrier_init(cid);

  // Printf-bug workaround
  ENABLE_PRINTF;

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a = (double *)domain_malloc(get_alloc_tile(0), dim * sizeof(double));
    b = (double *)domain_malloc(get_alloc_tile(0), dim * sizeof(double));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // Per-core column dimension
  const unsigned int c_dim_core = jacobi2d_l.c_dim_core;

  // How many cores in the C and R dimension?
  const unsigned int n_c_cores = c / c_dim_core;
  const unsigned int n_r_cores = active_cores / n_c_cores;

  // Per-core row dimension
  const unsigned int r_dim_core = r / n_r_cores;

  // Per-core ids
  const unsigned int c_core_id = cid % n_c_cores;
  const unsigned int r_core_id = cid / n_c_cores;

  // Per-core DRAM matrix pointers (slow)
  double* a_ptr_core_dram = jacobi2d_A_dram + (c_dim_core * c_core_id) + (c * r_dim_core * r_core_id);
  double* b_ptr_core_dram = jacobi2d_B_dram + (c_dim_core * c_core_id) + (c * r_dim_core * r_core_id);

  // Per-core matrix pointers (fast)
  double* a_ptr_core = a + (c_dim_core * c_core_id) + (c * r_dim_core * r_core_id);
  double* b_ptr_core = b + (c_dim_core * c_core_id) + (c * r_dim_core * r_core_id);

  // Only edge cores deal with padding
  uint32_t start_x = !c_core_id ? 1 : 0;
  uint32_t start_y = !r_core_id ? 1 : 0;
  uint32_t end_x   = (c_core_id == n_c_cores - 1) ? 1 : 0;
  uint32_t end_y   = (r_core_id == n_r_cores - 1) ? 1 : 0;

  // Calculate start/end offsets for the various cores
  uint32_t size_x = c_dim_core - start_x - end_x;
  uint32_t size_y = r_dim_core - start_y - end_y;

#ifdef VERBOSE
  for (uint32_t i = 0; i < active_cores; ++i) {
    if (cid == i) {
      printf("-- Core %d --\n", cid);
      printf("a = %p, b = %p\n", a, b);
      printf("a_ptr_core = %p, b_ptr_core = %p", a_ptr_core, b_ptr_core);
      printf("a_ptr_core_dram = %p, b_ptr_core_dram = %p\n", jacobi2d_A_dram, jacobi2d_B_dram);
      printf("c_dim_core = %d, c_core_id = %d, c = %d, r_dim_core = %d, r_core_id = %d, r == %d\n",
        c_dim_core, c_core_id, c, r_dim_core, r_core_id, r);
      printf("a_ptr_core = %p, b_ptr_core = %p", a_ptr_core, b_ptr_core);
      printf("start_y = %d, start_x = %d, size_y = %d, size_x = %d, c = %d\n",
        start_y, start_x, size_y, size_x, c);
      printf("-------------\n");
    }

    // Wait for all cores to finish
    mempool_barrier(num_cores);
  }
#endif

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Thus far, only perfect problem sizes supported
  if ((c % c_dim_core) || (active_cores % n_c_cores)) return -1;

  // Initialize matrices
  if (cid == 0) {
    init_matrix(a, jacobi2d_A_dram, dim);
    init_matrix(b, jacobi2d_B_dram, dim);
  }

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Calculate jacobi2d
  if (is_core_active) {
    // Start timer
    if (cid == 0)
      timer_start = mempool_get_timer();

    // Start dump
    if (cid == 0)
      start_kernel();

    // Calculate the result
    // TODO: add barriers after each single jacobi2d run
    for (uint32_t t = 0; t < TSTEPS; t++) {
#ifdef DRAM
      jacobi2d(a_ptr_core_dram, b_ptr_core_dram, start_y, start_x, size_y, size_x, c);
#else
      jacobi2d(a_ptr_core,      b_ptr_core,      start_y, start_x, size_y, size_x, c);
#endif
      //mempool_barrier(active_cores);
#ifdef DRAM
      jacobi2d(b_ptr_core_dram, a_ptr_core_dram, start_y, start_x, size_y, size_x, c);
#else
      jacobi2d(b_ptr_core,      a_ptr_core,      start_y, start_x, size_y, size_x, c);
#endif
      //mempool_barrier(active_cores);
    }
  }

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // End dump
  if (cid == 0)
    stop_kernel();

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // End timer
  if (cid == 0) {
    timer_end = mempool_get_timer();
    timer = timer_end - timer_start;
  }

  // Check and display results
  if (cid == 0) {
    unsigned int performance = 1000 * 2 * TSTEPS * 5 * c * r / timer;
    unsigned int utilization = performance / (active_cores * N_FPU);

    printf("\n----- (%dx%d) dp jacobi2d -----\n", dim, dim);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %u OP/1000cycle (%u%%o utilization).\n",
           performance, utilization);
  }

#ifdef CHECK
  if (cid == 0) {
    for (unsigned int i = 0; i < dim; ++i)
      if (!fp_check(jacobi2d_result[i], jacobi2d_A_dram[i], THRESHOLD)) {
        printf("Error core %d, index %d: result=%lx, golden=%lx\n",
          cid, i, *((uint64_t*)&jacobi2d_A_dram[i]), *((uint64_t*)&jacobi2d_result[i]));
        return -1;
      }
  }
#endif

  // Free the matrices
  if (cid == 0) {
    domain_free(get_alloc_tile(0), a);
    domain_free(get_alloc_tile(0), b);
  }

  // Wait for core 0 to finish displaying results
  mempool_barrier(num_cores);

  return 0;
}
