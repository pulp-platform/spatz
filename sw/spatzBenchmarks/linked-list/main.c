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

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/llist.c"

// #define VECTOR

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  const int measure_iter = 2;

  #if USE_CACHE == 1
  uint32_t spm_size = 16;
  #else
  uint32_t spm_size = 120;
  #endif

  if (cid == 0) {
    // Init the cache
    l1d_init(spm_size);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Reset timer
  unsigned int timer = (unsigned int)-1;
  unsigned int timer_tmp, timer_iter1;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  #ifdef VECTOR
  for (int iter = 0; iter < measure_iter; iter ++) {
    if (cid == 0) {

      start_kernel();

      timer_tmp = benchmark_get_cycle();

      // Traverse through the linked list
      traverseList_vec((node_t *) head, N, N_list);

      stop_kernel();

      timer_tmp = benchmark_get_cycle() - timer_tmp;
      timer = (timer < timer_tmp) ? timer : timer_tmp;
      if (iter == 0)
        timer_iter1 = timer;
    }
    snrt_cluster_hw_barrier();
  }

  #else
  node_t * temp_head = head[cid];

  for (int iter = 0; iter < measure_iter; iter ++) {
    // Start dump
    if (cid == 0)
      start_kernel();

    // Start timer
    if (cid == 0)
      timer_tmp = benchmark_get_cycle();

    // Traverse through the linked list
    for (int i = 0; i < N_list; i = i+2) {
      traverseList(temp_head);
      temp_head = head[cid + i];
    }

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    if (cid == 0) {
      timer_tmp = benchmark_get_cycle() - timer_tmp;
      timer = (timer < timer_tmp) ? timer : timer_tmp;
      if (iter == 0)
        timer_iter1 = timer;
    }

    snrt_cluster_hw_barrier();
  }
  #endif

  // Check and display results
  if (cid == 0) {
    write_cyc(timer_iter1);
  #ifdef PRINT_RESULT
    printf("\n----- llist traserve -----\n");
    printf("The first execution took %u cycles.\n", timer_iter1);
    printf("The best execution took %u cycles.\n", timer);
  #endif
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();
  set_eoc();
  

  return 0;
}
