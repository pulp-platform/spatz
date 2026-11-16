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
#include "kernel/equalize_zf_2xn.c"

static inline bool close_f(float a, float b) {
  if (a == b) return true;
  return fabsf(a - b) <= 1e-5f * fabsf(b);
}
static inline bool close_c(float gr, float gi, float rr, float ri) {
  if (gr == rr && gi == ri) return true;
  float dr = gr - rr, di = gi - ri;
  return (dr * dr + di * di) <= 1e-10f * (rr * rr + ri * ri);
}
static inline uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static uint32_t equalize_zf_2xn_check(const float *eq_symbols,
                                      const float *noise_vars,
                                      uint32_t nof_re) {
  uint32_t errors = 0;
  for (uint32_t i_re = 0; i_re < nof_re; i_re++) {
    for (uint32_t l = 0; l < 2; l++) {
      uint32_t s = 2 * i_re + l;
      bool bad = !close_c(eq_symbols[2 * s], eq_symbols[2 * s + 1],
                          symbols_ref_dram[2 * s], symbols_ref_dram[2 * s + 1]) ||
                 !close_f(noise_vars[s], nvars_ref_dram[s]);
      if (bad) {
        errors++;
        printf("[re %u L%u] re %08x/%08x im %08x/%08x nv %08x/%08x (got/ref)\n",
               i_re, l,
               f2u(eq_symbols[2 * s]),     f2u(symbols_ref_dram[2 * s]),
               f2u(eq_symbols[2 * s + 1]), f2u(symbols_ref_dram[2 * s + 1]),
               f2u(noise_vars[s]),         f2u(nvars_ref_dram[s]));
      }
    }
  }
  return errors;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  float *nvars_out, *eq_symbols;
  float noise_var_est = noise_var_est_dram;
  uint16_t *ch_symbols, *ch_estimates;
  float tx_scaling = tx_scaling_dram;
  uint32_t nof_re = nof_re_dram;

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  //Allocate arrays
  if (cid == 0) {
    nvars_out = (float *)snrt_l1alloc(2 * nof_re * sizeof(float));
    eq_symbols = (float *)snrt_l1alloc(4 * nof_re * sizeof(float)); 
    ch_symbols = (uint16_t *)snrt_l1alloc(RX_PORTS * 2 * nof_re * sizeof(uint16_t));
    ch_estimates = (uint16_t *)snrt_l1alloc(RX_PORTS * 2 * 2 * nof_re * sizeof(uint16_t));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(ch_symbols, ch_symbols_dram, RX_PORTS * 2 * nof_re * sizeof(uint16_t));
    snrt_dma_start_1d(ch_estimates, ch_estimates_dram, RX_PORTS * 2 * 2 * nof_re * sizeof(uint16_t));
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
    equalize_zf_2xn(eq_symbols, nvars_out, ch_symbols, ch_estimates, noise_var_est, tx_scaling, nof_re, RX_PORTS);
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
    long unsigned int performance = 1000 * (16 * RX_PORTS + 28 )* nof_re / timer;
    long unsigned int utilization =
        performance / (8 * 2 * 1);
        // data 32 bit -> each ipu computes 2 32bit elements
        // There are 8 fpu and 8ipu

    printf("\n----- equalize_zf_2xn (%u RE) -----\n", nof_re);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    unsigned errs = equalize_zf_2xn_check(eq_symbols, nvars_out, nof_re);
    printf("mismatches: %u\n", errs);
  }
  snrt_cluster_hw_barrier();

  return 0;
}