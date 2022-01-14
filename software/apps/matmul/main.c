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

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "kernel/matmul.c"
#include "printf.h"
#ifdef MEMPOOL
#include "runtime.h"
#include "synchronization.h"
#endif

#define PERFORMANCE(runtime, dim, core_id, num_cores) do {                            \
  if (core_id == 0) {                                                                 \
    uint32_t performance = 1000 * 2 * dim * dim * dim / runtime;                      \
    uint32_t utilization =  performance / (2 * num_cores * N_IPU);                    \
                                                                                      \
    printf("The execution took %d cycles.\n", runtime);                               \
    printf("The performance is %d OP/1000cycle (%d%%o utilization).\n", performance,  \
           utilization);                                                              \
  }                                                                                   \
} while (0)

#define VERIFY(c, m_start, m_end, s, p_start, p_end, A_a, A_b, A_c, B_a, B_b, B_c, core_id) do {        \
  int32_t error = verify_matrix(c, m_start, m_end, s, p_start, p_end, s, A_a, A_b, A_c, B_a, B_b, B_c); \
                                                                                                        \
  if (error != 0) {                                                                                     \
    printf("Error core %d: c[%d]=%d\n", core_id, error, c[error]);                                      \
    return error;                                                                                       \
  }                                                                                                     \
} while (0)

#define PRINT_HEADER(size, core_id) do {                                  \
  if (core_id == 0) printf("\n----- (%dx%d) matmul -----\n", size, size); \
} while (0)

#define TILES 4
#define CORES_PER_TILE 2
// Define Matrix dimensions:
// C = AB with A=[MxN], B=[NxP], C=[MxP]
#ifndef MATRIX_DIM
#define MATRIX_DIM 8
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

int32_t a[M * N] __attribute__((aligned(32), section(".l1_prio")));
int32_t b[N * P] __attribute__((aligned(32), section(".l1_prio")));
int32_t c[M * P] __attribute__((aligned(32), section(".l1_prio")));

// Initialize the matrices
void init_matrix(int32_t *matrix, uint32_t rows_start, uint32_t rows_end,
                 uint32_t num_columns, int32_t a, int32_t b, int32_t c) {
  for (uint32_t i = rows_start; i < rows_end; ++i) {
    for (uint32_t j = 0; j < num_columns; ++j) {
      matrix[i * num_columns + j] = a * (int32_t)i + b * (int32_t)j + c;
    }
  }
}

// Verify the matrices
int verify_matrix(int32_t *matrix, int32_t m_start, int32_t m_end, int32_t n,
                  int32_t p_start, int32_t p_end, int32_t p, int32_t aa,
                  int32_t ab, int32_t ac, int32_t ba, int32_t bb, int32_t bc) {
  for (int32_t i = m_start; i < m_end; i++) {
    for (int32_t j = p_start; j < p_end; j++) {
      int32_t lin = (aa * bb * i * j + aa * bc * i + ac * bb * j + ac * bc) * n;
      int32_t qua =
          ((aa * ba * i + ab * bb * j + ab * bc + ba * ac) * (n * (n - 1))) / 2;
      int32_t cub = ((ab * ba) * (n * (n - 1) * (2 * n - 1))) / 6;
      int32_t golden = lin + qua + cub;
      if (matrix[i * p + j] != golden) {
        return (i + j) == 0 ? -1 : i * p + j;
      }
      matrix[i * p + j] = 0;
    }
  }
  return 0;
}

void print_matrix(int32_t const *matrix, uint32_t num_rows,
                  uint32_t num_columns) {
  printf("0x%8X\n", (uint32_t)matrix);
  for (uint32_t i = 0; i < num_rows; ++i) {
    for (uint32_t j = 0; j < num_columns; ++j) {
      printf("%5d ", matrix[i * num_columns + j]);
    }
    printf("\n");
  }
}

