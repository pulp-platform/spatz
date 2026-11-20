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
#include <float.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include DATAHEADER
#include "kernel/demodulate_soft_BPSK.c"
#define M_SQRT2f32 1.41421356237309504880f
#define LLR_MAX 120
//There is no Div in snitch
/*
int8_t quantize(float value, float range_limit)
{
  float clipped = value;
  if (fabsf(value) > range_limit)
    clipped = copysignf(range_limit, value);
  return (int8_t)roundf(clipped / range_limit * LLR_MAX);
}

static uint32_t demodulate_soft_BPSK_check(int8_t *llrs, const float *symbols,
    const float *noise_vars, uint32_t nof_symbols) {
  uint32_t errors = 0;
  for (uint32_t i = 0; i < nof_symbols; i++) {
    int8_t llr;
    if (noise_vars[i] > 0)
      llr = quantize(2.0f * M_SQRT2f32 * (symbols[2*i] + symbols[2*i + 1]) / noise_vars[i],
                     24.0f);
    else
      llr = 0;
    if (llr != llrs[i])
      errors++;
  }
  return errors;
}
*/
static uint32_t demodulate_soft_BPSK_check(int8_t *llrs, uint32_t nof_symbols) {
  uint32_t errors = 0;
  for (uint32_t i = 0; i < nof_symbols; i++) {
    if (llrs_result_dram[i] != llrs[i]){
      errors++;
      printf("[%u] re, llrs_golden = %d, llrs = %d \n", i, llrs_result_dram[i], llrs[i]);
    }
  }
  return errors;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  float *symbols;
  float *noise_vars;
  int8_t *llrs;
  
  uint32_t nof_symbols = nof_symbols_dram;

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  //Allocate arrays
  if (cid == 0) {
    llrs = (int8_t *)snrt_l1alloc(nof_symbols * sizeof(int8_t));
    symbols = (float *)snrt_l1alloc(2 * nof_symbols * sizeof(float)); 
    noise_vars = (float *)snrt_l1alloc(nof_symbols * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(symbols, symbols_dram, 2 * nof_symbols * sizeof(float));
    snrt_dma_start_1d(noise_vars, noise_vars_dram, nof_symbols * sizeof(float));
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
  //snrt_cluster_hw_barrier();
  if (cid == 0)
    demodulate_soft_BPSK(llrs, symbols, noise_vars, nof_symbols);
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
    long unsigned int performance = 1000 * 8 * nof_symbols / timer;
    long unsigned int utilization =
        performance / (8 * 2 * 1);
        // data 32 bit -> each ipu computes 2 32bit elements
        // There are 8 fpu and 8ipu

    printf("\n----- demodulate soft BPSK (Number of symbols: %u) -----\n", nof_symbols);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    unsigned errs = demodulate_soft_BPSK_check(llrs, nof_symbols);
    printf("mismatches: %u\n", errs);
  }
  snrt_cluster_hw_barrier();

  return 0;
}