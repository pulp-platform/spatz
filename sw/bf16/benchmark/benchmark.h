// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <snrt.h>
#include <stddef.h>
#include <stdint.h>

#include "printf.h"

size_t benchmark_get_cycle();

void start_kernel();
void stop_kernel();

/* Lightweight validation with relative tolerance (~3%).
 * Converts BF16 bit patterns to float, uses multiply-based comparison
 * to avoid fdiv.s (not supported on Snitch).
 * Prints 1-line summary + up to MAX_PRINT mismatches.
 * Returns the number of elements outside tolerance.              */
static inline float bf16_to_float(uint16_t b) {
    uint32_t w = (uint32_t)b << 16;          /* BF16 → upper 16 bits of FP32 */
    float f;
    __builtin_memcpy(&f, &w, sizeof(f));
    return f;
}

static inline float fabsf_inline(float x) { return x < 0.0f ? -x : x; }

static inline int check_bf16(const uint16_t *got, const uint16_t *ref, int n,
                             const char *tag) {
    enum { MAX_PRINT = 5 };
    const float TOL = 0.03f;                 /* 3 % relative tolerance */
    int errs = 0;
    for (int i = 0; i < n; i++) {
        float g = bf16_to_float(got[i]);
        float r = bf16_to_float(ref[i]);
        float diff = fabsf_inline(g - r);
        float absref = fabsf_inline(r);
        /* Check: diff > TOL * absref  ⟺  diff > 0.03 * |ref|
         * Uses multiply instead of divide to avoid fdiv.s          */
        int fail;
        if (absref < 1e-6f)
            fail = (diff > 1e-6f);           /* near-zero ref: absolute check */
        else
            fail = (diff > TOL * absref);    /* rel check via multiply */
        if (fail) {
            if (errs < MAX_PRINT)
                printf("  [%s] idx %d: got 0x%04X  ref 0x%04X\n",
                       tag, i, got[i], ref[i]);
            errs++;
        }
    }
    if (errs > MAX_PRINT)
        printf("  [%s] ... and %d more beyond 3%% tolerance\n", tag, errs - MAX_PRINT);
    printf("[%s] %s  (%d / %d within 3%%)\n",
           tag, errs == 0 ? "PASS" : "FAIL", n - errs, n);
    return errs;
}
