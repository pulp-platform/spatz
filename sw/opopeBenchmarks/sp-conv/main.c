// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// CNN layer (valid KHxKW convolution + bias + ReLU) as im2col -> O-POPE GeMM -> ReLU.
//   Z[M][K] = Y + X*W,  M = Ho*W output positions (the KW-1 last columns of every image row
//   are computed but unused), N = Cin*KH*KW, K = Cout.
// Layout (no transposes): the im2col matrix is built directly as X^T[N][M]: row n=(c,ky,kx) is
// the input plane c shifted by ky*W+kx, i.e. one contiguous unit-stride copy. Weights are stored
// [N][K] in DRAM, Y is initialised with the broadcast bias, the output is spatial-major.

#include "opope_app.h"

#include DATAHEADER

float *in;
float *xt;
float *wm;
float *z;
float *bias;

// dst[0..n) = src[0..n) (unit stride, any alignment)
static void vcopy(float *dst, const float *src, unsigned int n) {
  unsigned int vl;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vle32.v v0, (%0)" ::"r"(src) : "memory");
    asm volatile("vse32.v v0, (%0)" ::"r"(dst) : "memory");
    src += vl;
    dst += vl;
    n -= vl;
  }
}

// Y[m][0..K) = bias[0..K) for every row
static void bias_broadcast(float *y, const float *b, unsigned int rows, unsigned int k) {
  unsigned int vl;
  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(k));  // k <= VLMAX (128)
  asm volatile("vle32.v v0, (%0)" ::"r"(b) : "memory");
  for (unsigned int m = 0; m < rows; m++) {
    asm volatile("vse32.v v0, (%0)" ::"r"(y + m * k) : "memory");
  }
}

// z[0..n) = max(z, 0)
static void relu(float *zz, unsigned int n) {
  unsigned int vl;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vle32.v v0, (%0)" ::"r"(zz) : "memory");
    asm volatile("vfmax.vf v0, v0, %0" ::"f"(0.0f));
    asm volatile("vse32.v v0, (%0)" ::"r"(zz) : "memory");
    zz += vl;
    n -= vl;
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(in, float, CIN * H_SIZE * W_SIZE + 4);
    OPOPE_L1_ALLOC(xt, float, N_SIZE * M_SIZE);
    OPOPE_L1_ALLOC(wm, float, N_SIZE * K_SIZE);
    OPOPE_L1_ALLOC(z, float, M_SIZE * K_SIZE);
    OPOPE_L1_ALLOC(bias, float, K_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(in, cv_in_dram, CIN * H_SIZE * W_SIZE * sizeof(float));
    snrt_dma_start_1d(wm, cv_w_dram, N_SIZE * K_SIZE * sizeof(float));
    snrt_dma_start_1d(bias, cv_bias_dram, K_SIZE * sizeof(float));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // Pre-processing (Spatz): im2col (transposed) and Y = broadcast bias
    for (unsigned int c = 0; c < CIN; c++)
      for (unsigned int ky = 0; ky < KH_SIZE; ky++)
        for (unsigned int kx = 0; kx < KW_SIZE; kx++) {
          unsigned int n = (c * KH_SIZE + ky) * KW_SIZE + kx;
          vcopy(xt + n * M_SIZE, in + c * H_SIZE * W_SIZE + ky * W_SIZE + kx, M_SIZE);
        }
    bias_broadcast(z, bias, M_SIZE, K_SIZE);

    // GeMM (O-POPE)
    opope_gemm(xt, wm, z, M_SIZE, N_SIZE, K_SIZE);

    // Post-processing (Spatz): ReLU
    relu(z, M_SIZE * K_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- conv %dx%dx%d -> %d (3x3) as GeMM %dx%dx%d -----\n", CIN, H_SIZE, W_SIZE,
           COUT, M_SIZE, N_SIZE, K_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (over the computed GeMM).\n", performance);

    // Check only the valid output positions
    for (unsigned int oy = 0; oy < HO_SIZE; oy++)
      errors += opope_check_fp32(z + oy * W_SIZE * K_SIZE,
                                 (const float *)cv_golden + oy * WO_SIZE * K_SIZE,
                                 WO_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
