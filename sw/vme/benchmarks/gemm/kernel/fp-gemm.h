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

#pragma once

// fp-gemm.h
// VME 64×64 GEMM kernel declarations + shared utilities.
//
// Kernel format / instruction mapping:
//   gemm_fp32      – fp32  SEW=e32 TK=1  vtfmm.tvv
//   gemm_fp16      – fp16  SEW=e16 TK=2  vtfmm.tvv     (clear_altfmt)
//   gemm_bf16      – BF16  SEW=e16 TK=2  vtfmm.alt.tvv (set_altfmt)
//   gemm_e4m3_e4m3 – E4M3×E4M3 SEW=e8 TK=4  vtfmm.tvv     altfmt=0
//   gemm_e4m3_e5m2 – E4M3×E5M2 SEW=e8 TK=4  vtfmm.tvv     altfmt=1
//   gemm_e5m2_e4m3 – E5M2×E4M3 SEW=e8 TK=4  vtfmm.alt.tvv altfmt=0
//   gemm_e5m2_e5m2 – E5M2×E5M2 SEW=e8 TK=4  vtfmm.alt.tvv altfmt=1

#include <stdint.h>
#include <string.h>
#include <printf.h>

// ─── Cycle counter ────────────────────────────────────────────────────────────

static inline uint32_t get_cycle(void)
{
    uint32_t c;
    asm volatile("csrr %0, mcycle" : "=r"(c));
    return c;
}

// ─── Format converters (fp32 → low-precision) ────────────────────────────────

static uint16_t f32_to_f16(float f);
static uint16_t f32_to_bf16(float f);
static uint8_t  f32_to_e4m3(float f);
static uint8_t  f32_to_e5m2(float f);

// ─── VME GEMM kernels ────────────────────────────────────────────────────────
// Parameters:
//   C        – output matrix [M × N], fp32, row-major
//   At / At_* – pre-transposed A [K × M] in the named element format
//   B  / B_*  – B matrix [K × N] in the named element format
//   M, N, K  – matrix dimensions (M % (2*TM) == 0, N % (2*TN) == 0, K % TK == 0)
//   TM, TN   – micro-tile sizes (typically 16)

void gemm_fp32(float *C, const float *At, const float *B,
               uint32_t M, uint32_t N, uint32_t K, uint32_t TM, uint32_t TN);

void gemm_fp16(float *C, const uint16_t *At_fp16, const uint16_t *B_fp16,
               uint32_t M, uint32_t N, uint32_t K, uint32_t TM, uint32_t TN);

void gemm_bf16(float *C, const uint16_t *At_bf16, const uint16_t *B_bf16,
               uint32_t M, uint32_t N, uint32_t K, uint32_t TM, uint32_t TN);

void gemm_e4m3_e4m3(float *C, const uint8_t *At_e4m3, const uint8_t *B_e4m3,
                    uint32_t M, uint32_t N, uint32_t K, uint32_t TM, uint32_t TN);

void gemm_e4m3_e5m2(float *C, const uint8_t *At_e4m3, const uint8_t *B_e5m2,
                    uint32_t M, uint32_t N, uint32_t K, uint32_t TM, uint32_t TN);

void gemm_e5m2_e4m3(float *C, const uint8_t *At_e5m2, const uint8_t *B_e4m3,
                    uint32_t M, uint32_t N, uint32_t K, uint32_t TM, uint32_t TN);

void gemm_e5m2_e5m2(float *C, const uint8_t *At_e5m2, const uint8_t *B_e5m2,
                    uint32_t M, uint32_t N, uint32_t K, uint32_t TM, uint32_t TN);

// ─── Compute-only microbenchmark kernels ─────────────────────────────────────
// Pre-load one set of registers, then time a tight loop of vtfmm instructions
// with no memory traffic to measure arithmetic peak.  Returns elapsed cycles.

#ifndef COMPUTE_ONLY_REPS
#define COMPUTE_ONLY_REPS 64
#endif

