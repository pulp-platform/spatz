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
// Author: Ettore Rayneri, ETH Zurich

#include <benchmark.h>
#include <quadrilatero.h>
#include <snrt.h>
#include <stdio.h>

// Data: generate with scripts/gen_simple_data.sh before building (the default gendata uses --transA 1)
#include DATAHEADER

#ifndef B_OFF_W
#define B_OFF_W 0   // offset of Bt in L1, in words (bank-conflict experiments, e.g. QUAD_RLEN/32)
#endif

// Simple (non-pipelined) INT32 matmul, transposed loads/store (include/matmul_simple_t.h): Ct = (A*B)^T
// Data generated with --transA 0 --transB 1: gemm_A_dram = A (M x K), gemm_B_dram = Bt (N x K),
// gemm_C_dram = C (M x N, not transposed): the check compares Ct with C^T.
void matmul_simple_t_INT32_INT32_INT32(void *addrA, void *addrBt, void *addrCt, int K, int N, int M, int widening);

void *a;
void *b;
int32_t *c;

int check_results(int32_t *matrix, const int32_t *expected, int N, int M)
{
  // check
  int i, j;
  int err = 0;

  // Check errors
  for(i = 0; i < M; i++) {
    for(j = 0; j < N; j++) {
      if(matrix[j*M+i] != expected[i*N+j]) {   // matrix = Ct (N x M)
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

  const unsigned int measure_iterations = 2;   // the best one runs with a warm instruction cache

  unsigned int timer_start, timer_end, timer;

  // Allocate the matrices in the local tile (b last: B_OFF_W moves only b)
  if (cid == 0) {
    c = (void *)snrt_l1alloc(gemm_l.M * gemm_l.N * sizeof(int32_t));
    a = (void *)snrt_l1alloc(gemm_l.M * gemm_l.K * sizeof(int32_t));
    b = (void *)((int32_t *)snrt_l1alloc((gemm_l.N * gemm_l.K + B_OFF_W) * sizeof(int32_t)) + B_OFF_W);
  }

  // Reset timer
  timer = (unsigned int)-1;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, gemm_A_dram, gemm_l.M * gemm_l.K * sizeof(int32_t));
    snrt_dma_start_1d(b, gemm_B_dram, gemm_l.N * gemm_l.K * sizeof(int32_t));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate matmul
  for (unsigned int i = 0; i < measure_iterations; ++i) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 1){
      start_kernel();
      matmul_simple_t_INT32_INT32_INT32(a, b, c, gemm_l.K, gemm_l.N, gemm_l.M, 0);
    }

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();
    if (cid == 1) stop_kernel();

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

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
        performance / (2 * QUAD_RLEN/32 * QUAD_RLEN/32);

    printf("\n----- (%dx%dx%d) INT32 simple transposed matmul, B_OFF_W=%d -----\n", gemm_l.M, gemm_l.K,gemm_l.N, B_OFF_W);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0) {
    int error =
      check_results(c, (const int32_t *)gemm_C_dram, gemm_l.N, gemm_l.M);

    if (error != 0) {
      int e = error < 0 ? 0 : error;   // index in C: e = i*N + j -> Ct[j*M + i]
      printf("Error core %d: c[%d]=%d\n", cid, error, c[(e % gemm_l.N) * gemm_l.M + e / gemm_l.N]);
      return error;
    }
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return 0;
}