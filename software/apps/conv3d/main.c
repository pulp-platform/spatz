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
//         Domenic Wüthrich

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "conv3d_3x7x7.c"

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
#ifndef KERNEL_P
#define KERNEL_P 8
#endif
#define M MATRIX_DIM
#define N MATRIX_DIM
#define F 7
#define CH 3

// Define Matrix dimensions:
// o = i ° f, with i=[(M+F-1)x(N+f-1)xCH], f=[FxFxCH], o=[MxN]
// The filter is a square matrix, and F is odd

// Matrices defined in data.S
extern int32_t i[] __attribute__((aligned(4))); // [ (M+floor(F/2)) * (N+floor(F/2)) * CH ]
extern int32_t f[] __attribute__((aligned(4)));        // [ F*F*CH ]
extern int32_t o[] __attribute__((aligned(4)));        // [ M*N ]
extern int32_t golden_o[] __attribute__((aligned(4))); // [ M*N ]
// M, N, F defined in data.S
/*extern int32_t M;
extern int32_t N;
extern int32_t CH;
extern int32_t F;*/

int32_t i_l1[(M+F/2*2)*(N+F/2*2)*CH] __attribute__((aligned(4), section(".l1_prio"))); // [ (M+floor(F/2)) * (N+floor(F/2)) * CH ]
int32_t f_l1[F*F*CH] __attribute__((aligned(4), section(".l1_prio")));        // [ F*F*CH ]
int32_t o_l1[M*N] __attribute__((aligned(4), section(".l1_prio")));        // [ M*N ]

void copy_matrix(int32_t *src, int32_t *dst, uint32_t size) {
  for (uint32_t idx = 0; idx < size; idx++) {
    dst[idx] = src[idx];
  }
}

int verify_matrix(int32_t *matrix, int32_t *golden_matrix, int32_t R,
                  int32_t C) {
  for (int r = 0; r < R; ++r)
    for (int c = 0; c < C; ++c)
      if (matrix[c + C * r] != golden_matrix[c + C * r]) {
        printf("Error: o[%d][%d] = %ld, instead of %ld\n", r, c,
               matrix[c + C * r], golden_matrix[c + C * r]);
        return 1;
      }
  return 0;
}

void print_matrix(int32_t const *matrix, uint32_t num_rows,
                  uint32_t num_columns) {
  printf("0x%8X\n", (uint32_t)matrix);
  for (uint32_t i = 0; i < num_rows; ++i) {
    for (uint32_t j = 0; j < num_columns; ++j) {
      printf("%10f ", matrix[i * num_columns + j]);
    }
    printf("\n");
  }
}

int main() {
  uint32_t num_cores = mempool_get_core_count();
  uint32_t core_id = mempool_get_core_id();

  if (F != 7) {
    printf("Error: the filter size is different from 7.\n");
    return -1;
  }

  #ifdef DISABLE_MULTICORE

    /////////////////
    // Single Core //
    /////////////////

    copy_matrix(i, i_l1, (M+F/2*2)*(N+F/2*2)*CH);
    copy_matrix(f, f_l1, F*F*CH);

    // Call the main kernel, and measure cycles
    uint32_t timer_start = mempool_get_timer();
    // Execute convolution
    conv3d_CHx7x7(o, i_l1, f_l1);

  #else

    ////////////////
    // Multi Core //
    ////////////////

    // Initialize multicore barrier
    mempool_barrier_init(core_id);

    // Set matrix dimension
    uint32_t dim = MATRIX_DIM;
    uint32_t vl = KERNEL_P;

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

    if (core_id == 0) {
      copy_matrix(i, i_l1, (M+F/2*2)*(N+F/2*2)*CH);
      copy_matrix(f, f_l1, F*F*CH);
    }

    asm volatile("vsetvli zero, %0, e32, m2, ta, ma" ::"r"(vl));

    // Wait for all cores to finish matrix copy
    mempool_barrier(num_cores);

    uint32_t timer_start = mempool_get_timer();
    // Execute convolution
    conv3d_CHx7x7_block(o_l1_start, i_l1_start, num_rows, f_l1, vl);

  #endif

  // Performance metrics
  uint32_t timer_end = mempool_get_timer();

  if (core_id == 0) {
    uint32_t runtime = timer_end - timer_start;
    uint32_t performance = 1000 * 2 * CH * F * F * M * N / runtime;
    uint32_t utilization = performance / (2 * 2);

    printf("The execution took %u cycles.\n", runtime);
    printf("The performance is %u OP/1000cycles (%u%%o utilization).\n",
           performance, utilization);

    // Verify correctness
    printf("Verifying result...\n");
    int error = verify_matrix(o, golden_o, M, N);
    if (error != 0) {
      printf("Fail.\n");
    } else {
      printf("Passed.\n");
    }

    return error;
  }

  #ifndef DISABLE_MULTICORE
    mempool_barrier(num_cores);
  #endif

  return 0;
}