int main() {
  #ifdef DISABLE_MULTICORE

    /////////////////
    // Single Core //
    /////////////////

    // Set matrix dimension
    uint32_t dim = MATRIX_DIM;

    // Initialize matrices
    init_matrix(a, 0, dim, dim, A_a, A_b, A_c);
    init_matrix(b, 0, dim, dim, B_a, B_b, B_c);

    // Execute matmul and measure runtime
    uint32_t timer_start = mempool_get_timer();
    matmul(c, a, b, dim, dim, dim);
    uint32_t timer_end = mempool_get_timer();

    // Check and display results
    PRINT_HEADER(dim, 0);
    PERFORMANCE(timer_end - timer_start, dim, 0, 1);
    VERIFY(c, 0, (int32_t)dim, (int32_t)dim, 0, (int32_t)dim, A_a, A_b, A_c, B_a, B_b, B_c, 0);
  #else

    ////////////////
    // Multi Core //
    ////////////////

    uint32_t core_id = mempool_get_core_id();
    uint32_t core_id_tile = core_id%CORES_PER_TILE;
    uint32_t tile_id = core_id/CORES_PER_TILE;
    uint32_t num_cores = mempool_get_core_count();

    uint32_t timer_start, timer_end, timer;
    uint32_t row_start, row_end;

    uint32_t m_start, m_end;
    uint32_t p_start, p_end;
    uint32_t vlen;
    uint32_t dim;
    bool p_split;
    uint32_t measure_iterations;

    // Initialize multicore barrier
    mempool_barrier_init(core_id);

    // Reset timer
    timer = (uint32_t)-1;

    // Set matrix dimension
    dim = MATRIX_DIM;

    // Define execution parameters
    switch (MATRIX_DIM) {
      case 8:
        vlen = 4;
        p_split = true;
        measure_iterations = 8;
        break;
      case 16:
        vlen = 8;
        p_split = true;
        measure_iterations = 4;
        break;
      case 32:
        vlen = 16;
        p_split = true;
        measure_iterations = 4;
        break;
      case 64:
        vlen = 16;
        p_split = false;
        measure_iterations = 2;
        break;
      default:
        return 1;
    }

    // Define way to split up matrix
    if (p_split) {
      // P-Split
      m_start = dim/TILES*(tile_id);
      m_end   = dim/TILES*(tile_id+1);
      p_start = dim/2*core_id_tile;
      p_end   = dim/2*(core_id_tile+1);
    } else {
      // M-Split
      m_start = dim/(TILES*CORES_PER_TILE)*(core_id);
      m_end   = dim/(TILES*CORES_PER_TILE)*(core_id+1);
      p_start = 0;
      p_end   = dim;
    }

    // Initialize matrices
    row_start = dim/(TILES*CORES_PER_TILE)*(core_id);
    row_end   = dim/(TILES*CORES_PER_TILE)*(core_id+1);
    init_matrix(a, row_start, row_end, dim, A_a, A_b, A_c);
    init_matrix(b, row_start, row_end, dim, B_a, B_b, B_c);

    // Execute matmul a few times and measure runtime
    for (uint32_t i = 0; i < measure_iterations; i++) {
      // Wait for all cores to finish
      mempool_barrier(num_cores);

      // Start timer
      timer_start = mempool_get_timer();

      // Calculate matmul
      #if MATRIX_DIM == 8
        MATMUL_2XVLEN_PARA(c, a, b, m_start, m_end, dim, p_start, p_end, dim, vlen);
      #elif MATRIX_DIM == 16
        MATMUL_4XVLEN_PARA(c, a, b, m_start, m_end, dim, p_start, p_end, dim, vlen);
      #else
        MATMUL_8XVLEN_PARA(c, a, b, m_start, m_end, dim, p_start, p_end, dim, vlen);
      #endif
      // Wait for all cores to finish matmul
      mempool_barrier(num_cores);

      // End timer and check if new best runtime
      timer_end = mempool_get_timer();
      uint32_t timer_temp = timer_end - timer_start;
      if (core_id == 0) {
        if (timer_temp < timer) {
          timer = timer_temp;
        }
      }
    }

    // Check and display results
    PRINT_HEADER(dim, core_id);
    PERFORMANCE(timer, dim, core_id, num_cores);
    VERIFY(c, (int32_t)m_start, (int32_t)m_end, (int32_t)dim, (int32_t)p_start,
           (int32_t)p_end, A_a, A_b, A_c, B_a, B_b, B_c, core_id);

    // Wait for core 0 to finish displaying results
    mempool_barrier(num_cores);
  #endif

  return 0;
}
