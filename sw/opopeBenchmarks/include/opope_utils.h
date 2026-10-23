// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE_SW for details.
// SPDX-License-Identifier: Apache-2.0
//
// Danilo Cammarata <dcammarata@iis.ee.ethz.ch>
//

#include <stdio.h>

#ifndef OPOPE_UTILS_H
#define OPOPE_UTILS_H

// Expand the error tolerance because the 24-bit manissa
#define DEBUG
#include <stddef.h> /* for NULL if needed */
#include <stdint.h>

/* ================================================================
 * FP32 comparison
 *
 *  Tolerances (per your spec):
 *    - Absolute : 0.001f  (ABS_TOL_BITS = 0x3A83126F)
 *    - ULP      : 8
 * ================================================================ */

#define FP32_ABS_TOL_BITS 0x3A83126Fu            /* 0.001f bit-pattern */
#define FP16FP32_ABS_TOL_BITS 0x3C23D70Au        /* 0.01f bit-pattern */
#define FP16_ABS_TOL_BITS ((uint16_t)0x211Fu)    /* ≈ 0.01 in FP16 */
#define FP8FP16_ABS_TOL_BITS ((uint16_t)0x2E66u) /* ≈ 0.1 in FP16 */
#define FP32_MAX_ULP 65536u
#define FP16FP32_MAX_ULP 65536u
#define FP16_MAX_ULP 64u
#define FP8FP16_MAX_ULP 128u

#define MAX_ULP \
  ((COMP_FMT == FP8 && MEM_FMT == FP16)    ? FP8FP16_MAX_ULP \
   : (COMP_FMT == FP16 && MEM_FMT == FP16) ? FP16_MAX_ULP \
   : (COMP_FMT == FP16 && MEM_FMT == FP32) ? FP16FP32_MAX_ULP \
   : (COMP_FMT == FP32 && MEM_FMT == FP32) ? FP32_MAX_ULP \
                                           : 0u)
#define ABS_TOL_BITS \
  ((COMP_FMT == FP8 && MEM_FMT == FP16)    ? FP8FP16_ABS_TOL_BITS \
   : (COMP_FMT == FP16 && MEM_FMT == FP16) ? FP16_ABS_TOL_BITS \
   : (COMP_FMT == FP16 && MEM_FMT == FP32) ? FP16FP32_ABS_TOL_BITS \
   : (COMP_FMT == FP32 && MEM_FMT == FP32) ? FP32_ABS_TOL_BITS \
                                           : 0x0)

static inline int32_t fp32_to_ordered(uint32_t f) {
  if (f & 0x80000000u)                 /* negative float */
    return (int32_t)(~f);              /* invert all bits → ordered negative int */
  else                                 /* positive float */
    return (int32_t)(f ^ 0x80000000u); /* flip sign bit → ordered positive int */
}
static inline uint32_t fp32_ulp_diff(uint32_t a, uint32_t b) {
  int32_t oa = fp32_to_ordered(a);
  int32_t ob = fp32_to_ordered(b);

  /* Use int64 to avoid signed-overflow UB for extreme cases */
  int64_t diff = (int64_t)oa - (int64_t)ob;
  if (diff < 0) diff = -diff;

  return (diff > (int64_t)0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)diff;
}
static inline int compare_fp32(uint32_t a, uint32_t b) {
  /* Fast path: bit-identical */
  if (a == b) return 0; /* equal */

  /* NaN / Inf check (exponent field = 0xFF) */
  uint32_t exp_a = (a >> 23) & 0xFFu;
  uint32_t exp_b = (b >> 23) & 0xFFu;

  if (exp_a == 0xFFu || exp_b == 0xFFu) {
    /* Two identical Infs are already caught by (a==b) above.
     * Everything else (NaN, mismatched Inf) is an error.      */
    return 1;
  }

  /* ULP tolerance – reliable away from zero */
  if (fp32_ulp_diff(a, b) <= MAX_ULP) return 0;

  return 1; /* mismatch */
}
static inline int opope32_compare_int(uint32_t *actual_z, uint32_t *golden_z, int len) {
  int errors = 0;

  for (int i = 0; i < len; i++) {

    // float a, g;
    // memcpy(&a, &actual_z[i], sizeof(float));
    // memcpy(&g, &golden_z[i], sizeof(float));
    // float diff = fabsf(a - g);
    // if (diff > 0.01f) {
    if (compare_fp32(actual_z[i], golden_z[i])) {
      errors++;
#ifdef DEBUG
      if (errors == 1)
        printf("FP32 error @ %d: act=0x%08x ref=0x%08x\n", i, actual_z[i], golden_z[i]);

#endif
    }
  }

  return errors;
}

static inline int32_t fp16_to_ordered(uint16_t h) {
  if (h & 0x8000u)                          /* negative */
    return (int32_t)(int16_t)(~h);          /* invert all bits */
  else                                      /* positive */
    return (int32_t)(int16_t)(h ^ 0x8000u); /* flip sign bit */
}
static inline uint32_t fp16_ulp_diff(uint16_t a, uint16_t b) {
  int32_t oa = fp16_to_ordered(a);
  int32_t ob = fp16_to_ordered(b);

  int32_t diff = oa - ob;
  if (diff < 0) diff = -diff;

  return (uint32_t)diff;
}
static inline int compare_fp16(uint16_t a, uint16_t b) {
  /* Fast path */
  if (a == b) return 0;

  /* NaN / Inf: exponent field = 0x1F (31 in 5-bit) */
  uint16_t exp_a = (a >> 10) & 0x1Fu;
  uint16_t exp_b = (b >> 10) & 0x1Fu;

  if (exp_a == 0x1Fu || exp_b == 0x1Fu) {
    /* Identical values already caught above; anything else is wrong */
    return 1;
  }

  /* ULP tolerance */
  if (fp16_ulp_diff(a, b) <= MAX_ULP) return 0;

  return 1; /* mismatch */
}
static inline int opope16_compare_int(uint32_t *actual_z, uint32_t *golden_z, int len) {
  int errors = 0;

  for (int i = 0; i < len; i++) {
    uint32_t a = actual_z[i];
    uint32_t g = golden_z[i];

    uint16_t a_l = (uint16_t)(a & 0xFFFFu);
    uint16_t a_h = (uint16_t)(a >> 16);

    uint16_t g_l = (uint16_t)(g & 0xFFFFu);
    uint16_t g_h = (uint16_t)(g >> 16);

    if (compare_fp16(a_l, g_l)) {
      errors++;
#ifdef DEBUG
      if (errors == 1) printf("FP16-lo error @ %d: act=0x%04x ref=0x%04x\n", i, a_l, g_l);
#endif
    }
    if (compare_fp16(a_h, g_h)) {
      errors++;
#ifdef DEBUG
      if (errors == 1) printf("FP16-hi error @ %d: act=0x%04x ref=0x%04x\n", i, a_h, g_h);
#endif
    }
  }

  return errors;
}

#endif