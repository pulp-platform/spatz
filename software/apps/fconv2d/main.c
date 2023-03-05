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

#include "data/data_fconv2d.h"
#include "kernel/fconv2d.c"
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

// Threshold for FP comparisons
#define THRESHOLD 0.000000001

// Macro to check similarity between two fp-values, wrt a threshold
#define fp_check(a, b, threshold) ((((a - b) < 0) ? b - a : a - b) < threshold)

// Matrices
double *imtx;
double *omtx;

// Initialize the matrices
void init_matrix(double *matrix, const double *src, const unsigned int len) {
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

  // Set matrix dimension
  const unsigned int r = fconv2d_l.R;
  const unsigned int c = fconv2d_l.C;
  const unsigned int f = fconv2d_l.F;
  const unsigned int ch = fconv2d_l.CH;
  const uint32_t c_dim_core = fconv2d_l.c_dim_core;

  unsigned int timer_start, timer_end, timer;

  // Initialize MemPool
  mempool_init(cid, num_cores);

  // Initialize multicore barrier
  mempool_barrier_init(cid);

  // Allocate the matrices in the local tile
  if (cid == 0) {
    imtx = (double *)domain_malloc(get_alloc_tile(0),
                                   (r + f - 1) * (c + f - 1) * sizeof(double));
    omtx = (double *)domain_malloc(get_alloc_tile(0), r * c * sizeof(double));
  }

  // Allocate the filter matrices
  double *fmtx;
  if (is_core_active) {
    fmtx =
        (double *)domain_malloc(get_alloc_tile(0), f * f * ch * sizeof(double));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // We support only square matrices for now
  if (r != c)
    return -9;

  // Can every core execute its desired kernel?
  if ((r * c) / c_dim_core < num_cores)
    return -2;
  // Does the c_dim_core fit inside the dim
  if (c_dim_core > c)
    return -3;

  uint32_t column_split = c / c_dim_core;
  uint32_t column_id = cid % column_split;

  if (active_cores < column_split)
    return -4;

  uint32_t num_rows = r * column_split / active_cores;

  if (num_rows < (f + 1))
    return -5;

  uint32_t row_offset = (r + f - 1) * num_rows * (cid / column_split);
  uint32_t column_offset = column_id * c_dim_core;
  double *i_start = imtx + row_offset + column_offset;

  uint32_t row_offset_o = r * num_rows * (cid / column_split);
  double *o_start = omtx + row_offset_o + column_offset;

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Initialize matrices
  if (cid == 0) {
    init_matrix(imtx, fconv2d_I_dram, (r + f - 1) * (c + f - 1));
    init_matrix(omtx, fconv2d_R_dram, r * c);
  }

  // Copy the filter
  init_matrix(fmtx, fconv2d_F_dram, f * f * ch);

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  //
  // Calculate fconv2d
  //

  // Start timer
  if (cid == 0)
    timer_start = mempool_get_timer();

  // Start dump
  if (cid == 0)
    start_kernel();

  // Calculate the result
  if (is_core_active)
    conv3d_CHx7x7(o_start, i_start, fmtx, num_rows);

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // End timer
  if (cid == 0) {
    timer_end = mempool_get_timer();
    timer = timer_end - timer_start;
  }

  // End dump
  if (cid == 0)
    stop_kernel();

  // Check and display results
  if (cid == 0) {
    unsigned int performance = 1000 * 2 * ch * f * f * r * c / timer;
    unsigned int utilization = performance / (2 * active_cores * N_FPU);

    printf("\n----- (%dx%d) dp fconv2d -----\n", r, c);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %u OP/1000cycle (%u%%o utilization).\n",
           performance, utilization);
  }

#ifdef CHECK
  if (cid == 0)
    for (unsigned int k = 0; k < ch * r * c; ++k) {
      if (!fp_check(fconv2d_GR_dram[k], omtx[k], THRESHOLD)) {
        printf("Error index %d: result = %x, golden = %x\n", k,
               *((uint64_t *)&omtx[k]), *((uint64_t *)&fconv2d_GR_dram[k]));
        return -1;
      }
    }
#endif

  // Free the matrices
  if (cid == 0) {
    domain_free(get_alloc_tile(0), imtx);
    domain_free(get_alloc_tile(0), omtx);
  }

  if (is_core_active)
    domain_free(get_alloc_tile(0), fmtx);

  // Wait for core 0 to finish displaying results
  mempool_barrier(num_cores);

  return 0;
}
