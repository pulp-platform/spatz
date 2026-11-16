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
/*
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
  uint32_t T = tile_dram;
  uint32_t n_tiles = (nof_re + T - 1)/T;

  // Reset timer
  uint32_t t_total   = 0;
  uint32_t t_compute = 0;

  // Set b of each buffer = b-th half of the existing allocation
  #define SYM(b) (ch_symbols   + (b) * RX_PORTS * 2 * T)
  #define EST(b) (ch_estimates + (b) * RX_PORTS * 2 * 2 * T)
  #define EQ(b)  (eq_symbols   + (b) * 4 * T)
  #define NV(b)  (nvars_out    + (b) * 2 * T)

  //Allocate arrays
  if (cid == 0) {
    nvars_out = (float *)snrt_l1alloc(2 * 2 * T * sizeof(float));
    eq_symbols = (float *)snrt_l1alloc(2 * 4 * T * sizeof(float)); 
    ch_symbols = (uint16_t *)snrt_l1alloc(2 * RX_PORTS * 2 * T * sizeof(uint16_t));
    ch_estimates = (uint16_t *)snrt_l1alloc(2 * RX_PORTS * 2 * 2 * T * sizeof(uint16_t));
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Start dump
  if (cid == 0)
    start_kernel();

  if(cid == 0)
    t_total = benchmark_get_cycle();

  if (cid == 0) {
    // Prologue: load tile 0 into set 0
    uint32_t len0 = (nof_re < T) ? nof_re : T;
    snrt_dma_start_2d(SYM(0), ch_symbols_dram,
                len0 * 2 * sizeof(uint16_t), len0 * 2 * sizeof(uint16_t),
                nof_re * 2 * sizeof(uint16_t), RX_PORTS);
    snrt_dma_start_2d(EST(0), ch_estimates_dram,
                len0 * 2 * sizeof(uint16_t), len0 * 2 * sizeof(uint16_t),
                nof_re * 2 * sizeof(uint16_t), 2 * RX_PORTS);
    for (uint32_t i = 0; i < n_tiles; i++){
      // Initialize the matrices
      uint32_t b   = i & 1;
      uint32_t s   = i * T;
      uint32_t len = (nof_re - s < T) ? (nof_re - s) : T;
      snrt_dma_wait_all();
      if (i + 1 < n_tiles) {
        uint32_t sn   = s + T;
        uint32_t lenn = (nof_re - sn < T) ? (nof_re - sn) : T;
        snrt_dma_start_2d(SYM(b ^ 1), ch_symbols_dram + 2 * sn,
                    lenn * 2 * sizeof(uint16_t), lenn * 2 * sizeof(uint16_t),
                    nof_re * 2 * sizeof(uint16_t), RX_PORTS);
        snrt_dma_start_2d(EST(b ^ 1), ch_estimates_dram + 2 * sn,
                    lenn * 2 * sizeof(uint16_t), lenn * 2 * sizeof(uint16_t),
                    nof_re * 2 * sizeof(uint16_t), 2 * RX_PORTS);
      }

      if (i >= 1) {
        snrt_dma_start_1d(eq_symbols_dram + 4 * (s - T), EQ(b ^ 1), T * 4 * sizeof(float));
        snrt_dma_start_1d(nvars_out_dram  + 2 * (s - T), NV(b ^ 1), T * 2 * sizeof(float));
      }
      // Start timer
      uint32_t t0 = benchmark_get_cycle();
      //One llr_scaling per core.
      //snrt_cluster_hw_barrier();
      equalize_zf_2xn(EQ(b), NV(b), SYM(b), EST(b), noise_var_est, tx_scaling, len, RX_PORTS);
      // End timer and check if new best runtime
      t_compute += benchmark_get_cycle() - t0;
    }
    // Epilogue: store the last tile
    snrt_dma_wait_all();
    uint32_t il = n_tiles - 1;
    uint32_t sl = il * T;
    uint32_t ll = nof_re - sl;
    snrt_dma_start_1d(eq_symbols_dram + 4 * sl, EQ(il & 1), ll * 4 * sizeof(float));
    snrt_dma_start_1d(nvars_out_dram  + 2 * sl, NV(il & 1), ll * 2 * sizeof(float));
    snrt_dma_wait_all();

    t_total = benchmark_get_cycle() - t_total;
  }
    // End dump
  if (cid == 0)
    stop_kernel();
  snrt_cluster_hw_barrier();
  // Check and display results
  if (cid == 0) {
    uint32_t performance = (uint32_t)(1000ULL * (16 * RX_PORTS + 28) * nof_re / t_total);
    uint32_t utilization = performance / (8 * 2 * 1);
        // data 32 bit -> each ipu computes 2 32bit elements
        // There are 8 fpu and 8ipu

    printf("\n----- equalize_zf (%u RE) -----\n", nof_re);
    printf("Total: %u cycles, compute: %u cycles, tiles: %u (T=%u)\n",
       t_total, t_compute, n_tiles, T);
    printf("The performance is %u OP/1000cycle (%u%%o utilization).\n",
       performance, utilization);
  }
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    unsigned errs = equalize_zf_2xn_check(eq_symbols_dram, nvars_out_dram, nof_re);
    printf("mismatches: %u\n", errs);
  }
  snrt_cluster_hw_barrier();

  return 0;
}
  */
