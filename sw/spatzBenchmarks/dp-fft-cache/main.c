// Copyright 2021 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti, Marco Bertuletti, ETH Zurich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/fft.c"

double *out;

static inline int fp_check(const double a, const double b) {
  const double threshold = 0.00001;

  // Absolute value
  double comp = a - b;
  if (comp < 0)
    comp = -comp;

  return comp > threshold;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  if (cid == 0) {
    // Init the cache
    l1d_init();
  }
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // log2(nfft).
  const unsigned int log2_nfft = 31 - __builtin_clz(NFFT >> 1);

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  // Add a output buffer in SPM for final result store (avoid racing between cores)
  if (cid == 0) {
    out = (double *)snrt_l1alloc(2 * NFFT * sizeof(double));
  }

  if (cid == 0) {
    snrt_dma_start_1d(out, buffer_dram, 2 * NFFT * sizeof(double));
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate pointers for the second butterfly onwards
  double *s_ = samples_dram + cid * (NFFT >> 1);
  double *buf_ = buffer_dram + cid * (NFFT >> 1);
  double *out_ = out + cid * (NFFT >> 1);
  // double *twi_ = twiddle + NFFT;
  double *twi_ = twiddle_dram + NFFT;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start timer
  timer = benchmark_get_cycle();

  // Start dump
  if (cid == 0)
    start_kernel();

  // First stage
  fft_2c(samples_dram, buffer_dram, twiddle_dram, NFFT, cid);

  // Wait for all cores to finish the first stage
  snrt_cluster_hw_barrier();

  // Fall back into the single-core case
  fft_sc(s_, buf_, twi_, store_idx_dram, bitrev_dram, NFFT >> 1, log2_nfft, cid, out_);

  // Wait for all cores to finish fft
  snrt_cluster_hw_barrier();

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;

  if (cid == 0) {
    // Flush the cache
    l1d_flush();
    l1d_wait();
  }
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Display runtime
  if (cid == 0) {
    long unsigned int performance =
        1000 * 5 * NFFT * (log2_nfft+1) / timer;
    long unsigned int utilization = (1000 * performance) / (1250 * num_cores * 4);

    printf("\n----- fft on %d samples -----\n", NFFT);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);

    // Verify the real part
    for (unsigned int i = 0; i < NFFT; i++) {
      if (fp_check(out[i], gold_out_dram[2 * i])) {
        printf("Error: Index %d -> Result = %f, Expected = %f\n", i,
               (float)out[i], (float)gold_out_dram[2 * i]);
      }
    }

    // Verify the imac part
    for (unsigned int i = 0; i < NFFT; i++) {
      if (fp_check(out[i + NFFT], gold_out_dram[2 * i + 1])) {
        printf("Error: Index %d -> Result = %f, Expected = %f\n", i + NFFT,
               (float)out[i + NFFT], (float)gold_out_dram[2 * i + 1]);
      }
    }
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}