uint32_t compute_only_fp32(const float *At, const float *B,
                           uint32_t M, uint32_t N, uint32_t K,
                           uint32_t TM, uint32_t TN);

uint32_t compute_only_fp16(const uint16_t *At_fp16, const uint16_t *B_fp16,
                           uint32_t M, uint32_t N, uint32_t K,
                           uint32_t TM, uint32_t TN);

uint32_t compute_only_bf16(const uint16_t *At_bf16, const uint16_t *B_bf16,
                           uint32_t M, uint32_t N, uint32_t K,
                           uint32_t TM, uint32_t TN);

uint32_t compute_only_fp8_tvv(const uint8_t *At8, const uint8_t *B8, int altfmt,
                              uint32_t M, uint32_t N, uint32_t K,
                              uint32_t TM, uint32_t TN);

uint32_t compute_only_fp8_alt(const uint8_t *At8, const uint8_t *B8, int altfmt,
                              uint32_t M, uint32_t N, uint32_t K,
                              uint32_t TM, uint32_t TN);

// ─── Compute-only helpers (pure arithmetic, no VME instructions) ──────────────

static inline uint64_t compute_only_groups(uint32_t M, uint32_t N, uint32_t K,
                                           uint32_t TM, uint32_t TN, uint32_t tk)
{
    return (uint64_t)COMPUTE_ONLY_REPS
         * (uint64_t)(M / TM / 2)
         * (uint64_t)(N / TN / 2)
         * (uint64_t)(K / tk);
}

static inline uint64_t compute_only_flops(uint32_t M, uint32_t N, uint32_t K,
                                          uint32_t TM, uint32_t TN, uint32_t tk)
{
    // groups × 4 vtfmm/group × 2*TM*TN*TK FLOPs/vtfmm
    return compute_only_groups(M, N, K, TM, TN, tk)
         * 4ull * 2ull * (uint64_t)TM * (uint64_t)TN * (uint64_t)tk;
}

// ─── Format decoders (low-precision → fp32) ──────────────────────────────────

static inline float f16_to_f32(uint16_t h)
{
    uint32_t s = (uint32_t)(h >> 15) << 31;
    uint32_t e = (h >> 10) & 0x1F;
    uint32_t m = h & 0x3FF;
    uint32_t x;
    if (e == 0)    x = s | (m << 13);                  // subnormal / zero
    else if (e == 31) x = s | 0x7F800000u | (m << 13); // inf / NaN
    else           x = s | ((e + 127 - 15) << 23) | (m << 13);
    float f; memcpy(&f, &x, 4); return f;
}

static inline float bf16_to_f32(uint16_t h)
{
    uint32_t x = (uint32_t)h << 16;
    float f; memcpy(&f, &x, 4); return f;
}

static inline float e4m3_to_f32(uint8_t x)
{
    if ((x & 0x7Fu) == 0) return 0.0f;
    int s   = (x >> 7) & 1;
    int e   = (x >> 3) & 0xF;
    int m   = x & 0x7;
    if (e == 0) return 0.0f;
    // build fp32: biased_exp = e - 7 + 127, mantissa in bits [22:20]
    uint32_t bits = ((uint32_t)s << 31)
                  | ((uint32_t)(e - 7 + 127) << 23)
                  | ((uint32_t)m << 20);
    float f; memcpy(&f, &bits, 4); return f;
}

static inline float e5m2_to_f32(uint8_t x)
{
    if ((x & 0x7Fu) == 0) return 0.0f;
    int s = (x >> 7) & 1;
    int e = (x >> 2) & 0x1F;
    int m = x & 0x3;
    if (e == 0) return 0.0f;
    uint32_t bits = ((uint32_t)s << 31)
                  | ((uint32_t)(e - 15 + 127) << 23)
                  | ((uint32_t)m << 21);
    float f; memcpy(&f, &bits, 4); return f;
}

// ─── Scalar reference GEMM ───────────────────────────────────────────────────
// Takes At in column-major / transposed layout [K × M]: At[k*M+i] = A[i][k].
// This avoids needing a separate row-major A buffer alongside Cref in L1.

