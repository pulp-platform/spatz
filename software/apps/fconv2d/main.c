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
double *imtx;  // (R+F-1)*(C+F-1)
double *fmtx;  // F*F
double *omtx;  // R*C
double *gomtx; // R*C

// Initialize the matrices
void init_matrix(double *matrix, const double *src, const unsigned int len) {
  for (unsigned int i = 0; i < len; ++i) {
    matrix[i] = src[i];
  }
}

void copy_matrix(double *src, double *dst, uint32_t size) {
  for (uint32_t idx = 0; idx < size; idx++) {
    dst[idx] = src[idx];
  }
}

int main() {
  const unsigned int num_cores = mempool_get_core_count();
  const unsigned int cores_per_group = num_cores / NUM_GROUPS;
  const unsigned int core_id = mempool_get_core_id();

  const unsigned int active_groups = 1;
  const unsigned int active_cores = cores_per_group * active_groups;
  const unsigned int is_core_active = core_id < active_cores;

  // Set matrix dimension
  const unsigned int r = fconv2d_l.R;
  const unsigned int c = fconv2d_l.C;
  const unsigned int f = fconv2d_l.F;
  const unsigned int ch = fconv2d_l.CH;
  const uint32_t r_dim_core = fconv2d_l.r_dim_core;
  const uint32_t c_dim_core = fconv2d_l.c_dim_core;
  const unsigned int dim = r * c;

  unsigned int timer_start, timer_end, timer;

  double f_stack[f*f*ch];

  if (core_id == 0) {
    printf("Ciao\n");
  }

  if (core_id == 0) {
    printf("Hello\n");
  }

  // Initialize MemPool
  mempool_init(core_id, num_cores);

  // Initialize multicore barrier
  mempool_barrier_init(core_id);

  // Allocate the matrices in the local tile
  if (core_id == 0) {
    imtx = (double *)domain_malloc(get_alloc_tile(0), dim * sizeof(double));
    omtx = (double *)domain_malloc(get_alloc_tile(0), dim * sizeof(double));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // We support only square matrices for now
  if (r != c) return -9;

  // Can every core execute its desired kernel?
  if ((r * c) / c_dim_core < num_cores) return -2;
  // Does the c_dim_core fit inside the dim
  if (c_dim_core > c) return -3;

  uint32_t column_split = c/c_dim_core;
  uint32_t column_id = core_id%column_split;

  if (active_cores < column_split) return -4;

  uint32_t num_rows = r*column_split/active_cores;

  if (num_rows < (f + 1)) return -5;

  uint32_t row_offset = (r + f - 1)*num_rows*(core_id/column_split);
  uint32_t column_offset = column_id*c_dim_core;
  double *i_tile_start = imtx + row_offset + column_offset;
  double *i_dram_start = fconv2d_I_dram + row_offset + column_offset;

  uint32_t row_offset_o = r*num_rows*(core_id/column_split);
  double *o_tile_start = omtx + row_offset_o + column_offset;
  double *o_dram_start = fconv2d_R_dram + row_offset_o + column_offset;

  for (uint32_t row = core_id; row < (r+f/2*2)*ch; row+=active_cores) {
    copy_matrix(imtx+row*(c+f/2*2), imtx+row*(c+f/2*2), (c+f/2*2));
  }

  if (core_id == 0) {
    printf("Core %d, dram_I = %x, dram_F = %x, dram_O = %x, dram_GR = %x, row_offset = %d, column_offset = %d\n", core_id, i_dram_start, fconv2d_F_dram, o_dram_start, fconv2d_GR_dram, row_offset, column_offset);
    printf("Core %d, omtx = %x, o_start = %x, imtx = %x, i_start = %x, num_rows = %d, f_stack = %x, c_dim_core = %d\n", core_id, omtx, o_tile_start, imtx, i_tile_start, num_rows, f_stack, c_dim_core);
  }

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  if (core_id == 1) {
    printf("Core %d, dram_I = %x, dram_F = %x, dram_O = %x, dram_GR = %x, row_offset = %d, column_offset = %d\n", core_id, i_dram_start, fconv2d_F_dram, o_dram_start, fconv2d_GR_dram, row_offset, column_offset);
    printf("Core %d, omtx = %x, o_start = %x, imtx = %x, i_start = %x, num_rows = %d, f_stack = %x, c_dim_core = %d\n", core_id, omtx, o_tile_start, imtx, i_tile_start, num_rows, f_stack, c_dim_core);
  }

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Initialize matrices
  if (core_id == 0) {
    init_matrix(imtx, fconv2d_I_dram, (r+f-1)*(c+f-1));
    init_matrix(omtx, fconv2d_R_dram, r*c);
  }
  // Copy the filter on the stack
  copy_matrix(fmtx, f_stack, f*f*ch);

  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(c_dim_core));

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // Calculate fconv2d

  // Start timer
  if (core_id == 0)
    timer_start = mempool_get_timer();

  // Start dump
  if (core_id == 0)
    start_kernel();

  // Calculate the result
  if (is_core_active) {
    // From DRAM
    conv3d_CHx7x7_block(o_dram_start, i_dram_start, num_rows, fconv2d_F_dram, c_dim_core);
    // From TILE
    //conv3d_CHx7x7_block(o_tile_start, i_tile_start, num_rows, f_stack, c_dim_core);
  }

  // End dump
  if (core_id == 0)
    stop_kernel();

  // Wait for all cores to finish
  mempool_barrier(num_cores);

  // End timer
  if (core_id == 0) {
    timer_end = mempool_get_timer();
    timer = timer_end - timer_start;
  }

  // Check and display results
  if (core_id == 0) {
    unsigned int performance = 1000 * 2 * ch * f * f * r * c / timer;
    unsigned int utilization = performance / (2 * active_cores * N_FPU);

    printf("\n----- (%dx%d) dp fconv2d -----\n", r, c);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %u OP/1000cycle (%u%%o utilization).\n",
           performance, utilization);
  }

#ifdef CHECK
  if (core_id == 0) {
    for (unsigned int k = 0; k < ch*r*c; ++k) {
      printf("%d\n", k);
      if (!fp_check(fconv2d_GR_dram[k], fconv2d_R_dram[k], THRESHOLD)) {
        printf("Error core %d, index %d: result=%x, golden=%x\n", core_id, k,
               *((uint64_t *)&fconv2d_R_dram[k]),
               *((uint64_t *)&fconv2d_GR_dram[k]));
        return -1;
      }
    }
  }
#endif

  // Free the matrices
  if (core_id == 0) {
    domain_free(get_alloc_tile(0), imtx);
    domain_free(get_alloc_tile(0), omtx);
  }

  // Wait for core 0 to finish displaying results
  mempool_barrier(num_cores);

  return 0;
}
