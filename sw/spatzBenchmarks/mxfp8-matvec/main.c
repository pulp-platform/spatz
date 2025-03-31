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

#include <benchmark.h>
#include <snrt.h>
#include <stdbool.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/mxfp8-matvec.c"

char *a;
char *b;
char *a_scale;
char *b_scale;
float *c;

bool verify_vector(const float *actual, const float *expected, const uint32_t m) {
  for (uint32_t i = 0; i < m; i++) {
    float exp = expected[i];
    float act = actual[i];
    float diff = fabsf(exp - act);
    float eps = 0.05 * fabsf(exp);
    if (diff > eps) {
      printf("Error in c[%d]:\n", i);
      printf(" acutal   = 0x%08x\n", *(uint32_t*)&act);
      printf(" expected = 0x%08x\n", *(uint32_t*)&exp);
      return false;
    }
  }
  printf("Success: checked %d vector elements\n", m);
  return true;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer;

  unsigned int m = mx_matvec_l.M;
  unsigned int n = mx_matvec_l.N;
  unsigned int n_block = n / MX_BLOCK_SIZE;

  // Check data consistency
  if (cid == 0) {
    if (n % MX_BLOCK_SIZE != 0) {
      printf("N is not a multiple of MX_BLOCK_SIZE\n");
      return 1;
    }
    if (mx_matvec_l.dtype_elements != FP8) {
      printf("unsupported element dtype\n");
      return 1;
    }
    if (mx_matvec_l.dtype_scales != E8M0) {
      printf("unsupported scale dtype\n");
      return 1;
    }
    if (mx_matvec_l.dtype_results != FP32) {
      printf("unsupported result dtype\n");
      return 1;
    }
  }

  bool skip_check = false;
  bool natural_layout = true;
  bool sdotp = false;

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a = (char *)snrt_l1alloc(m * n);
    b = (char *)snrt_l1alloc(n);
    a_scale = (char *)snrt_l1alloc(m * n_block);
    b_scale = (char *)snrt_l1alloc(n_block);
    c = (float *)snrt_l1alloc(m * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, mx_matvec_A_elements_dram, m * n);
    snrt_dma_start_1d(a_scale, mx_matvec_A_scales_dram, m * n_block);
    if (natural_layout || skip_check) {
      snrt_dma_start_1d(b, mx_matvec_B_elements_dram, n);
      snrt_dma_start_1d(b_scale, mx_matvec_B_scales_dram, n_block);
    } else {
      printf("TODO\n");
      return 1;
    }
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  // tile on output dimension (M)
  uint32_t     local_m       = m / num_cores;
  uint32_t     local_n       = n;
  const char  *local_a       = a       + (cid * local_m * n);
  const char  *local_b       = b;
  const char  *local_a_scale = a_scale + (cid * local_m * n_block);
  const char  *local_b_scale = b_scale;
  float       *local_c       = c       + (cid * local_m);

  snrt_cluster_hw_barrier();

  // Start timer
  timer = benchmark_get_cycle();

  // Start dump
  if (cid == 0)
    start_kernel();

  if (natural_layout) {
    if (sdotp) {
      mxfp8_matvec_fp32_inner_sdotp_4x(
        local_c, local_a, local_b, local_a_scale, local_b_scale,
        local_m, local_n);
    } else {
      mxfp8_matvec_fp32_inner_4x(
        local_c, local_a, local_b, local_a_scale, local_b_scale,
        local_m, local_n);
    }
  } else {
    if (sdotp) {
      printf("TODO\n");
      return 1;
    } else {
      printf("TODO\n");
      return 1;
    }
  }

  snrt_cluster_hw_barrier();

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer
  timer = benchmark_get_cycle() - timer;

  // Performance summary
  if (cid == 0) {
    long unsigned int performance =
        1000 * 2 * m * n / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE * 2);
    if (sdotp)
      utilization /= 2;

    printf("\n----- (%d,%d) mxfp8 matvec -----\n", m, n);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
            performance, utilization);
  }

  // Check results
  if (cid == 0 && !skip_check) {
    bool success = verify_vector(c, mx_matvec_C_results_dram, m);
    if (!success)
      return 1;
  }
  return 0;
}