static void gemm_ref(float *C, const float *At, const float *B,
                     uint32_t M, uint32_t N, uint32_t K)
{
    for (uint32_t i = 0; i < M; i++)
        for (uint32_t j = 0; j < N; j++) {
            float acc = 0.0f;
            for (uint32_t k = 0; k < K; k++)
                acc += At[k*M+i] * B[k*N+j];
            C[i*N+j] = acc;
        }
}

// ─── Dataset generation ───────────────────────────────────────────────────────

typedef enum {
    DATASET_SMALL_POS = 0,  // uniform [0.01, 1.0]
    DATASET_SIGNED    = 1,  // uniform [-2.0, 2.0]
    DATASET_DYNAMIC   = 2,  // decade-scaled [1e-3, 1e2]
    DATASET_COUNT     = 3
} dataset_t;

typedef enum {
    FMT_F32  = 0,
    FMT_F16  = 1,
    FMT_BF16 = 2,
    FMT_E4M3 = 3,
    FMT_E5M2 = 4
} fmt_t;

static const char *dataset_name(dataset_t d)
{
    switch (d) {
        case DATASET_SMALL_POS: return "small positive [0.01, 1.0]";
        case DATASET_SIGNED:    return "signed random  [-2.0, 2.0]";
        case DATASET_DYNAMIC:   return "dynamic range  [1e-3, 1e2]";
        default:                return "unknown";
    }
}

static inline uint32_t lcg_next(uint32_t *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static inline float rand01(uint32_t *state)
{
    return (float)(lcg_next(state) & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}

static float dataset_sample(dataset_t dataset, uint32_t *state)
{
    float u = rand01(state);
    if (dataset == DATASET_SMALL_POS) return 0.01f + 0.99f * u;
    if (dataset == DATASET_SIGNED)    return -2.0f + 4.0f  * u;
    // DATASET_DYNAMIC: pick a decade scale then a mantissa in [1, 10]
    static const float scales[6] = {
        1.0e-3f, 1.0e-2f, 1.0e-1f, 1.0e0f, 1.0e1f, 1.0e2f
    };
    int decade = (int)((lcg_next(state) >> 24) % 6u);
    return scales[decade] * (1.0f + 9.0f * u);
}

// Initialise At_f32 and B with dataset-drawn values and fill all converted
// formats.  Row-major A is NOT written; callers use At_f32[k*M+i] for
// FMT_F32 access, which avoids a separate 16 KB row-major buffer in L1.
static void init_dataset_and_convert(dataset_t dataset,
                                     float *At_f32, float *B_f32,
                                     uint16_t *At_f16, uint16_t *B_f16,
                                     uint16_t *At_b16, uint16_t *B_b16,
                                     uint8_t  *At_e4,  uint8_t  *B_e4,
                                     uint8_t  *At_e5,  uint8_t  *B_e5,
                                     uint32_t M, uint32_t N, uint32_t K)
{
    uint32_t sa = 0x12345678u ^ ((uint32_t)dataset * 0x9E3779B9u);
    uint32_t sb = 0xA5A5A5A5u ^ ((uint32_t)dataset * 0x85EBCA6Bu);

    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t k = 0; k < K; k++) {
            float v = dataset_sample(dataset, &sa);
            At_f32[k*M+i] = v;
            At_f16[k*M+i] = f32_to_f16(v);
            At_b16[k*M+i] = f32_to_bf16(v);
            At_e4 [k*M+i] = f32_to_e4m3(v);
            At_e5 [k*M+i] = f32_to_e5m2(v);
        }
    }
    for (uint32_t k = 0; k < K; k++) {
        for (uint32_t j = 0; j < N; j++) {
            float v = dataset_sample(dataset, &sb);
            B_f32[k*N+j] = v;
            B_f16[k*N+j] = f32_to_f16(v);
            B_b16[k*N+j] = f32_to_bf16(v);
            B_e4 [k*N+j] = f32_to_e4m3(v);
            B_e5 [k*N+j] = f32_to_e5m2(v);
        }
    }
}

