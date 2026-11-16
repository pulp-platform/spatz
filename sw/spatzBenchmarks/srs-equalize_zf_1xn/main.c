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
#include "kernel/equalize_zf_1xn.c"

static inline bool close_f(float a, float b) {
  if (a == b) return true;
  return fabsf(a - b) <= 1e-5f * fabsf(b);
}
static inline uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static uint32_t equalize_zf_1xn_check(const float *symbols_out,
                                      const float *nvars_out,
                                      uint32_t nof_re) {
  uint32_t errors = 0;
  for (uint32_t i = 0; i < nof_re; i++) {
    bool bad = !close_f(symbols_out[2 * i],     symbols_ref_dram[2 * i]) ||
               !close_f(symbols_out[2 * i + 1], symbols_ref_dram[2 * i + 1]) ||
               !close_f(nvars_out[i],           nvars_ref_dram[i]);
    if (bad) {
      errors++;
      printf("[%u] re %08x/%08x im %08x/%08x nv %08x/%08x (got/ref)\n", i,
            f2u(symbols_out[2*i]),   f2u(symbols_ref_dram[2*i]),
            f2u(symbols_out[2*i+1]), f2u(symbols_ref_dram[2*i+1]),
            f2u(nvars_out[i]),       f2u(nvars_ref_dram[i]));
    }
  }
  return errors;
}


int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  float *nvars_out, *symbols_out, *noise_var_est;
  uint16_t *ch_symbols, *ch_estimates;
  float tx_scaling = tx_scaling_dram;
  uint32_t nof_re = nof_re_dram;

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  //Allocate arrays
  if (cid == 0) {
    nvars_out = (float *)snrt_l1alloc(RX_PORTS * nof_re * sizeof(float));
    symbols_out = (float *)snrt_l1alloc(RX_PORTS * 2 * nof_re * sizeof(float)); 
    ch_symbols = (uint16_t *)snrt_l1alloc(RX_PORTS * 2 * nof_re * sizeof(uint16_t));
    ch_estimates = (uint16_t *)snrt_l1alloc(RX_PORTS * 2 * nof_re * sizeof(uint16_t));
    noise_var_est = (float *)snrt_l1alloc(RX_PORTS * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(ch_symbols, ch_symbols_dram, RX_PORTS * 2 * nof_re * sizeof(uint16_t));
    snrt_dma_start_1d(ch_estimates, ch_estimates_dram, RX_PORTS * 2 * nof_re * sizeof(uint16_t));
    snrt_dma_start_1d(noise_var_est, noise_var_est_dram, RX_PORTS * sizeof(float));
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
    equalize_zf_1xn(symbols_out, nvars_out, ch_symbols, ch_estimates, noise_var_est, tx_scaling, nof_re, RX_PORTS);
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
    long unsigned int performance = 1000 * (10 * RX_PORTS + 10 )* nof_re / timer;
    long unsigned int utilization =
        performance / (8 * 2 * 1);
        // data 32 bit -> each ipu computes 2 32bit elements
        // There are 8 fpu and 8ipu

    printf("\n----- equalize_zf_1xn (%u RE) -----\n", nof_re);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    unsigned errs = equalize_zf_1xn_check(symbols_out, nvars_out, nof_re);
    printf("mismatches: %u\n", errs);
  }
  snrt_cluster_hw_barrier();

  return 0;
}