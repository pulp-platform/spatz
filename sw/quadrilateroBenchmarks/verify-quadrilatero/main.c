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
// Author: Danilo Cammarata, ETH Zurich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/verify-quadrilatero.c"

float *a;
float *b;
float *c;

// Verify the matrices
int verify_matrix(float *matrix, const float *checksum,
                  const unsigned int num_rows, const unsigned int num_columns) {
  for (unsigned int i = 0; i < num_rows; ++i) {
    float sum = 0;
    for (unsigned int j = 0; j < num_columns; ++j) {
      sum += (float)matrix[i * num_columns + j];
    }

    float diff = sum - (float)checksum[i];
    if (diff < 0)
      diff = -diff;
    if (diff > 0.001) {
      return i == 0 ? -1 : (int)i;
    }
  }
  return 0;
}

int check_results(float *matrix, const float *expected, int N, int M)
{
  // check
  int i, j;
  int err = 0;

  // Check errors
  for(i = 0; i < M; i++) {
      for(j = 0; j < N; j++) {
         float diff;
         diff = matrix[i*N+j] - expected[i*N+j];
         diff = diff * (diff >= 0);
         if(diff > 0.001f) {
           if(i==0 && j==0) return -1;
           else return i*N+j;
          }
      }
  }

  return err;
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
    a = (float *)snrt_l1alloc(gemm_l.M * gemm_l.K * sizeof(float));
    b = (float *)snrt_l1alloc(gemm_l.K * gemm_l.N * sizeof(float));
    c = (float *)snrt_l1alloc(gemm_l.M * gemm_l.N * sizeof(float));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // Set matrix dimension
  kernel_size = QUAD_RLEN/32;

  // Work over complete P dimension
  p_start = 0;
  p_end = gemm_l.N;
  m_start = (gemm_l.M / num_cores) * cid;
  m_end = (gemm_l.M / num_cores) * (cid + 1);

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, gemm_A_dram, gemm_l.M * gemm_l.K * sizeof(float));
    snrt_dma_start_1d(b, gemm_B_dram, gemm_l.K * gemm_l.N * sizeof(float));
    // snrt_dma_start_1d(c, gemm_C_dram, gemm_l.M * gemm_l.N * sizeof(float));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start dump
  start_kernel();
  
  // printf("Matrix Load Test started\n");
  // test_load (a, gemm_l.K);
  // printf("Matrix Load Test completed successfully\n");

  // printf("Matrix Store Test started\n");
  // test_store (a, c, gemm_l.K);
  // printf("Matrix Store Test completed successfully\n");

  // printf("Matrix Load/Store Test started\n");
  // test_load_store (a, c, gemm_l.K);
  // printf("Matrix Load/Store Test completed successfully\n");

  // printf("Matrix MACC Test started\n");
  // test_macc (a,b,c, gemm_l.K);
  // printf("Matrix MACC Test completed successfully\n");
  verify_quadrilatero (a,b,c, gemm_l.K);
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // End dump
  stop_kernel();

    
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return 0;
}