// ─── Error checkers ───────────────────────────────────────────────────────────

static inline float absf_l(float x) { return x < 0.0f ? -x : x; }

// Element-wise relative error check against a full reference matrix.
static void check_simple(const float *C, const float *Cref,
                         int *n_err, float *max_relerr, float rel_tol,
                         uint32_t M, uint32_t N)
{
    *n_err = 0; *max_relerr = 0.0f;
    for (uint32_t i = 0; i < M * N; i++) {
        float denom = Cref[i] > 1e-9f ? Cref[i] : (Cref[i] < -1e-9f ? -Cref[i] : 1e-9f);
        float rel   = absf_l(C[i] - Cref[i]) / denom;
        if (rel > *max_relerr) *max_relerr = rel;
        if (rel > rel_tol) (*n_err)++;
    }
}

// Format-aware dot product of row i, column j using decoded quantized inputs.
// FMT_F32 for A uses At_f32[k*M+i] (column-major), matching all other formats.
static float dot_ref_fmt(uint32_t i, uint32_t j, fmt_t fmt_a, fmt_t fmt_b,
                         const float    *At_f32, const float    *B_f32,
                         const uint16_t *At_f16, const uint16_t *B_f16,
                         const uint16_t *At_b16, const uint16_t *B_b16,
                         const uint8_t  *At_e4,  const uint8_t  *B_e4,
                         const uint8_t  *At_e5,  const uint8_t  *B_e5,
                         uint32_t M, uint32_t N, uint32_t K)
{
    float acc = 0.0f;
    for (uint32_t k = 0; k < K; k++) {
        float a, b;
        switch (fmt_a) {
            case FMT_F32:  a = At_f32[k*M+i];              break;
            case FMT_F16:  a = f16_to_f32(At_f16[k*M+i]);  break;
            case FMT_BF16: a = bf16_to_f32(At_b16[k*M+i]); break;
            case FMT_E4M3: a = e4m3_to_f32(At_e4[k*M+i]);  break;
            case FMT_E5M2: a = e5m2_to_f32(At_e5[k*M+i]);  break;
            default:       a = 0.0f;                        break;
        }
        switch (fmt_b) {
            case FMT_F32:  b = B_f32[k*N+j];               break;
            case FMT_F16:  b = f16_to_f32(B_f16[k*N+j]);   break;
            case FMT_BF16: b = bf16_to_f32(B_b16[k*N+j]);  break;
            case FMT_E4M3: b = e4m3_to_f32(B_e4[k*N+j]);   break;
            case FMT_E5M2: b = e5m2_to_f32(B_e5[k*N+j]);   break;
            default:       b = 0.0f;                        break;
        }
        acc += a * b;
    }
    return acc;
}

// Two-level error analysis:
//   map_*  : VME output C vs scalar reference computed with decoded quantized inputs
//            (measures instruction mapping correctness, independent of quantization noise)
//   quant_* : scalar reference with decoded inputs vs original fp32 reference
//            (measures quantization distortion of the format)
static void check_mapping_and_quant(
    const float *C, const float *Cref_f32,
    const float *At_f32, const float *B_f32,
    const uint16_t *At_f16, const uint16_t *B_f16,
    const uint16_t *At_b16, const uint16_t *B_b16,
    const uint8_t  *At_e4,  const uint8_t  *B_e4,
    const uint8_t  *At_e5,  const uint8_t  *B_e5,
    fmt_t fmt_a, fmt_t fmt_b,
    float map_rel_tol, float map_abs_tol,
    int *map_err,
    float *map_max_rel, float *map_max_abs,
    float *quant_max_rel, float *quant_max_abs,
    uint32_t M, uint32_t N, uint32_t K)
{
    *map_err = 0;
    *map_max_rel = *map_max_abs = *quant_max_rel = *quant_max_abs = 0.0f;

    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t j = 0; j < N; j++) {
            uint32_t idx = i*N + j;

            float ref_fmt = dot_ref_fmt(i, j, fmt_a, fmt_b,
                                        At_f32, B_f32,
                                        At_f16, B_f16,
                                        At_b16, B_b16,
                                        At_e4,  B_e4,
                                        At_e5,  B_e5,
                                        M, N, K);
            float ref_f32 = Cref_f32[idx];

            float map_abs = absf_l(C[idx] - ref_fmt);
            float map_den = absf_l(ref_fmt) > 1e-6f ? absf_l(ref_fmt) : 1e-6f;
            float map_rel = map_abs / map_den;
            if (map_abs > *map_max_abs) *map_max_abs = map_abs;
            if (map_rel > *map_max_rel) *map_max_rel = map_rel;
            if (map_abs > map_abs_tol && map_rel > map_rel_tol) (*map_err)++;

            float q_abs = absf_l(ref_fmt - ref_f32);
            float q_den = absf_l(ref_f32) > 1e-6f ? absf_l(ref_f32) : 1e-6f;
            float q_rel = q_abs / q_den;
            if (q_abs > *quant_max_abs) *quant_max_abs = q_abs;
            if (q_rel > *quant_max_rel) *quant_max_rel = q_rel;
        }
    }
}

