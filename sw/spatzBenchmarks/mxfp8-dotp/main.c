// Copyright 2025 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include <benchmark.h>
#include <snrt.h>
#include <stdbool.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/mxfp8-dotp.c"

char *a;
char *b;
char *a_scale;
char *b_scale;
float *result;

static inline bool verify_result(const float actual, const float expected) {
  float diff = fabsf(expected - actual);
  float eps = 0.05 * fabsf(expected);

  if (diff > eps) {
    printf("Error:\n");
    printf(" actual   = 0x%08x\n", *(uint32_t*)&actual);
    printf(" expected = 0x%08x\n", *(uint32_t*)&expected);
    return false;
  }

  return true;
}

static inline void copy_repeat_vector(char *dst, const char *src,
                                      const uint32_t length,
                                      const uint32_t repeat) {
  for (uint32_t i = 0; i < length; i++) {
    char value = *src++;
    for (uint32_t j = 0; j < repeat; j++)
      *dst++ = value;
  }
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer;

  unsigned int n = mx_dotp_l.N;
  unsigned int n_block = n / MX_BLOCK_SIZE;

  // Check data consistency
  if (cid == 0) {
    if (n % MX_BLOCK_SIZE != 0) {
      printf("N is not a multiple of MX_BLOCK_SIZE\n");
      return 1;
    }
    if (mx_dotp_l.dtype_elements != FP8) {
      printf("unsupported element dtype\n");
      return 1;
    }
    if (mx_dotp_l.dtype_scales != E8M0) {
      printf("unsupported scale dtype\n");
      return 1;
    }
    if (mx_dotp_l.dtype_results != FP32) {
      printf("unsupported result dtype\n");
      return 1;
    }
  }

  bool mxdotp = true;
  bool skip_check = false;

  unsigned int scale_repeat = 1;
  if (mxdotp)
    scale_repeat = MX_BLOCK_SIZE / 8; // 8 = vector size

  printf("scale_repeat = %d\n", scale_repeat);

  // Allocate the matrices in the local tile
  if (cid == 0) {
    a = (char *)snrt_l1alloc(n);
    b = (char *)snrt_l1alloc(n);
    a_scale = (char *)snrt_l1alloc(scale_repeat * n_block);
    b_scale = (char *)snrt_l1alloc(scale_repeat * n_block);
    result = (float *)snrt_l1alloc(num_cores * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(a, mx_dotp_A_elements_dram, n);
    snrt_dma_start_1d(b, mx_dotp_B_elements_dram, n);
    copy_repeat_vector(a_scale, mx_dotp_A_scales_dram, n_block, scale_repeat);
    copy_repeat_vector(b_scale, mx_dotp_B_scales_dram, n_block, scale_repeat);
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  // tiling
  uint32_t    local_n       = n / num_cores;
  uint32_t    local_n_block = n_block / num_cores;
  const char *local_a       = a       + (cid * local_n);
  const char *local_a_scale = a_scale + (cid * local_n_block * scale_repeat);
  const char *local_b       = b       + (cid * local_n);
  const char *local_b_scale = b_scale + (cid * local_n_block * scale_repeat);

  snrt_cluster_hw_barrier();

  for (int i = 0; i < 2; i++) {
    if (cid == 0 && i == 1) {
      // Start timer
      timer = benchmark_get_cycle();
      // Start dump
      start_kernel();
    }

    float res;

    if (mxdotp) {
      res = mxfp8_dotp_fp32_mxdotp_lmul2(local_a, local_b,
                                         local_a_scale, local_b_scale,
                                         local_n);
    } else {
      res = mxfp8_dotp_fp32(local_a, local_b,
                            local_a_scale, local_b_scale,
                            local_n);
    }
    result[i] = res;

    snrt_cluster_hw_barrier();

    // final reduction
    if (cid == 0) {
      for (unsigned int i = 1; i < num_cores; i++)
        res += result[i];
    }
    result[0] = res;

    if (cid == 0 && i == 1) {
      // End dump
      stop_kernel();
      // End timer
      timer = benchmark_get_cycle() - timer;
    }

    snrt_cluster_hw_barrier();
  }

  // Performance summary
  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * n / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);
    // TODO: Compute correct utilization figures

    printf("\n----- (%d) mxfp8 dotp -----\n", n);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  // Check result
  if (cid == 0 && !skip_check) {
    bool success = verify_result(result[0], mx_dotp_C_result_dram);
    if (!success)
      return 1;
  }
  return 0;
}
