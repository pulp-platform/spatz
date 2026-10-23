// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// CNN layer (valid KHxKW convolution + bias + ReLU) as im2col -> O-POPE widening GeMM -> ReLU,
// FP16 activations/weights, FP32 accumulation/output.
//   Z[M][K] = Y + X*W,  M = Ho*W output positions (the KW-1 last columns of every image row
//   are computed but unused), N = Cin*KH*KW, K = Cout.
// The input is stored as channel-pair planes (two FP16 channels per word), so every packed row of the
// X^T operand, xT[p][0..M) with p = (cp*KH + ky)*KW + kx, is ONE unit-stride word copy of the
// plane pair shifted by ky*W + kx: no per-element packing is needed on the device. The packed
// weights [N/2][K] are prepared offline, Y is initialised with the broadcast FP32 bias.

#include "opope_transformer.h"

#include DATAHEADER

uint32_t *in;
uint32_t *xt;
uint32_t *wm;
float *z;
float *bias;

// dst[0..n) = src[0..n) (32-bit words, unit stride)
static void vcopy(uint32_t *dst, const uint32_t *src, unsigned int n) {
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

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(in, uint32_t, CIN / 2 * H_SIZE * W_SIZE + 4);
    OPOPE_L1_ALLOC(xt, uint32_t, (N_SIZE / 2) * M_SIZE);
    OPOPE_L1_ALLOC(wm, uint32_t, (N_SIZE / 2) * K_SIZE);
    OPOPE_L1_ALLOC(z, float, M_SIZE * K_SIZE);
    OPOPE_L1_ALLOC(bias, float, K_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize data
  if (cid == 0) {
    snrt_dma_start_1d(in, cv_in_dram, CIN / 2 * H_SIZE * W_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(wm, cv_w_dram, (N_SIZE / 2) * K_SIZE * sizeof(uint32_t));
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

    // Pre-processing (Spatz): packed im2col (X^T) and Y = broadcast bias
    for (unsigned int cp = 0; cp < CIN / 2; cp++)
      for (unsigned int ky = 0; ky < KH_SIZE; ky++)
        for (unsigned int kx = 0; kx < KW_SIZE; kx++) {
          unsigned int p = (cp * KH_SIZE + ky) * KW_SIZE + kx;
          vcopy(xt + p * M_SIZE, in + cp * H_SIZE * W_SIZE + ky * W_SIZE + kx, M_SIZE);
        }
    tf_fill_cols(z, bias, M_SIZE, K_SIZE);

    // GeMM (O-POPE, FP16 -> FP32)
    opope_gemm_fp16(xt, wm, z, M_SIZE, N_SIZE, K_SIZE);

    // Post-processing (Spatz, FP32): ReLU
    tf_relu(z, M_SIZE * K_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    long unsigned int performance = 1000 * 2UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- widening conv %dx%dx%d -> %d (3x3) as GeMM %dx%dx%d -----\n", CIN, H_SIZE,
           W_SIZE, COUT, M_SIZE, N_SIZE, K_SIZE);
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
