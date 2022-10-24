// Copyright 2021 ETH Zurich and University of Bologna.
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

// Author: Domenic Wüthrich, ETH Zurich

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "kernel/dp-fmatmul.c"
#include "printf.h"
#ifdef MEMPOOL
#include "runtime.h"
#include "synchronization.h"
#endif

// Define Matrix dimensions:
// C = AB with A=[MxN], B=[NxP], C=[MxP]
#ifndef MATRIX_DIM
#define MATRIX_DIM 256
#endif
#ifndef KERNEL_M
#define KERNEL_M 2
#endif
#ifndef KERNEL_P
#define KERNEL_P 2
#endif
#define M MATRIX_DIM
#define N MATRIX_DIM
#define P MATRIX_DIM
// Specify how the matrices A and B should be initialized
// The entries will follow this format:
// a(i,j) = A_a*i + A_b*j + A_c
// b(i,j) = B_a*i + B_b*j + B_c
// The result will be the following matrix
// c(i,j) = (A_a*B_b*i*j + A_a*B_c*i + A_c*B_b*j + A_c*B_c) * N
//        + (A_a*B_a*i + A_b*B_b*j + A_b*B_c + B_a*A_c) * (N*(N-1))/2
//        + (A_b*B_a) * (N*(N-1)*(2*N-1))/6
// Note: To keep the code simpler, we use indices that go from 0 to N-1 instead
// of 1 to N as the mathematicians do. Hence, for A, i=[0,M-1] j=[0,M-1]
#define A_a 1
#define A_b 1
#define A_c -32
#define B_a 2
#define B_b 1
#define B_c 16

double a[M * N] __attribute__((aligned(8), section(".l1")));
double b[N * P] __attribute__((aligned(8), section(".l1")));
double c[M * P] __attribute__((aligned(8), section(".l1")));

// Initialize the matrices
void init_matrix(double *matrix, const unsigned int rows_start,
                 const unsigned int rows_end, const unsigned int num_columns,
                 int a, int b, int c) {
  for (unsigned int i = rows_start; i < rows_end; ++i) {
    for (unsigned int j = 0; j < num_columns; ++j) {
      matrix[i * num_columns + j] = (double)(a * (int)i + b * (int)j + c);
    }
  }
}

// Verify the matrices
int verify_matrix(double const *matrix, const unsigned int row_start,
                  const unsigned int row_end, const unsigned int n,
                  const unsigned int p, int aa, int ab, int ac, int ba, int bb,
                  int bc) {
  for (int i = (int)row_start; i < (int)row_end; i++) {
    for (int j = 0; j < (int)p; j++) {
      int lin =
          (aa * bb * i * j + aa * bc * i + ac * bb * j + ac * bc) * (int)n;
      int qua = ((aa * ba * i + ab * bb * j + ab * bc + ba * ac) *
                 (int)(n * (n - 1))) /
                2;
      int cub = ((ab * ba) * (int)(n * (n - 1) * (2 * n - 1))) / 6;
      int golden = lin + qua + cub;
      if ((int)matrix[i * (int)p + j] != golden) {
        printf("Expected c[%d]=%u, got %u\n", i * (int)p +j, golden,
               (int)c[i * (int)+j]);
        return (i + j) == 0 ? -1 : i * (int)p + j;
      }
    }
  }
  return 0;
}

void print_matrix(double const *matrix, unsigned int num_rows,
                  unsigned int num_columns) {
  printf("0x%8X\n", (unsigned int)matrix);
  for (unsigned int i = 0; i < num_rows; ++i) {
    for (unsigned int j = 0; j < num_columns; ++j) {
      printf("%5u ", (unsigned int)matrix[i * num_columns + j]);
    }
    printf("\n");
  }
}

