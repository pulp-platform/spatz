// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

#include "gemm.h"

enum {
    GEMM_TILE_EDGE = 16,
    GEMM_PACKED_TILE_EDGE = 16,
};

/*
 * Execute one tm-by-tn FP32 output tile on mt0.
 *
 * msetmtypei selects FP32, TWIDEN=1 and therefore LMUL=1 for TE=16. msettn is
 * deliberately used for both vector-load lengths: unlike vset[i]vl[i], it
 * preserves mtype and keeps the matrix unit configured.
 */
#define GEMM_FP32_TILE(AT_PTR, B_PTR, C_PTR, A_STRIDE, B_STRIDE, C_STRIDE,   \
                       K_COUNT, TM, TN)                                      \
    do {                                                                     \
        uintptr_t _at = (uintptr_t)(AT_PTR);                                 \
        uintptr_t _b = (uintptr_t)(B_PTR);                                   \
        uintptr_t _c = (uintptr_t)(C_PTR);                                   \
        uintptr_t _a_stride = (uintptr_t)(A_STRIDE);                         \
        uintptr_t _b_stride = (uintptr_t)(B_STRIDE);                         \
        uintptr_t _c_stride = (uintptr_t)(C_STRIDE);                         \
        uintptr_t _k_count = (uintptr_t)(K_COUNT);                           \
        uintptr_t _tm = (uintptr_t)(TM);                                     \
        uintptr_t _tn = (uintptr_t)(TN);                                     \
        uintptr_t _tss;                                                      \
        uintptr_t _tss_end;                                                  \
                                                                             \
        asm volatile(                                                        \
            "msettm x0, %[tm]\n"                                            \
            "msettn x0, %[tn]\n"                                            \
            "vtzero mt0\n"                                                  \
            "beqz %[k_count], .Lgemm_store_%=\n"                             \
                                                                             \
            ".Lgemm_k_%=:\n"                                                \
            "msettn x0, %[tm]\n"                                            \
            "vle32.v v0, (%[at])\n"                                         \
            "add %[at], %[at], %[a_stride]\n"                              \
            "msettn x0, %[tn]\n"                                            \
            "vle32.v v16, (%[b])\n"                                         \
            "add %[b], %[b], %[b_stride]\n"                                \
            "vtfmm.tvv mt0, v0, v16\n"                                     \
            "addi %[k_count], %[k_count], -1\n"                             \
            "bnez %[k_count], .Lgemm_k_%=\n"                                \
                                                                             \
            ".Lgemm_store_%=:\n"                                            \
            "msettn x0, %[tn]\n"                                            \
            "li %[tss], 0\n"                                                \
            "add %[tss_end], %[tss], %[tm]\n"                               \
            ".Lgemm_store_row_%=:\n"                                        \
            "vtse32 %[tss], (%[c])\n"                                       \
            "add %[c], %[c], %[c_stride]\n"                                \
            "addi %[tss], %[tss], 1\n"                                      \
            "bltu %[tss], %[tss_end], .Lgemm_store_row_%=\n"                 \
            : [at] "+&r"(_at), [b] "+&r"(_b), [c] "+&r"(_c),              \
              [k_count] "+&r"(_k_count), [tss] "=&r"(_tss),                \
              [tss_end] "=&r"(_tss_end)                                     \
            : [a_stride] "r"(_a_stride), [b_stride] "r"(_b_stride),         \
              [c_stride] "r"(_c_stride), [tm] "r"(_tm), [tn] "r"(_tn)      \
            : "memory");                                                     \
    } while (0)

static inline void gemm_fp32_configure(void) {
    uintptr_t tk = 1;

    asm volatile(
        "msetmtypei 1, 2\n"
        "msettk x0, %[tk]\n"
        :
        : [tk] "r"(tk)
        : "memory");
}

/*
 * General, tightly packed ABI:
 *   addrA = At[K][M], addrB = B[K][N], addrC = C[M][N].
 *
 * The physical accumulator has a fixed TE=16. Runtime tm/tn select the
 * active rectangle, while each VTFMM consumes one FP32 K row (tk=1) from the
 * vectors rooted at v0 and v16.
 */
__attribute__((noinline, aligned(64))) void gemm_fp32(
    void *addrA, void *addrB, void *addrC,
    int K, int N, int M, int alt_fmt)
{
    const float *at = (const float *)addrA;
    const float *b = (const float *)addrB;
    float *c = (float *)addrC;

    if (M <= 0 || N <= 0)
        return;
    if (K < 0 || alt_fmt != 0)
        __builtin_trap();

    gemm_fp32_configure();

    for (int m = 0; m < M; m += GEMM_TILE_EDGE) {
        const int tm = M - m < GEMM_TILE_EDGE ? M - m : GEMM_TILE_EDGE;

        for (int n = 0; n < N; n += GEMM_TILE_EDGE) {
            const int tn = N - n < GEMM_TILE_EDGE ? N - n : GEMM_TILE_EDGE;

            GEMM_FP32_TILE(at + m, b + n, c + (uintptr_t)m * N + n,
                           (uintptr_t)M * sizeof(float),
                           (uintptr_t)N * sizeof(float),
                           (uintptr_t)N * sizeof(float), K, tm, tn);
        }
    }
}

/*
 * Compatibility entry for the existing DMA driver.  Its A/B panels are
 * packed as independent K-by-16 tiles, while C remains row-major.  This path
 * uses the same matrix configuration and the same no-vset microkernel.
 */
__attribute__((noinline, aligned(64))) void gemm_block_fp32(
    void *addrA, void *addrB, void *addrC,
    int K, int N, int M, int alt_fmt)
{
    const float *a = (const float *)addrA;
    const float *b = (const float *)addrB;
    float *c = (float *)addrC;

    if (M <= 0 || N <= 0)
        return;
    if (K < 0 || alt_fmt != 0)
        __builtin_trap();

    gemm_fp32_configure();

    for (int m = 0; m < M; m += GEMM_PACKED_TILE_EDGE) {
        const int tm = M - m < GEMM_PACKED_TILE_EDGE
                           ? M - m : GEMM_PACKED_TILE_EDGE;
        const float *a_tile = a + (uintptr_t)(m / GEMM_PACKED_TILE_EDGE) *
                                      K * GEMM_PACKED_TILE_EDGE;

        for (int n = 0; n < N; n += GEMM_PACKED_TILE_EDGE) {
            const int tn = N - n < GEMM_PACKED_TILE_EDGE
                               ? N - n : GEMM_PACKED_TILE_EDGE;
            const float *b_tile = b + (uintptr_t)(n / GEMM_PACKED_TILE_EDGE) *
                                          K * GEMM_PACKED_TILE_EDGE;

            GEMM_FP32_TILE(a_tile, b_tile, c + (uintptr_t)m * N + n,
                           GEMM_PACKED_TILE_EDGE * sizeof(float),
                           GEMM_PACKED_TILE_EDGE * sizeof(float),
                           (uintptr_t)N * sizeof(float), K, tm, tn);
        }
    }
}

#undef GEMM_FP32_TILE
