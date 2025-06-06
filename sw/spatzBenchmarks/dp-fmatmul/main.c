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

// Author: Matheus Cavalcante, ETH Zurich

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/dp-fmatmul.c"

double *a;
double *b;
double *c;

// Verify the matrices
int verify_matrix(double *matrix, const double *checksum,
                  const unsigned int num_rows, const unsigned int num_columns) {
  for (unsigned int i = 0; i < num_rows; ++i) {
    double sum = 0;
    for (unsigned int j = 0; j < num_columns; ++j) {
      sum += (double)matrix[i * num_columns + j];
    }

    double diff = sum - (double)checksum[i];
    if (diff < 0)
      diff = -diff;
    if (diff > 0.001) {
      return i == 0 ? -1 : (int)i;
    }
  }
  return 0;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int measure_iterations = 1;

  unsigned int timer_start, timer_end, timer;

  unsigned int m_start, m_end;
  unsigned int p_start, p_end;
  unsigned int kernel_size;

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a = (double *)snrt_l1alloc(gemm_l.M * gemm_l.K * sizeof(double));
    b = (double *)snrt_l1alloc(gemm_l.K * gemm_l.N * sizeof(double));
    c = (double *)snrt_l1alloc(gemm_l.M * gemm_l.N * sizeof(double));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // Set matrix dimension
  kernel_size = 4;

  // Work over complete P dimension
  p_start = 0;
  p_end = gemm_l.N;
  m_start = (gemm_l.M / num_cores) * cid;
  m_end = (gemm_l.M / num_cores) * (cid + 1);

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

  // Calculate matmul
  for (unsigned int i = 0; i < measure_iterations; ++i) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    if (kernel_size == 2) {
      matmul_2xVL(c, a, b, m_start, m_end, gemm_l.K, gemm_l.N, p_start, p_end);
    } else if (kernel_size == 4) {
      matmul_4xVL(c, a, b, m_start, m_end, gemm_l.K, gemm_l.N, p_start, p_end);
    } else if (kernel_size == 8) {
      matmul_8xVL(c, a, b, m_start, m_end, gemm_l.K, gemm_l.N, p_start, p_end);
    } else {
      return -2;
    }

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    unsigned int timer_temp = timer_end - timer_start;
    if (cid == 0) {
      if (timer_temp < timer) {
        timer = timer_temp;
      }
    }
  }

  // Check and display results
  if (cid == 0) {
    long unsigned int performance =
        1000 * 2 * gemm_l.M * gemm_l.N * gemm_l.K / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    PRINTF("\n----- (%dx%d) dp fmatmul -----\n", gemm_l.M, gemm_l.N);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0) {
    int error =
        verify_matrix(c, (const double *)gemm_checksum, gemm_l.M, gemm_l.N);

    if (error != 0) {
      PRINTF("Error core %d: c[%d]=%u\n", cid, error, (int)c[error]);
      return error;
    }
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return 0;
}
