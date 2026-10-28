// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Complex GeMM C = A*B (A: MxN, B: NxK) with FP16 complex inputs and FP32 complex output, as ONE
// O-POPE widening GeMM. A complex FP16 element is a 32-bit word {re (lo), im (hi)}, i.e. already a pair
// along the contraction dimension N, and the SDOTP computes lo*lo + hi*hi, which is a complex
// multiply-accumulate when the W word is chosen per output part:
//   Cr += Ar*Br + Ai*(-Bi)  = dot({Ar,Ai}, {Br, -Bi})
//   Ci += Ar*Bi + Ai*Br     = dot({Ar,Ai}, {Bi,  Br})
// X^T is A^T as stored (no processing at all); the W operand is [N][2K] words whose columns alternate
// {Br,-Bi} (even, real part) and {Bi,Br} (odd, imaginary part), so the O-POPE output [M][2K] is directly
// the interleaved complex C. The W operand is supplied by the header (the sign flip of the imaginary half
// and the half-word swap of B are done offline: no processing on the device). The accumulation over
// N complex terms is a single job of 2N real terms (n_size = 2N FP16 elements).

#include "opope_app.h"

#include DATAHEADER

uint32_t *at;  // A^T [N][M] words
uint32_t *wp;  // W operand [N][2K] words
float *c;      // C [M][2K] FP32 = [M][K] complex interleaved

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(at, uint32_t, N_SIZE * M_SIZE);
    OPOPE_L1_ALLOC(wp, uint32_t, N_SIZE * 2 * K_SIZE);
    OPOPE_L1_ALLOC(c, float, M_SIZE * 2 * K_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices (complex FP16 data)
  if (cid == 0) {
    snrt_dma_start_1d(at, cm_at_dram, N_SIZE * M_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(wp, cm_w_dram, N_SIZE * 2 * K_SIZE * sizeof(uint32_t));  // W' = {Br,-Bi},{Bi,Br} (layout prepared offline)
    opope_dma_zero(c, M_SIZE * 2 * K_SIZE * sizeof(float));                      // Y = 0
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    
    // One complex GeMM = one widening GeMM of M x 2N x 2K (O-POPE)
    opope_gemm_fp16(at, wp, c, M_SIZE, 2 * N_SIZE, 2 * K_SIZE);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    // Each complex MAC is 4 real MACs
    long unsigned int performance = 1000 * 8UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- (%dx%dx%d) widening O-POPE complex matmul -----\n", M_SIZE, N_SIZE, K_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle.\n", performance);

    errors = opope_check_fp32(c, (const float *)cm_golden, 2 * M_SIZE * K_SIZE, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