#include <snrt.h>
#include <printf.h>
#include <stdint.h>
#include <string.h>
 
#define VL 64
 
static const uint32_t a_bits[VL] = {
  0x3f800000, 0x40000000, 0x40400000, 0x40800000, 0x40a00000, 0x40c00000, 0x40e00000, 0x41000000,
  0x41100000, 0x41200000, 0x41300000, 0x41400000, 0x41500000, 0x41600000, 0x41700000, 0x41800000,
  0x41880000, 0x41900000, 0x41980000, 0x41a00000, 0x41a80000, 0x41b00000, 0x41b80000, 0x41c00000,
  0x41c80000, 0x41d00000, 0x41d80000, 0x41e00000, 0x41e80000, 0x41f00000, 0x41f80000, 0x42000000,
  0x42040000, 0x42080000, 0x420c0000, 0x42100000, 0x42140000, 0x42180000, 0x421c0000, 0x42200000,
  0x42240000, 0x42280000, 0x422c0000, 0x42300000, 0x42340000, 0x42380000, 0x423c0000, 0x42400000,
  0x42440000, 0x42480000, 0x424c0000, 0x42500000, 0x42540000, 0x42580000, 0x425c0000, 0x42600000,
  0x42640000, 0x42680000, 0x426c0000, 0x42700000, 0x42740000, 0x42780000, 0x427c0000, 0x42800000};
static const uint32_t b_bits[VL] = {
  0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000,
  0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000,
  0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000,
  0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000,
  0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000,
  0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000,
  0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x7fc00000, 0x7fc00000,
  0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000};
static const uint32_t c_bits[VL] = {
  0x42c80000, 0x42ca0000, 0x42cc0000, 0x42ce0000, 0x42d00000, 0x42d20000, 0x42d40000, 0x42d60000,
  0x42d80000, 0x42da0000, 0x42dc0000, 0x42de0000, 0x42e00000, 0x42e20000, 0x42e40000, 0x42e60000,
  0x42e80000, 0x42ea0000, 0x42ec0000, 0x42ee0000, 0x42f00000, 0x42f20000, 0x42f40000, 0x42f60000,
  0x42f80000, 0x42fa0000, 0x42fc0000, 0x42fe0000, 0x43000000, 0x43010000, 0x43020000, 0x43030000,
  0x43040000, 0x43050000, 0x43060000, 0x43070000, 0x43080000, 0x43090000, 0x430a0000, 0x430b0000,
  0x430c0000, 0x430d0000, 0x430e0000, 0x430f0000, 0x43100000, 0x43110000, 0x43120000, 0x43130000,
  0x43140000, 0x43150000, 0x43160000, 0x43170000, 0x43180000, 0x43190000, 0x431a0000, 0x431b0000,
  0x431c0000, 0x431d0000, 0x431e0000, 0x431f0000, 0x43200000, 0x43210000, 0x43220000, 0x43230000};
