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

// Author: Tim Frey,    ETH Zurich
//         Diyou Shen,  ETH Zurich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/dp-db-fmatmul.c"

#define SIZE_MAX 256

double c_res[SIZE_MAX * SIZE_MAX] __attribute__((section(".data"))) = {};

double *buf;

// Verify the matrices
int verify_matrix_checksum(double *matrix, const double *checksum,
                           const unsigned int num_rows,
                           const unsigned int num_columns) {
  for (unsigned int i = 0; i < num_rows; ++i) {
    double sum = 0;
    for (unsigned int j = 0; j < num_columns; ++j) {
      sum += (double)matrix[i * num_columns + j];
    }

    double diff = sum - (double)checksum[i];
    if (diff < 0) diff = -diff;
    if (diff > 0.1) {
      return -1;
    }
  }
  return 0;
}

int verify_matrix(double *matrix, const double *ground_truth,
                  const unsigned int num_rows, const unsigned int num_columns) {
  for (unsigned int i = 0; i < num_rows; ++i) {
    for (unsigned int j = 0; j < num_columns; ++j) {
      if (abs((double)ground_truth[i * num_columns + j] -
              (double)matrix[i * num_columns + j]) > 0.001) {
        printf("Error at [%d][%d]: %f != %f\n", i, j,
               (double)matrix[i * num_columns + j],
               (double)ground_truth[i * num_columns + j]);
        return i * num_columns + j + 1;
      }
    }
  }
  return 0;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  uint32_t spm_size = 124;

  if (cid == 0) {
    // Init the cache
    l1d_init(spm_size);
  }

  unsigned int timer_load, timer_store, timer_start, timer_end, timer,
      timer_kernel;

  const unsigned int BUF_SIZE = 12 * 1024;  // 10Ki words = slightly more than half L1
  // if (cid == 0) start_kernel();

  if (cid == 0) {
    timer_start = benchmark_get_cycle();

    buf = (double *)snrt_l1alloc(sizeof(double) * BUF_SIZE);
    if (buf == NULL) {
      printf("Error: Could not allocate buffer of size %d\n", BUF_SIZE);
      return -1;
    }
  }
  snrt_cluster_hw_barrier();
  debug_print("Core %d: buf at %p\n", cid, buf);

  timer_kernel = dp_fmatmul_double_buffering_kernel(
      c_res,        // result in DRAM
      gemm_A_dram,  // a in DRAM
      gemm_B_dram,  // b in DRAM
      buf, BUF_SIZE,
      gemm_l.M,  // a matrix rows
      gemm_l.K,  // a matrix columns / b matrix rows
      gemm_l.N   // b matrix columns
  );


  // Check and display results
  if (cid == 0) {
  //   stop_kernel();

    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance =
        1000 * 2 * gemm_l.M * gemm_l.N * gemm_l.K / timer_kernel;
    long unsigned int utilization =
        performance / (2 * num_cores * 4);

    printf("\n----- (%dx%d) dp fmatmul (kernel %d) -----\n", gemm_l.M, gemm_l.N,
           KERNEL_SIZE);
    printf("The execution took %u cycles. (total %u)\n", timer_kernel, timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
    printf("The matrix size is %dx%d * %dx%d = %dx%d.\n", gemm_l.M, gemm_l.K,
           gemm_l.K, gemm_l.N, gemm_l.M, gemm_l.N);
  }

  if (cid == 0) {
    int error =
        verify_matrix_checksum(c_res, gemm_checksum, gemm_l.M, gemm_l.N);
        // verify_matrix(c_res, gemm_result_dram, gemm_l.M, gemm_l.N);

    if (error != 0) {
      return error;
    }
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  set_eoc();

  return 0;
}
