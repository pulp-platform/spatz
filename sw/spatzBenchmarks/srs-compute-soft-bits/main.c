// Copyright 2022 ETH Zurich and University of Bologna.
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

// Author: Ihor Sachok <ihor.sachok@unibo.it>

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include DATAHEADER
#include "kernel/compute_soft_bits.c"

#define LLR_MAX     120
#define LLR_INFTY   127

static bool llr_isinf(int8_t v)
{
  return (v == LLR_INFTY) || (v == -LLR_INFTY);
}

static int8_t tackle_special_sums_ref(int8_t val_a, int8_t val_b, int *found)
{
  *found = 1;

  if (val_a == -val_b) {
    return 0;
  }
  if (llr_isinf(val_a)) {
    return val_a;
  }
  if (llr_isinf(val_b)) {
    return val_b;
  }

  *found = 0;
  return 0;
}

static int8_t promotion_sum_ref(int8_t a, int8_t b)
{
  int     found;
  int8_t  special = tackle_special_sums_ref(a, b, &found);
  if (found) {
    return special;
  }

  int tmp = (int)a + (int)b;
  if (abs(tmp) > LLR_MAX) {
    return (tmp > 0) ? (int8_t)LLR_INFTY : (int8_t)(-LLR_INFTY);
  }
  return (int8_t)tmp;
}

int compute_soft_bits_check(const int8_t *soft_bits,
                            const int8_t *var_to_check,
                            const int8_t *check_to_var,
                            const uint32_t offset,
                            const uint32_t lifting_size)
{
  int errors = 0;

  for (unsigned int i = 0; i < lifting_size; ++i) {
    int8_t a        = var_to_check[offset + i];
    int8_t b        = check_to_var[offset + i];
    int8_t expected = promotion_sum_ref(a, b);
    int8_t actual   = soft_bits[offset + i];

    if (expected != actual) {
      printf("Mismatch at [%u]: a=%d b=%d -> got %d, expected %d\n",
             i, a, b, actual, expected);
      ++errors;
    }
  }

  if (errors == 0) {
    printf("compute_soft_bits: OK (%u elements)\n", lifting_size);
  } else {
    printf("compute_soft_bits: %d errors out of %u\n", errors, lifting_size);
  }
  return errors;
}

#include <stdint.h>
#include <snrt.h>
#include <stdio.h>

int main() {
  int8_t *this_soft_bits;
  int8_t *this_var_to_check;
  int8_t *this_check_to_var;
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  //Allocate arrays
  if (cid == 0) {
    this_soft_bits = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t));
    this_var_to_check = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t));
    this_check_to_var = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t)); 
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(this_var_to_check, this_var_to_check_dram, num_cores * lifting_size * sizeof(int8_t));
    snrt_dma_start_1d(this_check_to_var, this_check_to_var_dram, num_cores * lifting_size * sizeof(int8_t));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();
  //One compute per core.
  //snrt_cluster_hw_barrier();
  if (cid == 0)
  compute_soft_bits(this_soft_bits + cid * lifting_size, this_var_to_check + cid * lifting_size, this_check_to_var + cid * lifting_size, lifting_size);
  // Wait for all cores to finish
  //snrt_cluster_hw_barrier();
  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;
  // End dump
  if (cid == 0)
    stop_kernel();
  // Wait for all cores to finish
  //snrt_cluster_hw_barrier();
  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 9 * lifting_size / timer;
    long unsigned int utilization =
        performance / (8 * 4 * 1);
        // each kernel computes 2 * 2 * lifting_size elements
        // data 8bit how many can IPU process? - 8
        // (4 IPU per Spatz core)
        // 9 operations per element

    printf("\n----- (%d) compute_soft_bits -----\n", lifting_size);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    compute_soft_bits_check(this_soft_bits, this_var_to_check, this_check_to_var,
                          0, lifting_size);
  }
  //if (cid == 1) {
  //  compute_soft_bits_check(this_soft_bits, this_var_to_check, this_check_to_var,
  //                        lifting_size, lifting_size);
  //}
  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}
