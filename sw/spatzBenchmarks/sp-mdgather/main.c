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

// sp-mdgather: molecular-dynamics cluster-pair gather benchmark
// (GROMACS-style cluster-pair algorithm), fp32, VLXBLK indexed block
// gather vs. plain-RVV vluxei32 baseline. See kernel/sp-mdgather.c for
// the force model and the FLOP-count convention (17 FLOP per
// (i-atom, j-atom) pair).

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/sp-mdgather.c"

#ifndef MDGATHER_USE_VLXBLK
#define MDGATHER_USE_VLXBLK 1
#endif

#if MDGATHER_USE_VLXBLK
#define MDGATHER_NAME "sp mdgather vlxblk"
#else
#define MDGATHER_NAME "sp mdgather rvv"
#endif

float *coords;
uint16_t *plist;
float *forces;

// Golden check with a scaled (relative-ish) fp32 tolerance: forces
// accumulate LIST*4 terms, so allow 1% of the golden magnitude with an
// absolute floor of 1e-2. Reports mismatches like hp-vqmatmul's
// fp16_check does.
static inline int md_check(const float *golden, const float *actual,
                           uint32_t n_atoms) {
  int errors = 0;
  for (uint32_t i = 0; i < n_atoms; ++i) {
    for (uint32_t d = 0; d < 3; ++d) {
      const float exp = golden[i * 3 + d];
      const float got = actual[i * 3 + d];
      float diff = exp - got;
      if (diff < 0)
        diff = -diff;
      float mag = (exp < 0) ? -exp : exp;
      const float tol = 0.01f * mag + 0.01f;
      if (diff > tol) {
        ++errors;
        printf("[%d, %d] EXP - %8x, GOT - %8x \n", i, d,
               *(const uint32_t *)&golden[i * 3 + d],
               *(const uint32_t *)&actual[i * 3 + d]);
      }
    }
  }
  return errors;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  (void)num_cores;

  unsigned int timer_start, timer_end, timer;

  const unsigned int NC = mdgather_l.NC;
  const unsigned int LIST = mdgather_l.LIST;
  const float CUT2 = mdgather_l.CUT2;

#if MDGATHER_USE_VLXBLK
  const unsigned int plist_size = NC * LIST * sizeof(uint16_t);
#else
  const unsigned int plist_size = NC * LIST * 4 * sizeof(uint16_t);
#endif

  // Allocate the data in the local tile. VLXBLK requires the coordinate
  // base to be aligned to the 16-B block size; we align to the full 64-B
  // cluster record. snrt_l1alloc chunks are 256-B aligned, but
  // over-allocate and align manually so the requirement never silently
  // breaks.
  if (cid == 0) {
    coords = (float *)snrt_l1alloc(NC * 16 * sizeof(float) + 64);
    coords = (float *)(((uintptr_t)coords + 63) & ~(uintptr_t)63);
    plist = (uint16_t *)snrt_l1alloc(plist_size);
    forces = (float *)snrt_l1alloc(NC * 4 * 3 * sizeof(float));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize the data
  if (cid == 0) {
    snrt_dma_start_1d(coords, mdgather_coords_dram, NC * 16 * sizeof(float));
#if MDGATHER_USE_VLXBLK
    snrt_dma_start_1d(plist, mdgather_pairlist_dram, plist_size);
#else
    snrt_dma_start_1d(plist, mdgather_pairlist_exp_dram, plist_size);
#endif
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Calculate the cluster-pair forces. Single Spatz core: the quoted
  // fp32 peak of 16 FLOP/cycle (4 FPUs x 2 fp32 lanes x FMA) is per
  // core, so the utilization print below assumes cid 0 does all work.
  timer_start = benchmark_get_cycle();

  // Start dump
  if (cid == 0) {
    start_kernel();

#if MDGATHER_USE_VLXBLK
    mdgather_vlxblk(forces, coords, plist, NC, LIST, CUT2);
#else
    mdgather_rvv(forces, coords, plist, NC, LIST, CUT2);
#endif

    // End dump
    stop_kernel();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  timer_end = benchmark_get_cycle();
  timer = timer_end - timer_start;

  // Check and display results
  int errors = 0;
  if (cid == 0) {
    // 17 FLOP per (i-atom, j-atom) pair; see kernel/sp-mdgather.c.
    const long long total_flops = (long long)NC * 4 * LIST * 4 * 17;

    printf("\n----- (NC=%u, LIST=%u) %s -----\n", NC, LIST, MDGATHER_NAME);
    printf("The execution took %u cycles.\n", timer);
    printf("perf: %ld FLOP/1000cycles (%ld%% of fp32 peak 16/cycle)\n",
           (long)(1000LL * total_flops / timer),
           (long)(100LL * total_flops / (16LL * timer)));

    errors = md_check(mdgather_golden, forces, NC * 4);
    if (errors) {
      printf("WRONG!   \n");
    } else {
      printf("CORRECT! \n");
    }
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return errors;
}
