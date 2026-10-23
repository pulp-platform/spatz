// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Complex GeMM C = A*B (A: MxN, B: NxK, interleaved Re/Im) with 4 real O-POPE GeMMs.
// Data layout: A is provided in DRAM TRANSPOSED (A^T, N x M, row-major, interleaved), so that
// de-interleaving it yields the transposed real planes that O-POPE needs for X without any
// transpose; B is row-major (N x K), C is row-major (M x K), all interleaved.
//   Cr = Ar*Br + Ai*(-Bi),   Ci = Ar*Bi + Ai*Br
// Spatz de-interleaves the operands and interleaves the result. Real accumulation is done by O-POPE through Y.

#include "opope_app.h"

#include DATAHEADER

#define NA (M_SIZE * N_SIZE)
#define NB (N_SIZE * K_SIZE)
#define NC (M_SIZE * K_SIZE)

// L1 regions. `raw` holds the interleaved A and B at the start and is reused for Cr/Ci once the
// operands are de-interleaved; `planes` holds the real operands and is reused for the interleaved
// result at the end.
#define RAW_FLOATS OPOPE_MAX(2 * NA + 2 * NB, 2 * NC)
#define PLANES_FLOATS OPOPE_MAX(2 * NA + 3 * NB, 2 * NC)

float *raw;
float *planes;

// NOTE: opope_vsync() is needed after every strided access (see opope_app.h)

// De-interleave src[0..2n) into re/im planes
static void deinterleave(float *dst_re, float *dst_im, const float *src, unsigned int n) {
  unsigned int vl;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vlse32.v v0, (%0), %1" ::"r"(src), "r"(8) : "memory");
    asm volatile("vse32.v v0, (%0)" ::"r"(dst_re) : "memory");
    opope_vsync(raw);
    asm volatile("vlse32.v v8, (%0), %1" ::"r"(src + 1), "r"(8) : "memory");
    asm volatile("vse32.v v8, (%0)" ::"r"(dst_im) : "memory");
    opope_vsync(raw);
    src += 2 * vl;
    dst_re += vl;
    dst_im += vl;
    n -= vl;
  }
}

// De-interleave src[0..2n) into re/im planes, plus dst_nim = -im
static void deinterleave_neg(float *dst_re, float *dst_im, float *dst_nim, const float *src,
                             unsigned int n) {
  unsigned int vl;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vlse32.v v0, (%0), %1" ::"r"(src), "r"(8) : "memory");
    asm volatile("vse32.v v0, (%0)" ::"r"(dst_re) : "memory");
    opope_vsync(raw);
    asm volatile("vlse32.v v8, (%0), %1" ::"r"(src + 1), "r"(8) : "memory");
    asm volatile("vse32.v v8, (%0)" ::"r"(dst_im) : "memory");
    asm volatile("vfmul.vf v8, v8, %0" ::"f"(-1.0f));
    asm volatile("vse32.v v8, (%0)" ::"r"(dst_nim) : "memory");
    opope_vsync(raw);
    src += 2 * vl;
    dst_re += vl;
    dst_im += vl;
    dst_nim += vl;
    n -= vl;
  }
}

// dst[2i] = re[i], dst[2i+1] = im[i]
static void interleave(float *dst, const float *re, const float *im, unsigned int n) {
  unsigned int vl;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vle32.v v0, (%0)" ::"r"(re) : "memory");
    asm volatile("vle32.v v8, (%0)" ::"r"(im) : "memory");
    asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst), "r"(8) : "memory");
    opope_vsync(raw);
    asm volatile("vsse32.v v8, (%0), %1" ::"r"(dst + 1), "r"(8) : "memory");
    opope_vsync(raw);
    dst += 2 * vl;
    re += vl;
    im += vl;
    n -= vl;
  }
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  unsigned int timer_start, timer_end, timer = 0;
  int errors = 0;

  if (cid == 0) {
    OPOPE_L1_ALLOC(raw, float, RAW_FLOATS);
    OPOPE_L1_ALLOC(planes, float, PLANES_FLOATS);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // Initialize matrices (interleaved complex data)
  if (cid == 0) {
    snrt_dma_start_1d(raw, cm_a_dram, 2 * NA * sizeof(float));
    snrt_dma_start_1d(raw + 2 * NA, cm_b_dram, 2 * NB * sizeof(float));
    snrt_dma_wait_all();
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    const float *raw_a = raw;
    const float *raw_b = raw + 2 * NA;
    float *art = planes;
    float *ait = art + NA;
    float *br = ait + NA;
    float *bi = br + NB;
    float *nbi = bi + NB;
    float *cr = raw;  // reuse of the (dead) interleaved operands
    float *ci = raw + NC;
    float *c_il = planes;  // reuse of the (dead) real operands

    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    start_kernel();

    // Pre-processing (Spatz): de-interleave, A transposed, -Bi
    OPOPE_TRACE("pre A\n");
    deinterleave(art, ait, raw_a, NA);
    OPOPE_TRACE("pre B\n");
    deinterleave_neg(br, bi, nbi, raw_b, NB);
    OPOPE_TRACE("zero\n");
    // Y = 0 for the two accumulations
    opope_vzero(cr, 2 * NC);  // cr and ci are contiguous

    OPOPE_TRACE("gemms\n");
    // Real GeMMs (O-POPE)
    opope_gemm(art, br, cr, M_SIZE, N_SIZE, K_SIZE);   // Cr  = Ar*Br
    opope_gemm(ait, nbi, cr, M_SIZE, N_SIZE, K_SIZE);  // Cr += Ai*(-Bi)
    opope_gemm(art, bi, ci, M_SIZE, N_SIZE, K_SIZE);   // Ci  = Ar*Bi
    opope_gemm(ait, br, ci, M_SIZE, N_SIZE, K_SIZE);   // Ci += Ai*Br

    OPOPE_TRACE("post\n");
    // Post-processing (Spatz): interleave the result
    interleave(c_il, cr, ci, NC);

    // End dump
    stop_kernel();

    // End timer
    timer_end = benchmark_get_cycle();
    timer = timer_end - timer_start;

    // Each complex MAC is 4 real MACs
    long unsigned int performance = 1000 * 8UL * M_SIZE * N_SIZE * K_SIZE / timer;

    PRINTF("\n----- (%dx%dx%d) O-POPE complex matmul -----\n", M_SIZE, N_SIZE, K_SIZE);
    PRINTF("The execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle.\n", performance);

    errors = opope_check_fp32(c_il, (const float *)cm_golden, 2 * NC, 1e-3f, 1e-3f);
    if (errors != 0) PRINTF("Error: %d mismatches\n", errors);
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  return errors;
}
