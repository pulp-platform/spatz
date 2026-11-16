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

#include DATAHEADER
#include "kernel/scale_llr.c"

int scale_llr_check(const int8_t *output_bits, const int8_t *input_bits, const uint32_t offset,
                         const uint32_t lifting_size) {
  float scale_factor = 0.8f;
  for (unsigned int i = 0; i < lifting_size; ++i) {
    int8_t input = input_bits[offset + i];
    //int8_t expected_output = (input > 120) ? input : (int8_t)lroundf(input * scale_factor);
    int8_t expected_output = (input > 120) ? input : (input * 205) >> 8;
    int8_t actual_output   = output_bits[offset + i];
    if (expected_output != actual_output) {
      printf("Incorrect output[%u] = %d \n", i, actual_output);
      printf("Correct output[%u] = %d \n", i, expected_output);
    }
  }
  return 0;
}


int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  int8_t *input_bits;
  int8_t *output_bits;

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  //Allocate arrays
  if (cid == 0) {
    input_bits = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t));
    output_bits = (int8_t *)snrt_l1alloc(num_cores * lifting_size * sizeof(int8_t)); 
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(input_bits, input_bits_dram, num_cores * lifting_size * sizeof(int8_t));
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
  //One llr_scaling per core.
  //snrt_cluster_hw_barrier();
  if (cid == 0)
    scale_llr(output_bits + cid * lifting_size, input_bits + cid * lifting_size, lifting_size);
  // Wait for all cores to finish
  //snrt_cluster_hw_barrier();
  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;
  // Wait for all cores to finish
    // End dump
  if (cid == 0)
    stop_kernel();
  snrt_cluster_hw_barrier();
  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 3 * lifting_size / timer;
    long unsigned int utilization =
        performance / (4 * 8* 1);
        // data 8bit how many can IPU process? - 8
        // (4 IPU per Spatz core)

    printf("\n----- (%d) scale_llr -----\n", lifting_size);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    scale_llr_check(output_bits, input_bits, 0, lifting_size);
  }
  //snrt_cluster_hw_barrier();
  //if (cid == 1) {
  // scale_llr_check(output_bits, input_bits, lifting_size, lifting_size);
  //}
  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}