// Copyright 2024 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Pei-Yu Lin <peilin@ethz.ch>

#include <snrt.h>
#include <stdint.h>
#include <string.h>
#include <printf.h>

#include DATAHEADER
#include "kernel/fp-gemm.c"

#ifndef PRINT_C_MATRIX
#define PRINT_C_MATRIX 0
#endif

#ifndef PRINT_A_B_MATRIX
#define PRINT_A_B_MATRIX 0
#endif

static inline float absf_local(float x)
{
    return x < 0.0f ? -x : x;
}

static void print_matrix_f32(const char *name,
                             const float *X,
                             uint32_t rows,
                             uint32_t cols)
{
    printf("\n%s = [\n", name);
    for (uint32_t i = 0; i < rows; i++) {
        printf("  ");
        for (uint32_t j = 0; j < cols; j++) {
            printf("% .7e", (double)X[i * cols + j]);
            if (j + 1 < cols) printf(", ");
        }
        if (i + 1 < rows) printf(";\n");
        else             printf("\n");
    }
    printf("]\n");
}

static int check_matrix_f32(const float *C,
                            const float *Cref,
                            uint32_t M,
                            uint32_t N,
                            float rel_tol,
                            float abs_tol,
                            float *max_abs_err,
                            float *max_rel_err,
                            uint32_t *max_idx)
{
    int n_err = 0;
    *max_abs_err = 0.0f;
    *max_rel_err = 0.0f;
    *max_idx = 0;

    for (uint32_t idx = 0; idx < M * N; idx++) {
        float ref = Cref[idx];
        float err = absf_local(C[idx] - ref);
        float den = absf_local(ref);
        if (den < 1.0e-9f) den = 1.0e-9f;

        float rel = err / den;

        if (err > *max_abs_err) {
            *max_abs_err = err;
            *max_rel_err = rel;
            *max_idx = idx;
        }

        if ((err > abs_tol) && (rel > rel_tol)) {
            n_err++;
        }
    }

    return n_err;
}

static inline float load_a_row_decoded(fmt_t fmt,
                                       const float *A_f32,
                                       const uint16_t *A16,
                                       const uint8_t *A8,
                                       uint32_t i,
                                       uint32_t k,
                                       uint32_t K)
{
    uint32_t idx = i * K + k;

    switch (fmt) {
        case FMT_F32:  return A_f32[idx];
        case FMT_F16:  return f16_to_f32(A16[idx]);
        case FMT_BF16: return bf16_to_f32(A16[idx]);
        case FMT_E4M3: return e4m3_to_f32(A8[idx]);
        case FMT_E5M2: return e5m2_to_f32(A8[idx]);
        default:       return 0.0f;
    }
}

static inline float load_b_decoded(fmt_t fmt,
                                   const float *B_f32,
                                   const uint16_t *B16,
                                   const uint8_t *B8,
                                   uint32_t k,
                                   uint32_t j,
                                   uint32_t N)
{
    uint32_t idx = k * N + j;

    switch (fmt) {
        case FMT_F32:  return B_f32[idx];
        case FMT_F16:  return f16_to_f32(B16[idx]);
        case FMT_BF16: return bf16_to_f32(B16[idx]);
        case FMT_E4M3: return e4m3_to_f32(B8[idx]);
        case FMT_E5M2: return e5m2_to_f32(B8[idx]);
        default:       return 0.0f;
    }
}

static void gemm_ref_decoded_rowA(float *Cref,
                                  fmt_t fmt_a,
                                  fmt_t fmt_b,
                                  const float *A_f32,
                                  const float *B_f32,
                                  const uint16_t *A16,
                                  const uint16_t *B16,
                                  const uint8_t *A8,
                                  const uint8_t *B8,
                                  uint32_t M,
                                  uint32_t N,
                                  uint32_t K)
{
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t j = 0; j < N; j++) {
            float acc = 0.0f;

            for (uint32_t k = 0; k < K; k++) {
                float a = load_a_row_decoded(fmt_a, A_f32, A16, A8, i, k, K);
                float b = load_b_decoded(fmt_b, B_f32, B16, B8, k, j, N);
                acc += a * b;
            }

            Cref[i * N + j] = acc;
        }
    }
}

static void print_header(void)
{
    printf("\n=== Dual GEMM functional check ===\n");
    printf("%-24s %-18s %10s %12s %12s %8s\n",
           "Format", "Kernel", "Errors", "MaxAbsErr", "MaxRelErr", "Status");
    printf("%-24s %-18s %10s %12s %12s %8s\n",
           "------", "------", "------", "---------", "---------", "------");
}

static void print_row(const char *format,
                      const char *kernel,
                      int errors,
                      float max_abs_err,
                      float max_rel_err)
{
    printf("%-24s %-18s %10d %12.5e %12.5e %8s\n",
           format, kernel, errors,
           (double)max_abs_err, (double)max_rel_err,
           errors == 0 ? "PASS" : "FAIL");
}

