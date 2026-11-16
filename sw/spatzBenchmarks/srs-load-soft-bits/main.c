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
#include "kernel/load_soft_bits.c"

int8_t *llrs;
int8_t *soft_bits;

uint32_t find_last_nonzero(const int8_t *llrs, const uint32_t nof_llr) {
  uint32_t i = nof_llr;
  while (i > 0 && llrs[i - 1] == 0) {
    i--;
  }
  return i;
}

int load_soft_bits_check(const int8_t *llrs, uint32_t nof_llr,
                         const int8_t *soft,
                         uint32_t lifting_size, uint32_t buffer_nodes) {
  const int8_t LOW  = -64;
  const int8_t HIGH =  64;

  uint32_t total   = buffer_nodes * lifting_size;
  uint32_t two_l   = 2 * lifting_size;
  uint32_t body    = (buffer_nodes - 2) * lifting_size;
  uint32_t to_copy = (nof_llr < body) ? nof_llr : body;

  unsigned errors = 0;

  for (uint32_t i = 0; i < total; ++i) {
    int8_t expected;

    if (i < two_l) {
      expected = 0;
    } else if (i - two_l < to_copy) {
      int8_t x = llrs[i - two_l];
      if (x > HIGH) x = HIGH;
      if (x < LOW)  x = LOW;
      expected = x;
    } else {
      expected = 0;
    }

    if (soft[i] != expected) {
      printf("MISMATCH soft[%u] = %d, expected %d\n", i, soft[i], expected);
      ++errors;
    }
  }

  if (errors == 0) {
    printf("load_soft_bits OK (%u bytes)\n", total);
  } else {
    printf("load_soft_bits FAILED: %u mismatches\n", errors);
  }
  return errors;
}


int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  //Allocate arrays
  if (cid == 0) {
    soft_bits = (int8_t *)snrt_l1alloc(buffer_nodes * lifting_size * sizeof(int8_t));
    llrs = (int8_t *)snrt_l1alloc(total_llr * sizeof(int8_t));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(llrs, load_soft_bits_llrs_dram, total_llr * sizeof(int8_t));
    snrt_dma_wait_all();
  }
  uint32_t nof_llr;
  if (cid ==0)
    nof_llr = find_last_nonzero(llrs, total_llr);
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();
  snrt_cluster_hw_barrier();
  if (cid == 0)
    load_soft_bits(llrs, nof_llr, soft_bits, lifting_size, buffer_nodes);
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 3  * lifting_size / timer;
    long unsigned int utilization =
        performance / (4 * 4  * 1);
        // data 8bit how many can IPU process? - 4
        // (4 IPU per Spatz core)
        // 2 * lifting_size elements, 3 instructions per element.

    printf("\n----- (%d) load_soft_bits -----\n", lifting_size);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
  load_soft_bits_check(llrs, nof_llr, soft_bits, lifting_size, buffer_nodes);
  }
  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}