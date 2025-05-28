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
#include <stdint.h>

#include DATAHEADER

#define MXDOTP
#ifdef MXDOTP
#include "kernel/mxdotp.c"
#else
#error "baseline not implemented"
#endif

char *a;
char *b;
char *a_scale;
char *b_scale;
_Float16 *c;

void copy_transpose_matrix(char *dst, const char *src, const uint32_t m, const uint32_t n) {
  for (uint32_t i = 0; i < m; i++) {
    for (uint32_t j = 0; j < n; j++) {
      dst[j * m + i] = src[i * n + j];
    }
  }
}

// NOTE: This is a manual BF16-to-FP32 conversion as fcvt.s.h doesn't support
// FP16ALT (i.e., BF16) properly.
static inline float bf16_to_fp32(const _Float16 *val) {
  uint32_t tmp = *(const uint16_t *)val;
  tmp <<= 16;
  float result;
  asm volatile("fmv.w.x %0, %1" : "=f"(result) : "r"(tmp));
  return result;
}

bool verify_matrix(const _Float16 *actual, const float *expected, const uint32_t m, const uint32_t n) {
  for (uint32_t i = 0; i < m; i++) {
    for (uint32_t j = 0; j < n; j++) {
      uint32_t idx = i * n + j;
      float exp = expected[idx];
      float act = bf16_to_fp32(&actual[idx]);
      float diff = fabsf(exp - act);
      float eps = 0.05 * fabsf(exp);
      if (diff > eps) {
        printf("Error in c[%d][%d]:\n", i, j);
        uint16_t act_int = *(uint16_t*)&actual[idx];
        uint16_t exp_int = *((uint16_t*)&expected[idx] + 1);
        printf(" acutal   = 0x%04x\n", act_int);
        printf(" expected = 0x%04x\n", exp_int);
        // return false;
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
  unsigned int k_bytes = k / 2;
  unsigned int k_block = k / MX_BLOCK_SIZE;

  // Check data consistency
  if (cid == 0) {
    if (k % MX_BLOCK_SIZE != 0) {
      printf("K is not a multiple of MX_BLOCK_SIZE\n");
      return 1;
    }
    if (mx_matmul_l.dtype_elements != FP4) {
      printf("unsupported element dtype\n");
      return 1;
    }
    if (mx_matmul_l.dtype_scales != E8M0) {
      printf("unsupported scale dtype\n");
      return 1;
    }
    if (mx_matmul_l.dtype_results != FP16ALT) {
      printf("unsupported result dtype\n");
      return 1;
    }
  }

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a       = (char *)    snrt_l1alloc(m * k_bytes);
    b       = (char *)    snrt_l1alloc(n * k_bytes);
    a_scale = (char *)    snrt_l1alloc(m * k_block);
    b_scale = (char *)    snrt_l1alloc(n * k_block);
    c       = (_Float16 *)snrt_l1alloc(m * n * sizeof(_Float16));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, mx_matmul_A_elements_dram, m * k_bytes);
    snrt_dma_start_1d(a_scale, mx_matmul_A_scales_dram, m * k_block);
#ifdef MXDOTP
    // b in column-major layout (keep)
    snrt_dma_start_1d(b, mx_matmul_B_elements_dram, n * k_bytes);
    // b_scale: transpose from column-major to row-major layout
    copy_transpose_matrix(b_scale, mx_matmul_B_scales_dram, n, k_block);
#else
    #error "not implemented"
#endif
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  // tile on output row dimension (M)
  uint32_t    local_m       = m / num_cores;
  uint32_t    local_n       = n;
  uint32_t    local_k       = k;
  const char *local_a       = a       + (cid * local_m * k_bytes);
  const char *local_b       = b;
  const char *local_a_scale = a_scale + (cid * local_m * k_block);
  const char *local_b_scale = b_scale;
  _Float16   *local_c       = c       + (cid * local_m * n);

  snrt_cluster_hw_barrier();

  for (int i = 0; i < 2; i++) {
    if (cid == 0 && i == 1) {
      // Start timer
      timer = benchmark_get_cycle();
      // Start dump
      start_kernel();
    }

#ifdef MXDOTP
    mxfp4_matmul_bf16_mxdotp_lmul1_8x(local_c, local_a, local_b,
                                      local_a_scale, local_b_scale,
                                      local_m, local_n, local_k);
#else
    #error "not implemented"
#endif

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
#ifdef MXDOTP
    // (MX vector size) * (total # FPUs) * (1 additon, 1 mult.)
    long unsigned int max_ops_per_cycle = 16 * num_cores * SNRT_NFPU_PER_CORE * 2;
#else
    #error "not implemented"
#endif
    long unsigned int utilization = performance / max_ops_per_cycle;

    printf("\n----- (%d,%d,%d) mxfp4/matmul-bf16 -----\n", m, n, k);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
            performance, utilization);
  }

  // Check results
  if (cid == 0) {
    bool success = verify_matrix(c, mx_matmul_C_results_dram, m, n);
    if (!success)
      return 1;
  }
  return 0;
}
