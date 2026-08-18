// Copyright 2026 ETH Zurich and University of Bologna.
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

// Author: Bowen Wang, ETH Zurich

// Database dictionary decompression benchmark (vectorized analytics:
// Polychroniou SIGMOD'15 gathers; Lemire's dictionary decoding): a column
// of N_CODES codes indexes a dictionary of K fixed-width entries
// (D x fp32 records, 16 B at D=4 up to 128 B at D=32); decoding
// materializes the values column. Zero FLOPs - throughput (bytes out per
// cycle) is the metric.

#include <benchmark.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/sp-dictdecode.c"

// Kernel variant: 0 = plain-RVV element-gather baseline, 1 = VLXBLK
// indexed block loads, 2 = plain unit-stride vle loop (the large-block
// alternative). DICTDECODE_USE_VLXBLK is honored for backward
// compatibility with the original two-variant build definitions.
#ifndef DICT_VARIANT
#ifdef DICTDECODE_USE_VLXBLK
#define DICT_VARIANT DICTDECODE_USE_VLXBLK
#else
#define DICT_VARIANT 1
#endif
#endif

#if DICT_VARIANT == 1
#define DICTDECODE_NAME "sp dictdecode vlxblk"
#define DICTDECODE_KERNEL dictdecode_vlxblk
#elif DICT_VARIANT == 2
#define DICTDECODE_NAME "sp dictdecode vle"
#define DICTDECODE_KERNEL dictdecode_vle
#else
#define DICTDECODE_NAME "sp dictdecode rvv"
#define DICTDECODE_KERNEL dictdecode_rvv
#endif

// dictdecode_rvv builds 16-bit byte offsets (code * D * 4).
#if DICT_VARIANT == 0 && (DICT_K * DICT_D * 4) > 65536
#error "dictdecode_rvv uses 16-bit byte offsets; requires K*D*4 <= 64 KiB"
#endif

// The fully materialized output column (N_CODES * D * 4 B) does not fit in
// the 128 KiB TCDM next to dict and codes. The timed region therefore
// decodes into a rotating 32 KiB L1 output tile (2048 codes at D=4, 256 at
// D=32; identical L1 store traffic to a full materialization);
// verification re-decodes tile by tile and exact-compares against the
// DMA'd golden slice, so every output element is checked bit-exactly.
#define DICT_TILE_CODES (32768 / (DICT_D * 4))

float *dict;
dict_code_t *codes;
float *out_tile;
uint32_t *golden_tile;

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int n_codes = dict_l.N_CODES;
  const unsigned int tile_codes =
      (n_codes < DICT_TILE_CODES) ? n_codes : DICT_TILE_CODES;
  const unsigned int n_tiles = n_codes / tile_codes;
  const unsigned int codes_per_core = tile_codes / num_cores;

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  if ((n_codes % tile_codes) || (tile_codes % num_cores))
    return -2;

  // Allocate the buffers
  if (cid == 0) {
    // VLXBLK requires the dictionary base to be aligned to the block size
    // (D*4 B, up to 128 B at D=32). snrt_l1alloc returns 256 B-aligned
    // chunks already; stay defensive and over-allocate + align manually
    // to 128 B.
    void *dict_raw = snrt_l1alloc(dict_l.K * DICT_D * sizeof(float) + 128);
    dict = (float *)(((uintptr_t)dict_raw + 127) & ~(uintptr_t)127);
    codes = (dict_code_t *)snrt_l1alloc(n_codes * sizeof(dict_code_t));
    out_tile = (float *)snrt_l1alloc(tile_codes * DICT_D * sizeof(float));
    golden_tile =
        (uint32_t *)snrt_l1alloc(tile_codes * DICT_D * sizeof(uint32_t));
    if (!dict_raw || !codes || !out_tile || !golden_tile)
      return -3;
  }

  snrt_cluster_hw_barrier();

  // Initialize the L1 buffers
  if (cid == 0) {
    snrt_dma_start_1d(dict, dict_vals_dram, dict_l.K * DICT_D * sizeof(float));
    snrt_dma_start_1d(codes, dict_codes_dram, n_codes * sizeof(dict_code_t));
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();

  // Each core decodes its own slice of every tile (disjoint output ranges,
  // no barriers needed inside the timed region).
  for (unsigned int t = 0; t < n_tiles; ++t) {
    const dict_code_t *codes_slice =
        codes + t * tile_codes + cid * codes_per_core;
    float *out_slice = out_tile + cid * codes_per_core * DICT_D;
    DICTDECODE_KERNEL(out_slice, dict, codes_slice, codes_per_core);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;

  // Display results: zero FLOPs, report throughput instead of utilization.
  if (cid == 0) {
    const long bytes_out = (long)n_codes * (DICT_D * sizeof(float));
    printf("\n----- (N=%u, K=%u, D=%u) %s -----\n", n_codes, dict_l.K, DICT_D,
           DICTDECODE_NAME);
    printf("The execution took %u cycles.\n", timer);
    printf("perf: %ld bytes/1000cycles (%ld%% of 32 B/cycle mem peak)\n",
           1000L * bytes_out / timer, 100L * bytes_out / (32L * timer));
    printf("codes: %ld codes/1000cycles\n", 1000L * (long)n_codes / timer);
  }

  // Verify (exact bit match, tile by tile, on core 0)
  int errors = 0;
  if (cid == 0) {
    for (unsigned int t = 0; t < n_tiles; ++t) {
      snrt_dma_start_1d(golden_tile,
                        dict_golden + (size_t)t * tile_codes * DICT_D,
                        tile_codes * DICT_D * sizeof(uint32_t));
      DICTDECODE_KERNEL(out_tile, dict, codes + t * tile_codes, tile_codes);
      snrt_dma_wait_all();

      const uint32_t *out_bits = (const uint32_t *)out_tile;
      for (unsigned int i = 0; i < tile_codes * DICT_D; ++i) {
        if (out_bits[i] != golden_tile[i]) {
          if (errors < 8)
            printf("Mismatch tile %u elem %u: GOT %x EXP %x\n", t, i,
                   out_bits[i], golden_tile[i]);
          errors++;
        }
      }
    }
    if (errors)
      printf("WRONG! (%d mismatches)\n", errors);
    else
      printf("CORRECT!\n");
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return errors ? -1 : 0;
}