int main() {
  const unsigned int num_cores = mempool_get_core_count();
  const unsigned int cores_per_group = num_cores / NUM_GROUPS;
  const unsigned int core_id = mempool_get_core_id();
  const unsigned int core_id_group = core_id % cores_per_group;
  const unsigned int group_id = core_id / cores_per_group;

  unsigned int timer_start, timer_end, timer;
  unsigned int row_start, row_end;

  unsigned int m_start, m_end;
  unsigned int p_start, p_end;
  unsigned int vl;
  unsigned int dim;
  unsigned int kernel_size;
  unsigned int measure_iterations = 1;

  // Initialize multicore barrier
  mempool_barrier_init(core_id);

  // Reset timer
  timer = (unsigned int)-1;

  // Set matrix dimension
  dim = MATRIX_DIM;
  kernel_size = KERNEL_M;
  vl = KERNEL_P;

  // Can every core execute its desired kernel?
  if ((dim * dim) / (kernel_size * vl) < num_cores)
    return -1;
  // Does the vl fit inside the dim
  if (vl > dim)
    return -2;

  // Block dimension of group
  const unsigned int dim_group = dim / NUM_GROUPS;
  // Number of parallel cores in m direction
  const unsigned int split_m_count = dim_group / kernel_size;

  if (split_m_count < cores_per_group) {
    // Split P dimension up
    const unsigned int split_p_count = cores_per_group / split_m_count;
    p_start = dim / split_p_count * (core_id_group % split_p_count);
    p_end = dim / split_p_count * ((core_id_group % split_p_count) + 1);
    m_start =
        dim_group * group_id + kernel_size * (core_id_group / split_p_count);
    m_end = dim_group * group_id +
            kernel_size * (core_id_group / split_p_count + 1);
  } else {
    // Work over complete P dimension
    p_start = 0;
    p_end = dim;
    m_start =
        dim_group * group_id + (dim_group / cores_per_group) * core_id_group;
    m_end = dim_group * group_id +
            (dim_group / cores_per_group) * (core_id_group + 1);
  }

  // Initialize matrices
  const unsigned int cores_per_row = num_cores / dim;
  unsigned int do_init_verify = 1;
  if (dim < num_cores) {
    row_start = core_id / cores_per_row;
    row_end = core_id / cores_per_row + 1;
    // Core will skip matrix init and verify parts
    do_init_verify = (core_id % cores_per_row) == 0;
  } else {
    row_start = dim / num_cores * (core_id);
    row_end = dim / num_cores * (core_id + 1);
  }

  if (do_init_verify) {
    init_matrix(a, row_start, row_end, dim, A_a, A_b, A_c);
    init_matrix(b, row_start, row_end, dim, B_a, B_b, B_c);
  }

  // Execute matmul a few times and measure runtime
  for (unsigned int i = 0; i < measure_iterations; i++) {
    // Wait for all cores to finish
    mempool_barrier(num_cores);

    // Start timer
    timer_start = mempool_get_timer();

    // Calculate matmul
    if (kernel_size == 2) {
      matmul_2xVL(c, a, b, m_start, m_end, dim, dim, p_start, p_end, vl);
    } else if (kernel_size == 4) {
      matmul_4xVL(c, a, b, m_start, m_end, dim, dim, p_start, p_end, vl);
    } else if (kernel_size == 8) {
      matmul_8xVL(c, a, b, m_start, m_end, dim, dim, p_start, p_end, vl);
    } else {
      return -2;
    }
    // Wait for all cores to finish matmul
    mempool_barrier(num_cores);

    // End timer and check if new best runtime
    timer_end = mempool_get_timer();
    unsigned int timer_temp = timer_end - timer_start;
    if (core_id == 0) {
      if (timer_temp < timer) {
        timer = timer_temp;
      }
    }
  }

  // Check and display results
  if (core_id == 0) {
    unsigned int performance = 1000 * 2 * dim * dim * dim / timer;
    unsigned int utilization = performance / (2 * num_cores * N_IPU);

    printf("\n----- (%dx%d) dp fmatmul -----\n", dim, dim);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %u OP/1000cycle (%u%%o utilization).\n",
           performance, utilization);
  }

  if (do_init_verify && core_id == 0) {
    int error =
        verify_matrix(c, 0, dim, dim, dim, A_a, A_b, A_c, B_a, B_b, B_c);

    if (error != 0) {
      printf("Error core %d: c[%d]=%u\n", core_id, error, (int)c[error]);
      return error;
    }
  }

  // Wait for core 0 to finish displaying results
  mempool_barrier(num_cores);

  return 0;
}
