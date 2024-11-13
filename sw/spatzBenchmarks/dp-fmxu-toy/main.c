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

// Author: Yichao Zhang, ETH Zurich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include "data/data_8_4_4.h"
#include "kernel/mxmatmul.c"

// Enable result check
#define CHECK

// Initialize the matrices
void init_matrix(double *matrix, const double *src,
                 const unsigned int rows_start, const unsigned int rows_end,
                 const unsigned int num_columns) {
  for (unsigned int i = rows_start; i < rows_end; ++i) {
    for (unsigned int j = 0; j < num_columns; ++j) {
      matrix[i * num_columns + j] = src[i * num_columns + j];
    }
  }
}

// Verify the matrices
int verify_matrix(double *matrix, const double *checksum,
                  const unsigned int num_rows, const unsigned int num_columns) {
  for (unsigned int i = 0; i < num_rows; ++i) {
    double sum = 0;
    for (unsigned int j = 0; j < num_columns; ++j) {
      // printf("The matrix result at row %d column %d is %d. \n", i, j,
      // matrix[i * num_columns + j] );
      sum += (double)matrix[i * num_columns + j];
    }
    printf("%d times sum checking. \n", i);
    printf("The golden is %d. \n", checksum[i]);
    printf("The sum is %d. \n", sum);
    if (sum != checksum[i]) {
      return i == 0 ? -1 : (int)i;
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

double *a;
double *b;
double *c;

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  uint32_t spm_size = 32;
  
  if (cid == 0) {
    // Init the cache
    l1d_init(spm_size);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  unsigned int dim = 4;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a = (double *)snrt_l1alloc(gemm_l.M * gemm_l.K * sizeof(double));
    b = (double *)snrt_l1alloc(gemm_l.K * gemm_l.N * sizeof(double));
    c = (double *)snrt_l1alloc(gemm_l.M * gemm_l.N * sizeof(double));
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, gemm_A_dram, gemm_l.M * gemm_l.K * sizeof(double));
    snrt_dma_start_1d(b, gemm_B_dram, gemm_l.K * gemm_l.N * sizeof(double));
    snrt_dma_start_1d(c, gemm_C_dram, gemm_l.M * gemm_l.N * sizeof(double));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    for (unsigned int i = 0; i < 32; i++) {
      c[i] = 0;
      // printf("The martix a[%d]=%x, c[%d]=%x, \n", i, a[i], i, c[i]);
    }
    // matmul_tiled_load_store_test(c, a, b, dim);
    matmul_tiled_mxmacc_test(c, a, b, dim);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // End dump
  double checksum = 0;
  if (cid == 0) {
    // Print first 32 elements of matrix C
    for (unsigned int i = 0; i < 32; i++) {
      printf("The martix c[%d]=%f, \n", i, c[i]);
    }
#ifdef CHECK
    // Calculate and print checksums
    for (unsigned int i = 0; i < gemm_l.M; i++) {
      checksum = 0;
      for (unsigned int j = 0; j < gemm_l.N; j++) {
        checksum += c[i * gemm_l.N + j];
      }
      printf("Checksum[%d]=%f\n", i, checksum);
      double diff = checksum - (double)gemm_checksum[i];
      if (diff < 0)
        diff = -diff;
      if (diff > 0.001) {
        return i == 0 ? -1 : (int)i;
      }
    }
#endif
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return 0;
}
