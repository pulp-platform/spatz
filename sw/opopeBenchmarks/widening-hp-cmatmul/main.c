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
// the interleaved complex C. Spatz builds the W operand from B: sign flip of the imaginary half
// (xor) and half-word swap (shifts + or), written with two stride-8 B stores. The accumulation over
// N complex terms is a single job of 2N real terms (n_size = 2N FP16 elements).

#include "opope_app.h"

#include DATAHEADER

uint32_t *at;  // A^T [N][M] words
uint32_t *bw;  // B [N][K] words
uint32_t *wp;  // W operand [N][2K] words
float *c;      // C [M][2K] FP32 = [M][K] complex interleaved

// wp[2i] = b[i] ^ 0x80000000 = {Br, -Bi},  wp[2i+1] = swap halves(b[i]) = {Bi, Br}
static void build_w_operand(void) {
  unsigned int n = N_SIZE * K_SIZE, vl;
  const uint32_t *src = bw;
  uint32_t *dst = wp;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vle32.v v0, (%0)" ::"r"(src) : "memory");
    asm volatile("vxor.vx v8, v0, %0" ::"r"(0x80000000u));
    asm volatile("vsll.vi v16, v0, 16");
    asm volatile("vsrl.vi v24, v0, 16");
    asm volatile("vor.vv v16, v16, v24");
    asm volatile("vsse32.v v8, (%0), %1" ::"r"(dst), "r"(8) : "memory");
    opope_vsync(wp);  // one strided access in flight at a time
    asm volatile("vsse32.v v16, (%0), %1" ::"r"(dst + 1), "r"(8) : "memory");
    opope_vsync(wp);
    src += vl;
    dst += 2 * vl;
    n -= vl;
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(at, uint32_t, N_SIZE * M_SIZE);
    OPOPE_L1_ALLOC(bw, uint32_t, N_SIZE * K_SIZE);
    OPOPE_L1_ALLOC(wp, uint32_t, N_SIZE * 2 * K_SIZE);
    OPOPE_L1_ALLOC(c, float, M_SIZE * 2 * K_SIZE);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices (complex FP16 data)
  if (cid == 0) {
    snrt_dma_start_1d(at, cm_at_dram, N_SIZE * M_SIZE * sizeof(uint32_t));
    snrt_dma_start_1d(bw, cm_b_dram, N_SIZE * K_SIZE * sizeof(uint32_t));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // Pre-processing (Spatz): W operand from B, Y = 0
    build_w_operand();
    opope_vzero(c, M_SIZE * 2 * K_SIZE);

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
