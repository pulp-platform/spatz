// Copyright 2023 ETH Zurich and University of Bologna.
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
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/fconv2d.c"

// Threshold for FP comparisons
#define THRESHOLD 0.000000001

// Macro to check similarity between two fp-values, wrt a threshold
#define fp_check(a, b, threshold)                                              \
  ((((a) < (b)) ? (b) - (a) : (a) - (b)) < (threshold))

// Matrices
double *imtx;
double *omtx;
double *fmtx;

// Initialize the matrices
void init_matrix(double *matrix, const double *src, const unsigned int len) {
  for (unsigned int i = 0; i < len; ++i) {
    matrix[i] = src[i];
  }
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Set matrix dimension
  const unsigned int r = fconv2d_l.R;
  const unsigned int c = fconv2d_l.C;
  const unsigned int f = fconv2d_l.F;

  unsigned int timer_start, timer_end, timer;

  // Allocate the matrices in the local tile
  if (cid == 0) {
    imtx = (double *)snrt_l1alloc((r + f - 1) * (c + f - 1) * sizeof(double));
    omtx = (double *)snrt_l1alloc(r * c * sizeof(double));
    fmtx = (double *)snrt_l1alloc(f * f * sizeof(double));
  }

  // Reset timer
  timer = (unsigned int)-1;

  // We support only square matrices for now
  if (r != c)
    return -9;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  double *i = imtx + (r + f - 1) * (r / num_cores) * cid;
  double *o = omtx + r * (r / num_cores) * cid;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices
  if (cid == 0) {
    snrt_dma_start_1d(imtx, fconv2d_I_dram,
                      (r + f - 1) * (c + f - 1) * sizeof(double));
    snrt_dma_start_1d(omtx, fconv2d_R_dram, r * c * sizeof(double));
    snrt_dma_start_1d(fmtx, fconv2d_F_dram, f * f * sizeof(double));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  //
  // Calculate fconv2d
  //

  // Start timer
  timer_start = benchmark_get_cycle();

  // Start dump
  if (cid == 0)
    start_kernel();

  // Calculate the result
  conv3d_CHx7x7(o, i, fmtx, r / num_cores, r, c, f);

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer
  if (cid == 0) {
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;
  }

  // End dump
  if (cid == 0)
    stop_kernel();

  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * f * f * r * c / timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    PRINTF("\n----- (%dx%d) dp fconv2d -----\n", r, c);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %lu OP/1000cycle (%lu%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0)
    for (unsigned int k = 0; k < r * c; ++k) {
      if (!fp_check(fconv2d_GR_dram[k], omtx[k], THRESHOLD)) {
        PRINTF("Error index %d: result = %x (@ %x), golden = %x\n", k,
               *((unsigned int *)&omtx[k]), (unsigned int)(omtx + k),
               *((unsigned int *)&fconv2d_GR_dram[k]));
        return -1;
      }
    }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return 0;
}