static const uint32_t div_exp[VL] = {
  0x3f000000, 0x3f800000, 0x3fc00000, 0x40000000, 0x40200000, 0x40400000, 0x40600000, 0x40800000,
  0x40900000, 0x40a00000, 0x40b00000, 0x40c00000, 0x40d00000, 0x40e00000, 0x40f00000, 0x41000000,
  0x41080000, 0x41100000, 0x41180000, 0x41200000, 0x41280000, 0x41300000, 0x41380000, 0x41400000,
  0x41480000, 0x41500000, 0x41580000, 0x41600000, 0x41680000, 0x41700000, 0x41780000, 0x41800000,
  0x41840000, 0x41880000, 0x418c0000, 0x41900000, 0x41940000, 0x41980000, 0x419c0000, 0x41a00000,
  0x41a40000, 0x41a80000, 0x41ac0000, 0x41b00000, 0x41b40000, 0x41b80000, 0x41bc0000, 0x41c00000,
  0x41c40000, 0x41c80000, 0x41cc0000, 0x41d00000, 0x41d40000, 0x41d80000, 0x7fc00000, 0x7fc00000,
  0x41e40000, 0x41e80000, 0x41ec0000, 0x41f00000, 0x41f40000, 0x41f80000, 0x41fc0000, 0x42000000};
 
int main(void) {
  if (snrt_cluster_core_idx() != 0) return 0;
 
  // Vector accesses must target TCDM
  uint32_t *a  = (uint32_t *)snrt_l1alloc(VL * sizeof(uint32_t));
  uint32_t *b  = (uint32_t *)snrt_l1alloc(VL * sizeof(uint32_t));
  uint32_t *c  = (uint32_t *)snrt_l1alloc(VL * sizeof(uint32_t));
  uint32_t *d  = (uint32_t *)snrt_l1alloc(VL * sizeof(uint32_t));
  uint32_t *rd = (uint32_t *)snrt_l1alloc(VL * sizeof(uint32_t));
  uint32_t *ra = (uint32_t *)snrt_l1alloc(VL * sizeof(uint32_t));
 
  for (int i = 0; i < VL; i++) {
    a[i] = a_bits[i]; b[i] = b_bits[i]; c[i] = c_bits[i]; d[i] = 0;
    rd[i] = 0xdeadbeef; ra[i] = 0xdeadbeef;
  }
 
  uint32_t vl;
  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VL));
  asm volatile("vle32.v v4,  (%0)" :: "r"(a) : "memory");
  asm volatile("vle32.v v8,  (%0)" :: "r"(b) : "memory");
  asm volatile("vle32.v v12, (%0)" :: "r"(c) : "memory");
  asm volatile("vle32.v v16, (%0)" :: "r"(d) : "memory");
 
  asm volatile("vfdiv.vv v20, v4, v8");     // lane 3 takes the special-case path
  asm volatile("vfmul.vv v28, v12, v12");
  asm volatile("vfmul.vv v28, v28, v12");
  asm volatile("vfmul.vv v28, v28, v12");
  asm volatile("vfadd.vv v24, v12, v16");   // independent, issued right after
 
  asm volatile("vse32.v v20, (%0)" :: "r"(rd) : "memory");
  asm volatile("vse32.v v24, (%0)" :: "r"(ra) : "memory");
  asm volatile("fence" ::: "memory");
 
  int errs = 0;
  for (int i = 0; i < VL; i++) {
    if (rd[i] != div_exp[i] || ra[i] != c_bits[i]) {
      errs++;
      printf("[lane %d el %2d] div 0x%08x exp 0x%08x | add 0x%08x exp 0x%08x\n",
             i / 2, i, rd[i], div_exp[i], ra[i], c_bits[i]);
    }
  }
 
  if (errs) printf("FAILED: %d elements\n", errs);
  else      printf("PASSED\n");
  return errs;
}