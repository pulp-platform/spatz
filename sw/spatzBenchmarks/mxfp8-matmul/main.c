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
#include "kernel/mxfp8-matmul.c"

char *a;
char *b;
char *a_scale;
char *b_scale;
float *c;

void copy_transpose_matrix(char *dst, const char *src, const uint32_t m, const uint32_t n) {
  for (uint32_t i = 0; i < m; i++) {
    for (uint32_t j = 0; j < n; j++) {
      dst[j * m + i] = src[i * n + j];
    }
  }
}

// - transpoes and shuffles B matrix for use with outer_sdotp kernels
// - assumes input matrix is in column-major order
//
// data layout in memory where b[i][j] is element of row i, column j:
//
// b[0][0] b[1][0] b[0][1] b[1][1] ... b[0][cols-1] b[1][cols-1]  (2*cols items)
// b[2][0] b[3][0] b[2][1] b[3][1] ... b[2][cols-1] b[3][cols-1]
// ...
// b[rows-2][0] b[rows-1][0]       ... b[rows-2][cols-1] b[rows-1][cols-1]
void copy_shuffle_matrix_for_sdotp(char *dst, const char *src, const uint32_t rows, const uint32_t cols) {
  for (uint32_t i = 0; i < rows; i += 2) {
    for (uint32_t j = 0; j < cols; j++) {
      dst[i * cols + 2 * j]     = src[j * rows + i];
      dst[i * cols + 2 * j + 1] = src[j * rows + i + 1];
    }
  }
}

bool verify_matrix(const float *actual, const float *expected, const uint32_t m, const uint32_t n) {
  for (uint32_t i = 0; i < m; i++) {
    for (uint32_t j = 0; j < n; j++) {
      uint32_t idx = i * n + j;
      float exp = expected[idx];
      float act = actual[idx];
      float diff = fabsf(exp - act);
      float eps = 0.05 * fabsf(exp);
      if (diff > eps) {
        printf("Error in c[%d][%d]:\n", i, j);
        printf(" acutal   = 0x%08x = %.4f\n", *(uint32_t*)&act, act);
        printf(" expected = 0x%08x = %.4f\n", *(uint32_t*)&exp, exp);
        return false;
      }
    }
  }
  printf("Success: checked %dx%d matrix elements\n", m, n);
  return true;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer;

  unsigned int m = mx_matmul_l.M;
  unsigned int n = mx_matmul_l.N;
  unsigned int k = mx_matmul_l.K;
  unsigned int k_block = k / MX_BLOCK_SIZE;

  // Check data consistency
  if (cid == 0) {
    if (k % MX_BLOCK_SIZE != 0) {
      printf("K is not a multiple of MX_BLOCK_SIZE\n");
      return 1;
    }
    if (mx_matmul_l.dtype_elements != FP8) {
      printf("unsupported element dtype\n");
      return 1;
    }
    if (mx_matmul_l.dtype_scales != E8M0) {
      printf("unsupported scale dtype\n");
      return 1;
    }
    if (mx_matmul_l.dtype_results != FP32) {
      printf("unsupported result dtype\n");
      return 1;
    }
  }

  bool mxdotp = true;
  bool skip_check = false;
  bool natural_layout = false;
  bool sdotp = false;

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a = (char *)snrt_l1alloc(m * k);
    b = (char *)snrt_l1alloc(n * k);
    a_scale = (char *)snrt_l1alloc(m * k_block);
    b_scale = (char *)snrt_l1alloc(n * k_block);
    c = (float *)snrt_l1alloc(m * n * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, mx_matmul_A_elements_dram, m * k);
    snrt_dma_start_1d(a_scale, mx_matmul_A_scales_dram, m * k_block);
    if (mxdotp) {
      // b in column-major layout
      snrt_dma_start_1d(b, mx_matmul_B_elements_dram, n * k);
      // b_scale in row-major layout
      copy_transpose_matrix(b_scale, mx_matmul_B_scales_dram, n, k_block);
    } else if (natural_layout || skip_check) {
      snrt_dma_start_1d(b, mx_matmul_B_elements_dram, n * k);
      snrt_dma_start_1d(b_scale, mx_matmul_B_scales_dram, n * k_block);
    } else {
      // test data has B in column-major format
      if (sdotp) {
        // complex shuffling for outer product with sdotp
        copy_shuffle_matrix_for_sdotp(b, mx_matmul_B_elements_dram, k, n);
        copy_transpose_matrix(b_scale, mx_matmul_B_scales_dram, n, k_block);
      } else {
        // transpose for row-major format
        copy_transpose_matrix(b, mx_matmul_B_elements_dram, n, k);
        copy_transpose_matrix(b_scale, mx_matmul_B_scales_dram, n, k_block);
      }
    }
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  // tile on output row dimension (M)
  uint32_t     local_m       = m / num_cores;
  uint32_t     local_n       = n;
  uint32_t     local_k       = k;
  const char  *local_a       = a       + (cid * local_m * k);
  const char  *local_b       = b;
  const char  *local_a_scale = a_scale + (cid * local_m * k_block);
  const char  *local_b_scale = b_scale;
  float       *local_c       = c       + (cid * local_m * n);

  snrt_cluster_hw_barrier();

  for (int i = 0; i < 2; i++) {
    if (cid == 0 && i == 1) {
      // Start timer
      timer = benchmark_get_cycle();
      // Start dump
      start_kernel();
    }

    if (mxdotp) {
      mxfp8_matmul_fp32_outer_mxdotp_lmul2_8x(
        local_c, local_a, local_b, local_a_scale, local_b_scale,
        local_m, local_n, local_k);
    } else if (natural_layout) {
      if (sdotp) {
        mxfp8_matmul_fp32_inner_sdotp_4x(
          local_c, local_a, local_b, local_a_scale, local_b_scale,
          local_m, local_n, local_k);
      } else {
        mxfp8_matmul_fp32_inner_4x(
          local_c, local_a, local_b, local_a_scale, local_b_scale,
          local_m, local_n, local_k);
      }
    } else {
      if (sdotp) {
        mxfp8_matmul_fp32_outer_sdotp_lmul4_2x(
          local_c, local_a, local_b, local_a_scale, local_b_scale,
          local_m, local_n, local_k);
      } else {
        mxfp8_matmul_fp32_outer_lmul4_2x(
          local_c, local_a, local_b, local_a_scale, local_b_scale,
          local_m, local_n, local_k);
      }
    }

    snrt_cluster_hw_barrier();

    if (cid == 0 && i == 1) {
      // End dump
      stop_kernel();
      // End timer
      timer = benchmark_get_cycle() - timer;
    }
  }

  // Performance summary
  if (cid == 0) {
    long unsigned int performance =
        1000 * 2 * m * n * k / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE * 2);
    if (sdotp)
      utilization /= 2;

    printf("\n----- (%d,%d,%d) mxfp8 matmul -----\n", m, n, k);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
            performance, utilization);
  }

  // Check results
  if (cid == 0 && !skip_check) {
    bool success = verify_matrix(c, mx_matmul_C_results_dram, m, n);
    if (!success)
      return 1;
  }
  return 0;
}
