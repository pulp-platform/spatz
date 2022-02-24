// Copyright 2020 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "conv2d_3x3.c"
#include "conv2d_5x5.c"
#include "conv2d_7x7.c"

#include "printf.h"
#ifdef MEMPOOL
#include "runtime.h"
#include "synchronization.h"
#endif

#ifndef N_IPU
#define N_IPU 2
#endif
#ifndef MATRIX_DIM
#define MATRIX_DIM 64
#endif
#ifndef KERNEL_M
#define KERNEL_M 3
#endif
#ifndef KERNEL_P
#define KERNEL_P 8
#endif
#define M MATRIX_DIM
#define N MATRIX_DIM
#define F KERNEL_M

// Define Matrix dimensions:
// o = i ° f, with i=[MxN], f=[FxF], o=[MxN]
// The filter is a square matrix, and F is odd

// Matrices defined in data.S
extern int32_t i[] __attribute__((aligned(4))); // [ (M+floor(F/2)) * (N+floor(F/2)) ]
extern int32_t f[] __attribute__((aligned(4)));        // [ F*F ]
extern int32_t o[] __attribute__((aligned(4)));        // [ M*N ]
extern int32_t golden_o[] __attribute__((aligned(4))); // [ M*N ]

int32_t i_l1[(M+F/2*2)*(N+F/2*2)] __attribute__((aligned(4), section(".l1_prio"))); // [ (M+floor(F/2)) * (N+floor(F/2)) ]
int32_t o_l1[M*N] __attribute__((aligned(4), section(".l1_prio")));        // [ M*N ]

void copy_matrix(int32_t *src, int32_t *dst, uint32_t size) {
  for (uint32_t idx = 0; idx < size; idx++) {
    dst[idx] = src[idx];
  }
}

int verify_matrix(int32_t *matrix, int32_t *golden_matrix, int32_t size) {
  for (int idx = 0; idx < size; idx++) {
    if (matrix[idx] != golden_matrix[idx]) {
      return idx;
    }
  }

  return 0;
}

void print_matrix(int32_t const *matrix, uint32_t num_rows,
                  uint32_t num_columns) {
  printf("0x%8X\n", (uint32_t)matrix);
  for (uint32_t i = 0; i < num_rows; ++i) {
    for (uint32_t j = 0; j < num_columns; ++j) {
      printf("%10d ", matrix[i * num_columns + j]);
    }
    printf("\n");
  }
}

int main() {

  uint32_t num_cores = mempool_get_core_count();
  uint32_t core_id = mempool_get_core_id();
  int32_t f_stack[F*F];

  if (F != 3 && F != 7) {
    printf("Error: the filter size is different from 3.\n");
    return -1;
  }

  #ifdef DISABLE_MULTICORE

    /////////////////
    // Single Core //
    /////////////////

    num_cores = 1;

    copy_matrix(i, i_l1, (M+F/2*2)*(N+F/2*2));
    copy_matrix(f, f_stack, F*F);

    // Call the main kernel, and measure cycles
    uint32_t timer_start = mempool_get_timer();
    // Execute convolution
    #if F == 3
      conv2d_3x3(o_l1, i_l1, f_stack, M, N);
    #elif F == 7
      conv2d_7x7(o_l1, i_l1, f_stack, M, N);
    #endif

  #else

    ////////////////
    // Multi Core //
    ////////////////

    // Initialize multicore barrier
    mempool_barrier_init(core_id);

    // Set matrix dimension
    uint32_t dim = MATRIX_DIM;
    uint32_t vl = KERNEL_P;
    #if F == 7
      vl *= 2;
    #endif

    // Can every core execute its desired kernel?
    if ((dim*dim)/vl < num_cores) return -2;
    // Does the vl fit inside the dim
    if (vl > dim) return -3;

    uint32_t column_split = dim/vl;
    uint32_t column_id = core_id%column_split;

    if (num_cores < column_split) return -4;

    uint32_t num_rows = dim*column_split/num_cores;

    if (num_rows < 8) return -5;

    uint32_t row_offset = (dim + F - 1)*num_rows*(core_id/column_split);
    uint32_t column_offset = column_id*vl;
    //int32_t *i_start = i + i_offset;
    int32_t *i_l1_start = i_l1 + row_offset + column_offset;

    uint32_t row_offset_o = dim*num_rows*(core_id/column_split);
    int32_t *o_l1_start = o_l1 + row_offset_o + column_offset;

    for (uint32_t row = core_id; row < (M+F/2*2); row+=num_cores) {
      copy_matrix(i+row*(N+F/2*2), i_l1+row*(N+F/2*2), (N+F/2*2));
    }

    copy_matrix(f, f_stack, F*F);

    // Wait for all cores to finish matrix copy
    mempool_barrier(num_cores);

    uint32_t timer_start = mempool_get_timer();
    // Execute convolution
    #if F == 3
      conv2d_3x3(o_l1_start, i_l1_start, f_stack, num_rows, vl);
    #elif F == 7
      conv2d_7x7(o_l1_start, i_l1_start, f_stack, num_rows, vl);
    #endif

    mempool_barrier(num_cores);

  #endif

  // Performance metrics
  uint32_t timer_end = mempool_get_timer();

  if (core_id == 0) {
    uint32_t runtime = timer_end - timer_start;
    uint32_t performance = 1000 * 2 * F * F * M * N / runtime;
    uint32_t utilization = performance / (2 * num_cores * N_IPU);

    printf("The execution took %u cycles.\n", runtime);
    printf("The performance is %u OP/1000cycles (%u%%o utilization).\n",
           performance, utilization);
    // Verify correctness
    printf("Verifying result...\n");
  }

  for (uint32_t row = core_id; row < M; row+=num_cores) {
    int error = verify_matrix(o_l1+row*N, golden_o+row*N, N);
    if (error != 0) {
      printf("Fail.\n");
      if (row == 0) row = M;
      return (int)row*N+error;
    }
  }

  #ifndef DISABLE_MULTICORE
    mempool_barrier(num_cores);
  #endif

  /*for (int idx = 0; idx < (M+F/2*2)*(N+F/2*2); idx++) {
    i_l1[idx] = i[idx];
  }

  for (int idx = 0; idx < F*F; idx++) {
    f_l1[idx] = f[idx];
  }

  // Call the main kernel, and measure cycles
  uint32_t timer_start = mempool_get_timer();
  if (F == 3)
    conv2d_3x3(o_l1, i_l1, f_l1, M, N);
  else if (F == 5)
    conv2d_5x5(o_l1, i_l1, f_l1, M, N, F);
  else if (F == 7)
    conv2d_7x7(o_l1, i_l1, f_l1);
  else
    printf("Error: the filter size is different from 3 or 5 or 7.\n");

  // Performance metrics
  uint32_t timer_end = mempool_get_timer();
  uint32_t runtime = timer_end - timer_start;
  uint32_t performance = 1000 * 2 * F * F * M * N / runtime;
  uint32_t utilization = performance / 4;

  printf("The execution took %u cycles.\n", runtime);
  printf("The performance is %u OP/1000cycles (%u%%o utilization).\n",
         performance, utilization);

  // Verify correctness
  printf("Verifying result...\n");
  int error = verify_matrix(o_l1, golden_o, M, N);
  if (error != 0) {
    printf("Fail.\n");
  } else {
    printf("Passed.\n");
  }*/

  return 0;
}
