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

// VRF materialization benchmark: the load stage of indexed block decode in
// isolation. Codes index a K-entry dictionary of D x fp32 records; the
// timed region loads every referenced block into the VRF and never stores
// it back - modeling fused consumers (VQ-LLM inference dequantization)
// where decompressed weights feed the FPUs directly from the VRF. The
// combined load+store pipeline remains the sp-dictdecode benchmark
// (dictionary application).
//
// Single-core on purpose: core 0 does all the work, other cores only
// barrier, so the measurement is free of cross-core TCDM bank contention.
// Load-only port peak of one core is 32 B/cycle (all four ports reading).
//
// Correctness: an UNTIMED second stage re-runs the identical load path
// through the sp-dictdecode load+store kernels tile by tile and exact-
// compares against the DMA'd golden slices.

#include <benchmark.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "../sp-dictdecode/kernel/sp-dictdecode.c"
#include "kernel/sp-vrfload.c"

// Kernel variant: 0 = plain-RVV element gather, 1 = VLXBLK, 2 = vle loop.
#ifndef DICT_VARIANT
#define DICT_VARIANT 1
#endif

#if DICT_VARIANT == 4
#define VRFLOAD_NAME "sp vrfload rvv-swp"
#define VRFLOAD_KERNEL vrfload_rvv_swp
#define CHECK_KERNEL dictdecode_rvv
#elif DICT_VARIANT == 3
#define VRFLOAD_NAME "sp vrfload vlxblk-swp"
#define VRFLOAD_KERNEL vrfload_vlxblk_swp
#define CHECK_KERNEL dictdecode_vlxblk
#elif DICT_VARIANT == 1
#define VRFLOAD_NAME "sp vrfload vlxblk"
#define VRFLOAD_KERNEL vrfload_vlxblk
#define CHECK_KERNEL dictdecode_vlxblk
#elif DICT_VARIANT == 2
#define VRFLOAD_NAME "sp vrfload vle"
#define VRFLOAD_KERNEL vrfload_vle
#define CHECK_KERNEL dictdecode_vle
#else
#define VRFLOAD_NAME "sp vrfload rvv"
#define VRFLOAD_KERNEL vrfload_rvv
#define CHECK_KERNEL dictdecode_rvv
#endif

#if DICT_VARIANT == 0 && (DICT_K * DICT_D * 4) > 65536
#error "vrfload_rvv uses 16-bit byte offsets; requires K*D*4 <= 64 KiB"
#endif

// Output tiles are used only by the untimed check stage; sizing matches
// sp-dictdecode (halved at D=64 so dict + tiles fit the 128 KiB TCDM).
#if DICT_D >= 64
#define DICT_TILE_CODES (16384 / (DICT_D * 4))
#else
#define DICT_TILE_CODES (32768 / (DICT_D * 4))
#endif

float *dict;
dict_code_t *codes;
float *out_tile;
uint32_t *golden_tile;

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int n_codes = dict_l.N_CODES;
  const unsigned int tile_codes =
      (n_codes < DICT_TILE_CODES) ? n_codes : DICT_TILE_CODES;
  const unsigned int n_tiles = n_codes / tile_codes;

  // Reset timer
  unsigned int timer = (unsigned int)-1;

  if (n_codes % tile_codes)
    return -2;

  // Allocate the buffers (128 B alignment for the VLXBLK dictionary base)
  if (cid == 0) {
    void *dict_raw = snrt_l1alloc(dict_l.K * DICT_D * sizeof(float) + 128);
    dict = (float *)(((uintptr_t)dict_raw + 127) & ~(uintptr_t)127);
    codes = (dict_code_t *)snrt_l1alloc(n_codes * sizeof(dict_code_t));
    out_tile = (float *)snrt_l1alloc(tile_codes * DICT_D * sizeof(float));
    golden_tile =
        (uint32_t *)snrt_l1alloc(tile_codes * DICT_D * sizeof(uint32_t));
    if (!dict_raw || !codes || !out_tile || !golden_tile)
      return -3;
#if DICT_VARIANT == 2
    {
      size_t scr_els;
      asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(scr_els));
      void *scr_raw = snrt_l1alloc(scr_els * sizeof(float) + 128);
      if (!scr_raw)
        return -3;
      vrfload_vle_scratch =
          (float *)((((uintptr_t)scr_raw) + 127) & ~(uintptr_t)127);
    }
#endif
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

  // Timed region: core 0 loads every referenced block into the VRF.
  // No output buffer, no tiles, no stores.
  if (cid == 0)
    VRFLOAD_KERNEL(dict, codes, n_codes);

  snrt_cluster_hw_barrier();

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;

  // Display results: bytes loaded per cycle vs the 32 B/cycle single-core
  // load peak (four 64-bit ports, all reading).
  if (cid == 0) {
    const long bytes_in = (long)n_codes * (DICT_D * sizeof(float));
    printf("\n----- (N=%u, K=%u, D=%u) %s -----\n", n_codes, dict_l.K, DICT_D,
           VRFLOAD_NAME);
    printf("The execution took %u cycles.\n", timer);
    printf("perf: %ld bytes/1000cycles (%ld%% of 32 B/cycle load peak)\n",
           1000L * bytes_in / timer, 100L * bytes_in / (32L * timer));
    printf("codes: %ld codes/1000cycles\n", 1000L * (long)n_codes / timer);
  }

  // UNTIMED check stage: identical load path, plus store-back, tile by
  // tile, exact bit compare against the golden reference. Compiled out in
  // fast-sweep builds (VRFLOAD_NO_CHECK): those runs are UNVERIFIED and
  // must be reproduced with the checked targets before results are final.
  int errors = 0;
#ifdef VRFLOAD_NO_CHECK
  if (cid == 0)
    printf("CHECK SKIPPED (unverified fast-sweep build)\n");
#else
  if (cid == 0) {
    for (unsigned int t = 0; t < n_tiles; ++t) {
      snrt_dma_start_1d(golden_tile,
                        dict_golden + (size_t)t * tile_codes * DICT_D,
                        tile_codes * DICT_D * sizeof(uint32_t));
      CHECK_KERNEL(out_tile, dict, codes + t * tile_codes, tile_codes);
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
#endif

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return errors ? -1 : 0;
}
