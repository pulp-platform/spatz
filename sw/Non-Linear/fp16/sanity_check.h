/*  sanity_check.h – Fast post-kernel sanity check for all fp16 benchmarks.
 *
 *  Call:  sanity_check("COS_SW", g_out, outCos, N, 4);
 *
 *  Checks:
 *    1. Output not all-zero  (kernel didn't execute / illegal-instruction)
 *    2. No NaN values        (corrupted computation)
 *    3. First few outputs vs golden for a quick visual spot-check
 *    4. Number of bit-exact mismatches
 *
 *  Prints one-line PASS / per-issue FAIL, then the spot-check values.
 *  Keeps output minimal so you can scan 16 kernels fast.
 */
#pragma once
#include <stdint.h>
#include "printf.h"

static inline int is_fp16_nan_sc(uint16_t x) {
    return ((x & 0x7C00) == 0x7C00) && ((x & 0x03FF) != 0);
}

static inline void sanity_check(const char *label,
                                const uint16_t *out,
                                const uint16_t *ref,
                                int N,
                                int nspot)
{
    int all_zero = 1, nan_cnt = 0, mismatch = 0;

    for (int i = 0; i < N; i++) {
        if (out[i] != 0) all_zero = 0;
        if (is_fp16_nan_sc(out[i])) nan_cnt++;
        if (out[i] != ref[i]) mismatch++;
    }

    if (all_zero)
        printf("[%s] FAIL: output entirely zero\n", label);
    if (nan_cnt)
        printf("[%s] FAIL: %d NaN values\n", label, nan_cnt);
    if (!all_zero && !nan_cnt)
        printf("[%s] PASS  mismatches=%d/%d\n", label, mismatch, N);

    /* spot-check */
    if (nspot > N) nspot = N;
    for (int i = 0; i < nspot; i++)
        printf("[%s]  [%d] out=0x%04x  exp=0x%04x\n", label, i, out[i], ref[i]);
}

/* Note: check_fp16() is provided by benchmark/benchmark.h
 * with relative-tolerance validation.  */
