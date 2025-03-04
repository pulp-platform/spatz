// Copyright 2024 ETH Zurich and University of Bologna.
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

// Author: Diyou Shen, ETH Zurich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/dp-db-fmatmul.c"

#define DEBUG

#ifndef KERNEL_SIZE
#define KERNEL_SIZE 4
#endif

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

double *matrix_a1, *matrix_a2;
double *matrix_b1, *matrix_b2;
double *matrix_c1, *matrix_c2;

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // This configuration can take up to 128x128x128 matmul (peak 104 KiB SPM)
  // Adjust the M and N depends on SPM size and matmul size
  const unsigned int matrix_M = ((gemm_l.M * gemm_l.K) > 2048) ? (2048/gemm_l.K) : gemm_l.M;
  const unsigned int matrix_K = gemm_l.K;
  const unsigned int matrix_N = ((gemm_l.N * gemm_l.K) > 4096) ? (4096/gemm_l.K) : gemm_l.N;
  unsigned int timer_start, timer_end, timer;

  unsigned int m_start, m_end;
  unsigned int n_start, n_end;
  unsigned int kernel_size;

  const unsigned int A_size = matrix_M * matrix_K;
  const unsigned int B_size = matrix_K * matrix_N;
  const unsigned int C_size = matrix_M * matrix_N;

  // Total number of in and out loops
  const int tot_out = gemm_l.N / matrix_N;
  const int tot_in  = gemm_l.M / matrix_M;

  uint32_t spm_size = 127;

  if (cid == 0) {
    // Init the cache
    l1d_init(spm_size);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Allocate the L1 region for double buffering matrices
  if (cid == 0) {
    if (tot_in > 1) {
      // Divide entire SPM into two half for double buffering
      matrix_a1 = (double *)snrt_l1alloc(A_size * sizeof(double));
      matrix_a2 = (double *)snrt_l1alloc(A_size * sizeof(double));
      matrix_b1 = (double *)snrt_l1alloc(B_size * sizeof(double));
      if (tot_out > 1) {
        matrix_b2 = (double *)snrt_l1alloc(B_size * sizeof(double));
      } else {
        matrix_b2 = 0;
      }
      matrix_c1 = (double *)snrt_l1alloc(C_size * sizeof(double));
      matrix_c2 = (double *)snrt_l1alloc(C_size * sizeof(double));
      printf("Tiled matrix size:\n");
      printf("A:%ux%u\n", matrix_M, matrix_K);
      printf("B:%ux%u\n", matrix_K, matrix_N);
      printf("C:%ux%u\n", matrix_M, matrix_N);

      printf("Allocate memory to the following regions:\n");
      printf("A_size:%p,a1:%p,a2:%p\n", A_size * sizeof(double), matrix_a1, matrix_a2);
      printf("B_size:%p,b1:%p,b2:%p\n", B_size * sizeof(double), matrix_b1, matrix_b2);
      printf("C_size:%p,c1:%p,c2:%p\n", C_size * sizeof(double), matrix_c1, matrix_c2);
    } else {
      // Divide entire SPM into two half for double buffering
      matrix_a1 = (double *)snrt_l1alloc(A_size * sizeof(double));
      matrix_b1 = (double *)snrt_l1alloc(B_size * sizeof(double));
      matrix_c1 = (double *)snrt_l1alloc(C_size * sizeof(double));
      printf("Tiled matrix size:\n");
      printf("A:%ux%u\n", matrix_M, matrix_K);
      printf("B:%ux%u\n", matrix_K, matrix_N);
      printf("C:%ux%u\n", matrix_M, matrix_N);

      printf("Allocate memory to the following regions:\n");
      printf("A_size:%p,a1:%p\n", A_size * sizeof(double), matrix_a1);
      printf("B_size:%p,b1:%p\n", B_size * sizeof(double), matrix_b1);
      printf("C_size:%p,c1:%p\n", C_size * sizeof(double), matrix_c1);
    }

  }

  double *a_comp;
  double *b_comp;
  double *c_comp;
  double *a_dma;
  double *b_dma;
  double *c_dma;
  double *a_l2;
  double *b_l2;
  double *c_l2, *c_l2_st;

  // Reset timer
  timer = (unsigned int)-1;

  // Set matrix dimension
  kernel_size = KERNEL_SIZE;

  // Work load distribution across cores:
  // All cores take entire Mat B, and horizontally distribute on A
  n_start = 0;
  n_end   = matrix_N;
  m_start = (matrix_M / num_cores) *  cid;
  m_end   = (matrix_M / num_cores) * (cid + 1);

  a_l2    = gemm_A_dram;
  b_l2    = gemm_B_dram;
  c_l2    = gemm_C_dram;
  c_l2_st = gemm_C_dram;

  if (cid == 0) {
    printf("total in loops:%u, out loops:%u\n\n", tot_in, tot_out);
  #ifdef DEBUG
    printf("L2 a:%p, b:%p, c:%p\n", (void*)a_l2, (void*)b_l2, (void*)c_l2);
    printf("L1 a:%p, b:%p, c:%p\n", (void*)matrix_a1, (void*)matrix_b1, (void*)matrix_c1);
  #endif
    // Copy first parts of matrix for initialization
    snrt_dma_start_1d(matrix_a1, a_l2, A_size * sizeof(double));

    // We want to load a K-by-N matrix to SPM, each stride has length of N
    // then it jumps to next row repeating K times
    snrt_dma_start_2d(matrix_b1, b_l2, matrix_N * sizeof(double),
                      0, gemm_l.N, matrix_K);

    // We want to load a m-by-n matrix from L2 in M-by-N, 
    // each stride has length of n then it jumps to next row repeating m times
    snrt_dma_start_2d(matrix_c1, c_l2, matrix_N * sizeof(double),
                      0, gemm_l.N, matrix_M);
    
    a_l2 += A_size;
    // The starting address of B moves right of N (each time a vertical slice)
    b_l2 += matrix_N;
    // inner loop moves c in vertical direction
    c_l2 += gemm_l.N;
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  // Start dump
  if (cid == 0)
    start_kernel();

  timer_start = benchmark_get_cycle();

  // Outer Loop of N/32 trunks, basing on B matrix with DB
  for (int out_loop = 0; out_loop < tot_out; out_loop++) {
    if (out_loop % 2 == 0) {
      b_comp = (double *)matrix_b1;
      b_dma  = (double *)matrix_b2;
    } else {
      b_dma  = (double *)matrix_b1;
      b_comp = (double *)matrix_b2;
    }
    snrt_cluster_hw_barrier();

    if (cid == 0 & (out_loop < tot_out-1)) {
      #ifdef DEBUG
        printf("pre-load b from %p to %p\n", (void*)b_l2, (void*)b_dma);
      #endif
      // Load next part of B if it has
      // We want to load a K-by-N matrix to SPM, each stride has length of N
      // then it jumps to next row repeating K times
      snrt_dma_start_2d(b_dma, b_l2, matrix_N * sizeof(double),
                        0, gemm_l.N, matrix_K);
      // The starting address of B moves right of N (each time a vertical slice)
      b_l2 += matrix_N;
    }

    c_l2_st = gemm_C_dram + matrix_N * out_loop;

    // Inner Loop of 128/16 trunks, basing on A matrix with DB
    // Each round of inner loop will create 16x32 data output
    for (int in_loop = 0; in_loop < tot_in; in_loop++) {
      if (in_loop % 2 == 0) {
        a_comp = (double *)matrix_a1;
        c_comp = (double *)matrix_c1;
        a_dma  = (double *)matrix_a2;
        c_dma  = (double *)matrix_c2;
      } else {
        a_dma  = (double *)matrix_a1;
        c_dma  = (double *)matrix_c1;
        a_comp = (double *)matrix_a2;
        c_comp = (double *)matrix_c2;
      }

      snrt_cluster_hw_barrier();

      if (cid == 0) {
        if (in_loop > 0) {
        #ifdef DEBUG
          printf("store c from %p to %p\n\n", (void*)c_dma, (void*)c_l2_st);
        #endif
          // Store C back to its loaded place
          // We want to store a m-by-n matrix to L2 in M-by-N, 
          // each stride has length of n then it jumps to next row repeating m times
          snrt_dma_start_2d(c_l2_st, c_dma, matrix_N * sizeof(double),
                            gemm_l.N, 0, matrix_M);
          // inner loop moves c in horizontal direction
          // outer loop moves c in vertical direction
          c_l2_st += gemm_l.N;
          snrt_dma_wait_all();
        }
        if (in_loop < tot_in-1) {
        #ifdef DEBUG
          printf("pre-load a from %p to %p\n", (void*)a_l2, (void*)a_dma);
        #endif
          // Copy first parts of matrix for initialization
          snrt_dma_start_1d(a_dma, a_l2, A_size * sizeof(double));
          a_l2 += A_size;
        #ifdef DEBUG
          printf("pre-load c from %p to %p\n", (void*)c_l2, (void*)c_dma);
        #endif
          // We want to load a m-by-n matrix from L2 in M-by-N, 
          // each stride has length of n then it jumps to next row repeating m times
          snrt_dma_start_2d(c_dma, c_l2, matrix_N * sizeof(double),
                            0, gemm_l.N, matrix_M);
          // inner loop moves c in horizontal direction
          c_l2 += gemm_l.N;
        }
      }
      #ifdef DEBUG
      if(cid == 0) {
        printf("calc a:%p,b:%p,c:%p\n", (void*)a_comp, (void*)b_comp, (void*)c_comp);
      }
      #endif
      // Now run the new calculation
      matmul_4xVL(c_comp, a_comp, b_comp, m_start, m_end, matrix_K, matrix_N, n_start, n_end);
    }
    // Pointer reset for A and C, add in vertical direction after each round
    a_l2    = gemm_A_dram;
    c_l2    = gemm_C_dram + matrix_N * (out_loop+1);
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
  #ifdef DEBUG
    printf("store c from %p to %p\n\n", (void*)c_comp, (void*)c_l2_st);
  #endif
    // Write final trunk into dram
    snrt_dma_start_1d(c_l2_st, c_comp, C_size * sizeof(double));
    snrt_dma_wait_all();
  }
  snrt_cluster_hw_barrier();

  timer = benchmark_get_cycle() - timer_start;

  // Check and display results
  if (cid == 0) {
    stop_kernel();
    long unsigned int performance =
        1000 * 2 * gemm_l.M * gemm_l.N * gemm_l.K / timer;
    long unsigned int utilization = performance / (2 * num_cores * 4);
    write_cyc(timer);
  #ifdef PRINT_RESULT
    printf("\n----- (%dx%d) dp fmatmul -----\n", gemm_l.M, gemm_l.N);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  #endif
    l1d_flush();
  }

  snrt_cluster_hw_barrier();

  #ifdef DEBUG
  if (cid == 0) {
    printf("Check data stored in %p\n", (void*)gemm_C_dram);
    int error =
        verify_matrix(gemm_C_dram, (const double *)gemm_checksum, gemm_l.M, gemm_l.N);

    if (error != 0) {
      printf("Error core %d: c[%d]=%u\n", cid, error, (int)gemm_C_dram[error]);
      return error;
    }
  }
  #endif

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  set_eoc();

  return 0;
}