int main(void)
{
    if (snrt_cluster_core_idx() != 0) {
        snrt_cluster_hw_barrier();
        return 0;
    }

    enable_vec();
    enable_fp();
    enable_vme();

    const uint32_t M  = gemm_l.M;
    const uint32_t N  = gemm_l.N;
    const uint32_t K  = gemm_l.K;
    const uint32_t TM = gemm_l.TM;
    const uint32_t TN = gemm_l.TN;

    if (M != 64 || N != 64 || K != 64) {
        printf("ERROR: main_functional_check_dual.c is intended for 64x64x64 only.\n");
        printf("Got M=%u N=%u K=%u\n",
               (unsigned)M, (unsigned)N, (unsigned)K);
        snrt_cluster_hw_barrier();
        return 1;
    }

    float    *A_f32  = (float    *)snrt_l1alloc(M * K * sizeof(float)); // 16 KB
    float    *B_f32  = (float    *)snrt_l1alloc(K * N * sizeof(float)); // 16 KB
    float    *C      = (float    *)snrt_l1alloc(M * N * sizeof(float)); // 16 KB
    float    *Cref   = (float    *)snrt_l1alloc(M * N * sizeof(float)); // 16 KB

    uint16_t *A16    = (uint16_t *)snrt_l1alloc(M * K * sizeof(uint16_t)); // 8 KB
    uint16_t *B16    = (uint16_t *)snrt_l1alloc(K * N * sizeof(uint16_t)); // 8 KB

    uint8_t  *A8     = (uint8_t  *)snrt_l1alloc(M * K * sizeof(uint8_t));   // 4 KB
    uint8_t  *B8     = (uint8_t  *)snrt_l1alloc(K * N * sizeof(uint8_t));   // 4 KB

    if (!A_f32 || !B_f32 || !C || !Cref ||
        !A16 || !B16 || !A8 || !B8) {
        printf("ERROR: snrt_l1alloc failed.\n");
        snrt_cluster_hw_barrier();
        return 1;
    }

    snrt_dma_start_1d(A_f32, gemm_A_f32_dram, M * K * sizeof(float));
    snrt_dma_start_1d(B_f32, gemm_B_f32_dram, K * N * sizeof(float));
    snrt_dma_wait_all();

#if PRINT_A_B_MATRIX
    print_matrix_f32("A_gemm_A_f32_dram_row_major", A_f32, M, K);
    print_matrix_f32("B_gemm_B_f32_dram_row_major", B_f32, K, N);
#endif

    printf("\n=== DATAHEADER generated GEMM ===\n");
    printf("M=%u N=%u K=%u TM=%u TN=%u\n",
           (unsigned)M, (unsigned)N, (unsigned)K,
           (unsigned)TM, (unsigned)TN);

    print_header();

    int total_err = 0;
    float max_abs = 0.0f;
    float max_rel = 0.0f;
    uint32_t max_idx = 0;
    int err = 0;

#define RUN_ONE(format_label, kernel_label, kernel_call, rel_tol, abs_tol) do { \
        memset(C, 0, M * N * sizeof(float));                                    \
        kernel_call;                                                            \
        err = check_matrix_f32(C, Cref, M, N, (rel_tol), (abs_tol),             \
                               &max_abs, &max_rel, &max_idx);                  \
        total_err += err;                                                       \
        print_row((format_label), (kernel_label), err, max_abs, max_rel);       \
        if (PRINT_C_MATRIX) {                                                   \
            print_matrix_f32((format_label), C, M, N);                          \
        }                                                                       \
    } while (0)

    // ─────────────────────────────────────────────────────────────────────
    // FP32
    // ─────────────────────────────────────────────────────────────────────
    gemm_ref_decoded_rowA(Cref, FMT_F32, FMT_F32,
                          A_f32, B_f32, NULL, NULL, NULL, NULL,
                          M, N, K);

    RUN_ONE("fp32", "Arow+vlse",
            gemm_fp32(C, A_f32, B_f32, M, N, K, TM, TN),
            1.0e-3f, 1.0e-3f);

    // ─────────────────────────────────────────────────────────────────────
    // FP16
    // ─────────────────────────────────────────────────────────────────────
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t k = 0; k < K; k++) {
            uint16_t v = f32_to_f16(A_f32[i * K + k]);
            A16[i * K + k] = v;
        }
    }
    for (uint32_t k = 0; k < K; k++) {
        for (uint32_t j = 0; j < N; j++) {
            B16[k * N + j] = f32_to_f16(B_f32[k * N + j]);
        }
    }

    gemm_ref_decoded_rowA(Cref, FMT_F16, FMT_F16,
                          A_f32, B_f32, A16, B16, NULL, NULL,
                          M, N, K);

    RUN_ONE("fp16", "Arow+vlse",
            gemm_fp16(C, A16, B16, M, N, K, TM, TN),
            5.0e-2f, 1.0e-2f);

    // ─────────────────────────────────────────────────────────────────────
    // BF16
    // ─────────────────────────────────────────────────────────────────────
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t k = 0; k < K; k++) {
            uint16_t v = f32_to_bf16(A_f32[i * K + k]);
            A16[i * K + k] = v;
        }
    }
    for (uint32_t k = 0; k < K; k++) {
        for (uint32_t j = 0; j < N; j++) {
            B16[k * N + j] = f32_to_bf16(B_f32[k * N + j]);
        }
    }

    gemm_ref_decoded_rowA(Cref, FMT_BF16, FMT_BF16,
                          A_f32, B_f32, A16, B16, NULL, NULL,
                          M, N, K);

    RUN_ONE("bf16", "Arow+vlse",
            gemm_bf16(C, A16, B16, M, N, K, TM, TN),
            5.0e-2f, 1.0e-2f);

    // ─────────────────────────────────────────────────────────────────────
    // E4M3 x E4M3
    // ─────────────────────────────────────────────────────────────────────
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t k = 0; k < K; k++) {
            uint8_t v = f32_to_e4m3(A_f32[i * K + k]);
            A8[i * K + k] = v;
        }
    }
    for (uint32_t k = 0; k < K; k++) {
        for (uint32_t j = 0; j < N; j++) {
            B8[k * N + j] = f32_to_e4m3(B_f32[k * N + j]);
        }
    }

    gemm_ref_decoded_rowA(Cref, FMT_E4M3, FMT_E4M3,
                          A_f32, B_f32, NULL, NULL, A8, B8,
                          M, N, K);

    RUN_ONE("e4m3xe4m3", "Arow+vlse",
            gemm_e4m3_e4m3(C, A8, B8, M, N, K, TM, TN),
            2.0e-1f, 1.0f);

    // ─────────────────────────────────────────────────────────────────────
    // E4M3 x E5M2
    // ─────────────────────────────────────────────────────────────────────
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t k = 0; k < K; k++) {
            uint8_t v = f32_to_e4m3(A_f32[i * K + k]);
            A8[i * K + k] = v;
        }
    }
    for (uint32_t k = 0; k < K; k++) {
        for (uint32_t j = 0; j < N; j++) {
            B8[k * N + j] = f32_to_e5m2(B_f32[k * N + j]);
        }
    }

    gemm_ref_decoded_rowA(Cref, FMT_E4M3, FMT_E5M2,
                          A_f32, B_f32, NULL, NULL, A8, B8,
                          M, N, K);

    RUN_ONE("e4m3xe5m2", "Arow+vlse",
            gemm_e4m3_e5m2(C, A8, B8, M, N, K, TM, TN),
            2.0e-1f, 1.0f);

    // ─────────────────────────────────────────────────────────────────────
    // E5M2 x E4M3
    // ─────────────────────────────────────────────────────────────────────
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t k = 0; k < K; k++) {
            uint8_t v = f32_to_e5m2(A_f32[i * K + k]);
            A8[i * K + k] = v;
        }
    }
    for (uint32_t k = 0; k < K; k++) {
        for (uint32_t j = 0; j < N; j++) {
            B8[k * N + j] = f32_to_e4m3(B_f32[k * N + j]);
        }
    }

    gemm_ref_decoded_rowA(Cref, FMT_E5M2, FMT_E4M3,
                          A_f32, B_f32, NULL, NULL, A8, B8,
                          M, N, K);

    RUN_ONE("e5m2xe4m3", "Arow+vlse",
            gemm_e5m2_e4m3(C, A8, B8, M, N, K, TM, TN),
            2.0e-1f, 1.0f);

    // ─────────────────────────────────────────────────────────────────────
    // E5M2 x E5M2
    // ─────────────────────────────────────────────────────────────────────
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t k = 0; k < K; k++) {
            uint8_t v = f32_to_e5m2(A_f32[i * K + k]);
            A8[i * K + k] = v;
        }
    }
    for (uint32_t k = 0; k < K; k++) {
        for (uint32_t j = 0; j < N; j++) {
            B8[k * N + j] = f32_to_e5m2(B_f32[k * N + j]);
        }
    }

    gemm_ref_decoded_rowA(Cref, FMT_E5M2, FMT_E5M2,
                          A_f32, B_f32, NULL, NULL, A8, B8,
                          M, N, K);

    RUN_ONE("e5m2xe5m2", "Arow+vlse",
            gemm_e5m2_e5m2(C, A8, B8, M, N, K, TM, TN),
            2.0e-1f, 1.0f);

#undef RUN_ONE

    printf("\nOverall Status: %s\n", total_err == 0 ? "PASS" : "FAIL");
    printf("Total errors across all checks: %d\n", total_err);

    snrt_cluster_hw_barrier();
    return total_err == 0 ? 0 : 1;
}