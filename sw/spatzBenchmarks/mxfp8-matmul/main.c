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
// Author: Max Wipfli <mwipfli@ethz.ch>

#include "kernel/mxfp8-matmul.h"
#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

// #include DATAHEADER
#include "kernel/mxfp8-matmul.c"

char *a;
char *b;
char *a_scale;
char *b_scale;
float *c;

#define M 64
#define N 64
#define K 64

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

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  if (cid != 0)
    return 0;

  unsigned int timer_start, timer_end, timer;

  // Allocate the matrices in the local tile
  a = (char *)snrt_l1alloc(M * K);
  b = (char *)snrt_l1alloc(N * K);
  a_scale = (char *)snrt_l1alloc(M * K / 32);
  b_scale = (char *)snrt_l1alloc(N * K / 32);
  c = (float *)snrt_l1alloc(M * N * sizeof(float));

  memset(a, 0x3c, M * K); // 1.0 in FP8
  memset(b, 0x3c, N * K); // 1.0 in FP8
  memset(a_scale, 0x7f, M * K / 32); // 1 in E8M0
  memset(b_scale, 0x7f, N * K / 32); // 1 in E8M0

  // Start timer
  timer = benchmark_get_cycle();

  // Start dump
  start_kernel();

  mxfp8_matmul_fp32_dotp(c, a, b, a_scale, b_scale, M, N, K);

  // End dump
  stop_kernel();

  // End timer
  timer = benchmark_get_cycle() - timer;

  // Check and display results
  long unsigned int performance =
      1000 * 2 * M * N * K / timer;
  long unsigned int utilization =
      performance / (2 * num_cores * SNRT_NFPU_PER_CORE * 2);

  printf("\n----- (%d,%d,%d) mxpf8 matmul -----\n", M, N, K);
  printf("The execution took %u cycles.\n", timer);
  printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
          performance, utilization);

  printf("c[0][0] = %.3f\n", c[0]);
  printf("c[0][1] = %.3f\n", c[1]);
  printf("c[1][0] = %.3f\n", c[N]);

  return 0;
}
