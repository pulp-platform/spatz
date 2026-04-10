/*  sanity_check.h – Fast post-kernel sanity check for all fp32 benchmarks.
 *
 *  Call:  sanity_check("COS_SW", g_out, outCos, N, 4);
 *
 *  Checks:
 *    1. Output not all-zero  (kernel didn't execute / illegal-instruction)
 *    2. No NaN values        (corrupted computation)
 *    3. First few outputs vs golden for a quick visual spot-check
 *    4. Max absolute error across the whole array
 *
 *  Prints one-line PASS / per-issue FAIL, then the spot-check values.
 *  Keeps output minimal so you can scan 16 kernels fast.
 */
#pragma once
#include <math.h>
#include "printf.h"

static inline void sanity_check(const char *label,
                                const float *out,
                                const float *ref,
                                int N,
                                int nspot)
{
    int all_zero = 1, nan_cnt = 0;
    float max_err = 0.0f;

    for (int i = 0; i < N; i++) {
        if (out[i] != 0.0f) all_zero = 0;
        if (out[i] != out[i]) nan_cnt++;                 /* NaN */
        float d = fabsf(out[i] - ref[i]);
        if (d > max_err) max_err = d;
    }

    if (all_zero)
        printf("[%s] FAIL: output entirely zero\n", label);
    if (nan_cnt)
        printf("[%s] FAIL: %d NaN values\n", label, nan_cnt);
    if (!all_zero && !nan_cnt)
        printf("[%s] PASS  max_err=%f\n", label, max_err);

    /* spot-check */
    if (nspot > N) nspot = N;
    for (int i = 0; i < nspot; i++)
        printf("[%s]  [%d] out=%f  exp=%f\n", label, i, out[i], ref[i]);
}
