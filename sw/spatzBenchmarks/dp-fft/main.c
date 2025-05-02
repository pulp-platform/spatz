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


double *samples;
double *buffer;
double *twiddle;

uint16_t *store_idx;
uint16_t *bitrev;

// NaN detection
int is_nan_double(double x) {
    union {
        double d;
        uint64_t u;
    } value;

    value.d = x;

    uint64_t exponent = (value.u >> 52) & 0x7FF;
    uint64_t mantissa = value.u & 0xFFFFFFFFFFFFF; // lower 52 bits

    // #ifdef PRINT_RESULT
    // if ((exponent == 0x7FF) && (mantissa != 0)) {
    //   printf("Calculation result is NaN!\n");
    // } else {
    //   printf("Calculation result is not NaN!\n");
    // }
    // #endif

    return (exponent == 0x7FF) && (mantissa != 0);
}

static inline int fp_check(const double a, const double b) {
  const double threshold = 0.00001;

  // // Check nan
  // if (is_nan_double(a)){
  //   return 1;
  // }

  // Absolute value
  double comp = a - b;
  if (comp < 0)
    comp = -comp;

  return comp > threshold;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  #if MEAS_1ITER == 1
  const int measure_iter = 1;
  #else
  const int measure_iter = 2;
  #endif

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

  // log2(nfft).
  const unsigned int log2_nfft = 31 - __builtin_clz(NFFT >> 1);

  // Reset timer
  unsigned int timer = (unsigned int)-1;
  unsigned int timer_tmp, timer_iter1;

  #if USE_CACHE == 0
  // Allocate the matrices
  if (cid == 0) {
    samples = (double *)snrt_l1alloc(2 * NFFT * sizeof(double));
    buffer = (double *)snrt_l1alloc(2 * NFFT * sizeof(double));
    twiddle = (double *)snrt_l1alloc((2 * NTWI + NFFT) * sizeof(double));
    store_idx =
        (uint16_t *)snrt_l1alloc(log2_nfft * (NFFT / 4) * sizeof(uint16_t));
    bitrev = (uint16_t *)snrt_l1alloc((NFFT / 4) * sizeof(uint16_t));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(samples, samples_dram, 2 * NFFT * sizeof(double));
    snrt_dma_start_1d(buffer, buffer_dram, 2 * NFFT * sizeof(double));
    snrt_dma_start_1d(twiddle, twiddle_dram,
                      (2 * NTWI + NFFT) * sizeof(double));
    snrt_dma_start_1d(store_idx, store_idx_dram,
                      log2_nfft * (NFFT / 4) * sizeof(uint16_t));
    snrt_dma_start_1d(bitrev, bitrev_dram, (NFFT / 4) * sizeof(uint16_t));
    snrt_dma_wait_all();
  }
  #else
  samples   = samples_dram;
  buffer    = buffer_dram;
  twiddle   = twiddle_dram;
  store_idx = store_idx_dram;
  bitrev    = bitrev_dram;
  #endif

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate pointers for the second butterfly onwards
  double *s_ = samples + cid * (NFFT >> 1);
  double *buf_ = buffer + cid * (NFFT >> 1);
  double *twi_ = twiddle + NFFT;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  for (int iter = 0; iter < measure_iter; iter ++) {
    // Start timer
    timer_tmp = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    // First stage
    fft_2c(samples, buffer, twiddle, NFFT, cid);

    // Wait for all cores to finish the first stage
    snrt_cluster_hw_barrier();

    // Fall back into the single-core case
    fft_sc(s_, buf_, twi_, store_idx, bitrev, NFFT >> 1, log2_nfft, cid);

    // Wait for all cores to finish fft
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
      
      #ifdef PRINT_RESULT
      printf("Iteration %u: %u cycles\n", iter, timer_tmp);
      #endif
    }

    if (cid == 0){
      if (iter == 0) {
        // Verify the real part
        for (unsigned int i = 0; i < NFFT; i++) {
          if (fp_check(buffer[i], gold_out_dram[2 * i])) {
            #ifdef PRINT_RESULT
            printf("Error: Index %d -> Result = %f, Expected = %f\n", i,
                   (float)buffer[i], (float)gold_out_dram[2 * i]);
            #endif
            return 1;
          }
        }

        // Verify the imac part
        for (unsigned int i = 0; i < NFFT; i++) {
          if (fp_check(buffer[i + NFFT], gold_out_dram[2 * i + 1])) {
            #ifdef PRINT_RESULT
            printf("Error: Index %d -> Result = %f, Expected = %f\n", i + NFFT,
                   (float)buffer[i + NFFT], (float)gold_out_dram[2 * i + 1]);
            #endif
            return 1;
          }
        }
      }
    }

    snrt_cluster_hw_barrier();
  }

  // Display runtime
  if (cid == 0) {
    write_cyc(timer);
    long unsigned int performance =
        1000 * 5 * NFFT * (log2_nfft+1) / timer;
    long unsigned int utilization = (1000 * performance) / (1250 * num_cores * 4);
    #ifdef PRINT_RESULT
    printf("\n----- fft on %d samples -----\n", NFFT);
    printf("The first execution took %u cycles.\n", timer_iter1);
    printf("The best execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
    #endif
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();
  set_eoc();

  return 0;
}
