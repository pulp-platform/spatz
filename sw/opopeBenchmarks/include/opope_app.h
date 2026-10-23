// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Common definitions of the O-POPE + Spatz applications.
//
// O-POPE computes Z = Y + X*W in FP32 (X: MxN, W: NxK, Y/Z: MxK), in place over Y, with
//   - X stored TRANSPOSED in memory: element X[m][n] is at xT[n*M + m],
//   - W and Y/Z stored row-major.
// A plain matmul is obtained by zero-filling Y.

#ifndef OPOPE_APP_H
#define OPOPE_APP_H

// Enable snRuntime printf (PRINTF is a no-op otherwise)
#ifndef __SNRT_USE_PRINT
#define __SNRT_USE_PRINT
#endif

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include "hal_opope.h"

#ifdef OPOPE_VERBOSE
#define OPOPE_VERBOSE_CHECK 1
#else
#define OPOPE_VERBOSE_CHECK 0
#endif

// Phase tracing, enabled with -DOPOPE_VERBOSE (useful to locate hangs)
#ifdef OPOPE_VERBOSE
#define OPOPE_TRACE(...) PRINTF(__VA_ARGS__)
#else
#define OPOPE_TRACE(...) ((void)0)
#endif

#define OPOPE_MAX(a, b) ((a) > (b) ? (a) : (b))
#define OPOPE_MIN(a, b) ((a) < (b) ? (a) : (b))

// Allocate `count` elements of `type` in L1 (the O-POPE only accesses the TCDM). Returns -1 from
// the calling function if the L1 is exhausted.
#define OPOPE_L1_ALLOC(ptr, type, count)                                    \
  do {                                                                      \
    (ptr) = (type *)snrt_l1alloc((count) * sizeof(type));                   \
    if ((ptr) == 0) {                                                       \
      PRINTF("L1 allocation of %u bytes failed\n",                          \
             (unsigned)((count) * sizeof(type)));                           \
      return -1;                                                            \
    }                                                                       \
  } while (0)

// Wait until all the vector memory operations issued so far have completed: the scalar core cannot
// issue a load while Spatz memory requests are in flight, so a dummy scalar load from the TCDM
// drains them. `tcdm_ptr` must point into the L1.
//
// CAUTION with STRIDED vector accesses (vlse32/vsse32) on this cluster: with some strides and
// addresses (observed: 320 B and 128 B, with two accesses back to back) the VLSU never completes
// (the simulation looks hung), while stride 8 B (de/interleaving 32-bit pairs) is fine. Do not
// use strides other than 8 B; avoid transposes through strided accesses (lay the data out so that
// no transpose is needed). Also avoid odd vl with strided accesses (the last element is wrong).
// Drain after strided accesses to keep a single one in flight.
static inline void opope_vsync(const void *tcdm_ptr) { (void)*(volatile uint32_t *)tcdm_ptr; }

// FP32 GeMM on O-POPE: Z = Y + X*W (see above for the layouts). Returns when the job is done.
// The core sleeps on the O-POPE end-of-job event (machine external interrupt).
static inline void opope_gemm(const float *xT, const float *w, float *y, unsigned int m,
                              unsigned int n, unsigned int k) {
  opope_vsync(xT);  // all the vector stores that prepare the operands must have reached the L1
  hwpe_soft_clear();
  while (hwpe_acquire_job() < 0)
    ;
  snrt_interrupt_enable(IRQ_M_EXT);
  opope_cfg((unsigned int)xT, (unsigned int)w, (unsigned int)y, m, n, k, (uint8_t)GEMM,
            (uint8_t)Float32, (uint8_t)Float32);
  hwpe_trigger_job();
  while (hwpe_get_status() != 0) snrt_wfi();
  snrt_interrupt_disable(IRQ_M_EXT);
}

// O-POPE constraint (observed): M must be a multiple of 16. Otherwise the job processes M rounded up
// to a multiple of 16 and also WRITES the extra rows past the end of Z (garbage), corrupting
// whatever follows Z in memory (N and K may be any size >= 16). Use the assert in the data
// generators, or pad Z with extra rows.

// Compare FP32 arrays: |a - g| <= abs_tol + rel_tol * |g|. Returns the number of mismatches.
static inline int opope_check_fp32(const float *actual, const float *golden, unsigned int len,
                                   float rel_tol, float abs_tol) {
  int errors = 0;
  for (unsigned int i = 0; i < len; i++) {
    float d = actual[i] - golden[i];
    if (d < 0) d = -d;
    float g = golden[i] < 0 ? -golden[i] : golden[i];
    if (!(d <= abs_tol + rel_tol * g)) {  // also catches NaN
      if (errors == 0 || (errors < 8 && OPOPE_VERBOSE_CHECK))
        PRINTF("Mismatch @ %u: got %f, expected %f\n", i, (double)actual[i], (double)golden[i]);
      errors++;
    }
  }
  return errors;
}

// ---------------------------------------------------------------------------------------------
// Small RVV helpers (FP32, LMUL=8, 1-D loops)
// ---------------------------------------------------------------------------------------------

// dst[0..n) = 0
static inline void opope_vzero(float *dst, unsigned int n) {
  unsigned int vl;
  while (n > 0) {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(n));
    asm volatile("vmv.v.i v0, 0");
    asm volatile("vse32.v v0, (%0)" ::"r"(dst) : "memory");
    dst += vl;
    n -= vl;
  }
}

#endif  // OPOPE_APP_H
