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

// Author: Enis Mustafa, ETH Zurich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

// -------------------  IMPORT  --------------------//
// Size of Marices import from `data/data_gemm.h`;
// The matrices generation scripts under `script/`
// Usage: `python3 gen_data.python -c matmul.json`;
// -------------------------------------------------//
#include DATAHEADER
#include "kernel/mxfmatmul.c"

// --------------  DEFINE TILE SIZE  ---------------//
// Define Tile (Kernel) Dimensions:
// C = AB with A=[MxK], B=[KxN], C=[MxN];
// Martix B is transposed in the memory: B_Trans=[NxK]
// Note: Accumulator size = (KERNEL_M x KERNEL_N)/4;
// We enforce KERNEL_M * KERNEL_K == vl
// We enforce KERNEL_N * KERNEL_K <= vl, i.e., KERNEL_N <= KERNEL_M
// We enforce KERNEL_M * KERNEL_N <= vl, i.e., KERNEL_N <= KERNEL_K
// We enforce KERNEL_M, KERNEL_N, and KERNEL_K to be either 4 or 8
// Mind that KERNEL_M * KERNEL_K <= maxvl for the used LMUL
// -------------------------------------------------//
#ifndef KERNEL_M
#define KERNEL_M 8
#endif
#ifndef KERNEL_N
#define KERNEL_N 4
#endif
#ifndef KERNEL_K
#define KERNEL_K 4
#endif

#if (KERNEL_N > KERNEL_K) || (KERNEL_N > KERNEL_M)
#error "KERNEL_N should be lower than KERNEL_K and KERNEL_M"
#endif

//#define B2 1

// --------------  RUNTIME PARAMETERS  -------------//
// For MemPool_Spatz4: 4 Groups with 64 Cores in total;
// For MemPool_Spatz2: 4 Groups with 128 Cores in total;
// -------------------------------------------------//
#define ACTIVE_GROUPS NUM_GROUPS

// -----------------  FUNCTIONS  -------------------//
// `init_matrix`  : Import matrix from L2 to L1
// `verify_matrix`: Compare the sum of the row elements.
// `print_matrix` : Matrices printing by HEX
// -------------------------------------------------//
void init_matrix(double *matrix, const double *src,
                 const unsigned int rows_start, const unsigned int rows_end,
                 const unsigned int num_columns) {
  for (unsigned int i = rows_start; i < rows_end; ++i) {
    for (unsigned int j = 0; j < num_columns; ++j) {
      matrix[i * num_columns + j] = src[i * num_columns + j];
    }
  }
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

// ----------------  Main Program  ------------------//
// Row-based parallelization by M dimension of Matrix A;
// First,  determine how many cores parallelized per
//         tile on Matrix A, based on M/KERNEL_M;
// Second, determine how many tiles need to be done per
//         core on Matrix B, based on `tiles_per_core`=
//         (N/(M/KERNEL_M))/KERNEL_N;
// Double buffering require `tiles_per_core` % 2 = 0;
// -------------------------------------------------//

#define CHECK
//#define PRINT_RESULT

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int measure_iterations = 3;

  unsigned int timer_start, timer_end, timer;

  unsigned int m_start, m_end;
  unsigned int p_start, p_end;
  unsigned int kernel_size;

  // Todo: we need to clarify the vl
  // It can be M*K, K*N, or M*N
  unsigned int vl = KERNEL_M * KERNEL_K;

#if USE_CACHE == 0
  uint32_t spm_size = 120; // 120 KB out of 128 KB
#else
  uint32_t spm_size = 16; // Reserve small portion for SPM
#endif

  if (cid == 0) {
    // Init the cache
    l1d_init(spm_size);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

#if USE_CACHE == 0
  // Allocate the matrices in the local tile
  if (cid == 0) {
    a = (double *)snrt_l1alloc(gemm_l.M * gemm_l.K * sizeof(double));
    b = (double *)snrt_l1alloc(gemm_l.K * gemm_l.N * sizeof(double));
    c = (double *)snrt_l1alloc(gemm_l.M * gemm_l.N * sizeof(double));
  }
#else
  a = gemm_A_dram;
  b = gemm_B_dram;
  c = gemm_C_dram;
#endif

  // Reset timer
  timer = (unsigned int)-1;

  // Work over complete P dimension with one core only
  p_start = 0;
  p_end = gemm_l.N;
  m_start = (gemm_l.M / num_cores) * cid;
  m_end = (gemm_l.M / num_cores) * (cid + 1);

  // Parallelization partition control
  uint32_t nrelem_a = KERNEL_M * gemm_l.K;
  uint32_t nrelem_b = KERNEL_N * gemm_l.K;
  uint32_t nrelem_c = KERNEL_M * gemm_l.N;

  // Work over complete K dimension
  unsigned int inner_loops = gemm_l.K / KERNEL_K;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

#if USE_CACHE == 0
  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, gemm_A_dram, gemm_l.M * gemm_l.K * sizeof(double));
    snrt_dma_start_1d(b, gemm_B_dram, gemm_l.K * gemm_l.N * sizeof(double));
    snrt_dma_start_1d(c, gemm_C_dram, gemm_l.M * gemm_l.N * sizeof(double));
    snrt_dma_wait_all();
  }
#endif

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate matmul
  for (unsigned int i = 0; i < measure_iterations; ++i) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0 && (i == 2))
      start_kernel();

#ifdef B2
    matmul_tiled_Bx2(c, a, b, KERNEL_M, KERNEL_N, KERNEL_K, gemm_l.N, gemm_l.K,
                     inner_loops, m_start, m_end, p_end, vl, nrelem_a, nrelem_b,
                     nrelem_c);
#else
    matmul_tiled_Bx4(c, a, b, KERNEL_M, KERNEL_N, KERNEL_K, gemm_l.M, gemm_l.N, gemm_l.K,
                     inner_loops, m_start, m_end, p_end, vl, nrelem_a, nrelem_b,
                     nrelem_c);
#endif

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0 && (i == 2))
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    unsigned int timer_temp = timer_end - timer_start;
    if (cid == 0) {
      if (timer_temp < timer) {
        timer = timer_temp;
      }
    }

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();
  }

  // Wait for all cores finish
  snrt_cluster_hw_barrier();

  // Check and display results
  if (cid == 0) {
    long unsigned int performance =
        1000 * 2 * gemm_l.M * gemm_l.N * gemm_l.K / timer;
    long unsigned int utilization = performance / (2 * 2 * 4);

    printf("\n----- (%dx%d) dp fmatmul -----\n", gemm_l.M, gemm_l.N);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0) {
#ifdef PRINT_RESULT
    // Print results one by one.
    for (unsigned int i = 0; i < gemm_l.M; ++i) {
      for (unsigned int j = 0; j < gemm_l.N; ++j) {
        printf("C[%d][%d] = %f\n", i, j, c[i * gemm_l.N + j]);
      }
    }
#endif
#ifdef CHECK
    // Calculate and print checksums
    for (unsigned int i = 0; i < gemm_l.M; i++) {
      double checksum = 0;
      for (unsigned int j = 0; j < gemm_l.N; j++) {
        checksum += c[i * gemm_l.N + j];
      }
      // printf("Checksum[%d]=%f\n", i, checksum);
      double diff = checksum - (double)gemm_checksum[i];
      if (diff < 0)
        diff = -diff;
      if (diff > 0.001) {
        return i == 0 ? -1 : (int)i;
      }
    }
#endif
  }

  // Wait for all cores finish
  snrt_cluster_hw_barrier();
  set_eoc();

  return 0;
}
