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

// Author: Domenic Wüthrich

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "data/data_axpy.h"
#include "printf.h"
#ifdef MEMPOOL
#include "alloc.h"
#include "runtime.h"
#include "synchronization.h"
#endif

double *x;
double *y;

// Initialize the matrices
void init_matrix(double *matrix, const double *src,
                 const unsigned int num_columns) {
  for (unsigned int j = 0; j < num_columns; ++j)
    matrix[j] = src[j];
}

// Verify the matrices
int verify_matrix(double *matrix, const double *golden,
                  const unsigned int num_columns) {
  for (unsigned int j = 0; j < num_columns; ++j) {
    double diff = matrix[j] - golden[j];
    if (diff < 0)
      diff = -diff;
    if (diff > 0.001)
      return j == 0 ? -1 : (int)j;
  }
  return 0;
}

int main() {
  const unsigned int num_cores = mempool_get_core_count();
  const unsigned int cores_per_group = num_cores / NUM_GROUPS;
  const unsigned int cid = mempool_get_core_id();

  const unsigned int active_groups = 1;
  const unsigned int active_cores = cores_per_group * active_groups;
  const unsigned int is_core_active = cid < active_cores;

  unsigned int timer_start, timer_end, timer;

  unsigned int vl;
  unsigned int dim;

  // Initialize MemPool
  mempool_init(cid, num_cores);

  // Initialize multicore barrier
  mempool_barrier_init(cid);

  // Allocate the matrices in the local tile
  if (cid == 0) {
    x = (double *)domain_malloc(get_alloc_tile(0), axpy_l.M * sizeof(double));
    y = (double *)domain_malloc(get_alloc_tile(0), axpy_l.M * sizeof(double));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // Set matrix dimension
  dim = axpy_l.M;
  vl = axpy_l.M / active_cores;

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Initialize matrices
  if (cid == 0) {
    init_matrix(x, axpy_X_dram, dim);
    init_matrix(y, axpy_Y_dram, dim);
  }

  // Initialize alpha
  double alpha = axpy_alpha_dram;

  // Calculate local pointers
  double *x_ = x + cid * vl;
  double *y_ = y + cid * vl;

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Calculate AXPY
  if (is_core_active) {
    // Start timer
    timer_start = mempool_get_timer();

    // Start dump
    if (cid == 0)
      start_kernel();

    // Stripmining loop
    while (vl > 0) {
      // Set the VL
      size_t gvl;
      asm volatile("vsetvli %[gvl], %[vl], e64, m8, ta, ma"
                   : [gvl] "=r"(gvl)
                   : [vl] "r"(vl));

      // Load vectors
      asm volatile("vle64.v v0, (%0)" ::"r"(x_));
      asm volatile("vle64.v v16, (%0)" ::"r"(y_));

      // Multiply-accumulate
      asm volatile("vfmacc.vf v16, %0, v0" ::"f"(alpha));

      // Store results
      asm volatile("vse64.v v16, (%0)" ::"r"(y_));

      // Bump pointers
      x_ += gvl;
      y_ += gvl;
      vl -= gvl;
    }
  }

  // Wait for all cores to finish matmul
  mempool_barrier(num_cores);

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer and check if new best runtime
  timer_end = mempool_get_timer();
  if (cid == 0) {
    unsigned int timer_temp = timer_end - timer_start;
    if (timer_temp < timer) {
      timer = timer_temp;
    }
  }

  // Check and display results
  if (cid == 0) {
    unsigned int performance = 1000 * 2 * dim / timer;
    unsigned int utilization = performance / (2 * active_cores * N_FPU);

    printf("\n----- (%d) axpy -----\n", dim);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %u OP/1000cycle (%u%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0) {
    int error = verify_matrix(y, (const double *)axpy_GR_dram, dim);

    if (error != 0) {
      printf("Error: y[%d]=%04x\n", error, *(uint32_t *)(y + error));
      return error;
    }
  }

  // Free the matrices
  if (cid == 0) {
    domain_free(get_alloc_tile(0), x);
    domain_free(get_alloc_tile(0), y);
  }

  // Wait for core 0 to finish displaying results
  mempool_barrier(num_cores);

  return 0;
}