// ─── Display helpers ──────────────────────────────────────────────────────────

static void print_part1_header(void)
{
    printf("\n%-22s  %8s  %12s  %8s  %12s  %10s  %7s  %s\n",
           "Format", "FullCyc", "FullFPC", "PeakCyc", "PeakFPC",
           "Full/Peak", "Errors", "Status");
    printf("%-22s  %8s  %12s  %8s  %12s  %10s  %7s  %s\n",
           "------", "-------", "-------", "-------", "-------",
           "---------", "------", "------");
}

static void print_part1_row(const char *label,
                            uint32_t full_cycles, uint32_t full_flops,
                            uint32_t peak_cycles, uint32_t tk,
                            int n_err,
                            uint32_t M, uint32_t N, uint32_t K,
                            uint32_t TM, uint32_t TN)
{
    uint64_t peak_flops = compute_only_flops(M, N, K, TM, TN, tk);
    double full_fpc = (double)full_flops / (double)full_cycles;
    double peak_fpc = (double)peak_flops / (double)peak_cycles;
    double util_pct = 100.0 * full_fpc / peak_fpc;
    printf("%-22s  %8lu  %12.2f  %8lu  %12.2f  %9.2f%%  %7d  %s\n",
           label,
           (unsigned long)full_cycles, full_fpc,
           (unsigned long)peak_cycles, peak_fpc,
           util_pct, n_err,
           n_err == 0 ? "PASS" : "FAIL");
}

static void print_part1_scalar(const char *label, uint32_t cycles, uint32_t flops)
{
    printf("%-22s  %8lu  %12.2f  %8s  %12s  %10s  %7s  %s\n",
           label, (unsigned long)cycles, (double)flops / (double)cycles,
           "-", "-", "-", "-", "-");
}

static void print_part2_header(void)
{
    printf("%-20s  %8s  %12s  %7s  %12s  %14s  %12s  %14s  %s\n",
           "Format", "Cycles", "FLOP/cyc", "MapErr",
           "MapRel", "MapAbs", "QRel", "QAbs", "Status");
    printf("%-20s  %8s  %12s  %7s  %12s  %14s  %12s  %14s  %s\n",
           "------", "------", "--------", "------",
           "------", "------", "----", "----", "------");
}

static void print_part2_row(const char *label, uint32_t cycles, uint32_t flops,
                            int map_err,
                            float map_max_rel, float map_max_abs,
                            float quant_max_rel, float quant_max_abs)
{
    printf("%-20s  %8lu  %12.2f  %7d  %12.6f  %14.6f  %12.6f  %14.6f  %s\n",
           label, (unsigned long)cycles, (double)flops / (double)cycles,
           map_err,
           (double)map_max_rel, (double)map_max_abs,
           (double)quant_max_rel, (double)quant_max_abs,
           map_err == 0 ? "PASS" : "CHECK");
}
