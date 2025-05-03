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
#include "kernel/mxfp8-ww-synthetic.c"

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer;

  unsigned int n = LENGTH;

  snrt_cluster_hw_barrier();

  // tiling
  uint32_t    local_n       = n / num_cores;

  snrt_cluster_hw_barrier();

  for (int i = 0; i < 2; i++) {
    if (cid == 0 && i == 1) {
      // Start timer
      timer = benchmark_get_cycle();
      // Start dump
      start_kernel();
    }

    volatile float res = mxfp8_mxdotp_ww_synthetic(local_n);

    snrt_cluster_hw_barrier();

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

    printf("\n----- (%d) mxfp8 ww synthetic -----\n", n);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  return 0;
}
