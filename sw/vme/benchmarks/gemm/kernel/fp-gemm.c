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

// fp-gemm.c

#include "fp-gemm.h"
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// ─── Format converters ───────────────────────────────────────────────────────

static uint16_t f32_to_f16(float f)
{
    uint32_t x; memcpy(&x, &f, 4);
    uint32_t s = x >> 31;
    uint32_t e = (x >> 23) & 0xFF;
    uint32_t m = x & 0x7FFFFF;

    if (e == 0)    return (uint16_t)(s << 15);
    if (e == 0xFF) return (uint16_t)((s << 15) | 0x7C00 | (m ? 0x200 : 0));

    int32_t e16 = (int32_t)e - 127 + 15;
    if (e16 <= 0)  return (uint16_t)(s << 15);
    if (e16 >= 31) return (uint16_t)((s << 15) | 0x7C00);

    uint32_t m_rounded = m + 0x1000;   // add 0.5 ULP for round-to-nearest
    uint32_t m16 = m_rounded >> 13;
    return (uint16_t)((s << 15) | (e16 << 10) | (m16 & 0x3FF));
}

static uint16_t f32_to_bf16(float f)
{
    uint32_t x;
    memcpy(&x, &f, 4);
    uint32_t round = 0x7FFF + ((x >> 16) & 1);
    x += round;
    return (uint16_t)(x >> 16);
}

static uint8_t f32_to_e4m3(float f)
{
    uint32_t x; memcpy(&x, &f, 4);
    uint32_t s = x >> 31;
    int32_t  e = ((x >> 23) & 0xFF) - 127 + 7;
    uint32_t m_full = (x >> 19) & 0xF;
    uint32_t m = (m_full >> 1) + (m_full & 1);
    if (m == 8) { m = 0; e += 1; }
    if (e <= 0)  return 0;
    if (e >= 15) { e = 15; m = 6; }
    return (uint8_t)((s << 7) | (e << 3) | m);
}

static uint8_t f32_to_e5m2(float f)
{
    uint32_t x; memcpy(&x, &f, 4);
    uint32_t s = x >> 31;
    int32_t  e = ((x >> 23) & 0xFF) - 127 + 15;
    uint32_t m_full = (x >> 20) & 0x7;
    uint32_t m = m_full >> 1;
    uint32_t r = m_full & 1;
    m += r;
    if (m == 4) { m = 0; e += 1; }
    if (e <= 0)  return 0;
    if (e >= 30) { e = 30; m = 3; }
    return (uint8_t)((s << 7) | (e << 2) | m);
}

// ─── Tile-store helper (FP32, 2×2 block) ─────────────────────────────────────
// Writes mt0/mt4/mt8/mt12 → C[c00], C[c01], C[c10], C[c11] (row-major).
// Uses runtime TM/N passed via enclosing function scope.

#define STORE_4TILES(c00, c01, c10, c11) do {                                  \
    for (int _r = 0; _r < (int)TM; _r++) {                                     \
        uintptr_t _t0  = TSS_ROW( 0,_r), _t4  = TSS_ROW( 4,_r);               \
        uintptr_t _t8  = TSS_ROW( 8,_r), _t12 = TSS_ROW(12,_r);               \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t0),  "r"((c00)+_r*N) : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t4),  "r"((c01)+_r*N) : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t8),  "r"((c10)+_r*N) : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t12), "r"((c11)+_r*N) : "memory"); \
    }                                                                            \
} while(0)

// ─── FP8 inner-loop helper (SEW=e8, TK=4, row_stride=2) ─────────────────────
// Loads v8–v15 for A (ti=0,1 × k=0..3) and v16–v23 for B (tj=0,1 × k=0..3).
// References loop vars ti, tj and function params K, M, N, TM, TN.

#define INNER_LOOP_FP8(At8, B8, INSN) do {                                     \
    for (int _k = 0; _k < (int)K; _k += 4) {                                   \
        const uint8_t *_a0 = (At8) + _k*(int)M + ti*(int)TM;                   \
        const uint8_t *_a1 = (At8) + _k*(int)M + (ti+1)*(int)TM;              \
        const uint8_t *_b0 = (B8)  + _k*(int)N + tj*(int)TN;                   \
        const uint8_t *_b1 = (B8)  + _k*(int)N + (tj+1)*(int)TN;              \
        asm volatile("vle8.v v8,  (%0)" :: "r"(_a0          ) : "v8",  "memory"); \
        asm volatile("vle8.v v10, (%0)" :: "r"(_a0+(int)M   ) : "v10", "memory"); \
        asm volatile("vle8.v v12, (%0)" :: "r"(_a0+2*(int)M ) : "v12", "memory"); \
        asm volatile("vle8.v v14, (%0)" :: "r"(_a0+3*(int)M ) : "v14", "memory"); \
        asm volatile("vle8.v v9,  (%0)" :: "r"(_a1          ) : "v9",  "memory"); \
        asm volatile("vle8.v v11, (%0)" :: "r"(_a1+(int)M   ) : "v11", "memory"); \
        asm volatile("vle8.v v13, (%0)" :: "r"(_a1+2*(int)M ) : "v13", "memory"); \
        asm volatile("vle8.v v15, (%0)" :: "r"(_a1+3*(int)M ) : "v15", "memory"); \
        asm volatile("vle8.v v16, (%0)" :: "r"(_b0          ) : "v16", "memory"); \
        asm volatile("vle8.v v18, (%0)" :: "r"(_b0+(int)N   ) : "v18", "memory"); \
        asm volatile("vle8.v v20, (%0)" :: "r"(_b0+2*(int)N ) : "v20", "memory"); \
        asm volatile("vle8.v v22, (%0)" :: "r"(_b0+3*(int)N ) : "v22", "memory"); \
        asm volatile("vle8.v v17, (%0)" :: "r"(_b1          ) : "v17", "memory"); \
        asm volatile("vle8.v v19, (%0)" :: "r"(_b1+(int)N   ) : "v19", "memory"); \
        asm volatile("vle8.v v21, (%0)" :: "r"(_b1+2*(int)N ) : "v21", "memory"); \
        asm volatile("vle8.v v23, (%0)" :: "r"(_b1+3*(int)N ) : "v23", "memory"); \
        asm volatile(INSN " mt0,  v8, v16"  ::: "memory");                     \
        asm volatile(INSN " mt4,  v8, v17"  ::: "memory");                     \
        asm volatile(INSN " mt8,  v9, v16"  ::: "memory");                     \
        asm volatile(INSN " mt12, v9, v17"  ::: "memory");                     \
    }                                                                            \
} while(0)

#define VLSE32(dst, base, stride_bytes) \
    asm volatile("vlse32.v " dst ", (%0), %1" :: "r"(base), "r"(stride_bytes) : dst, "memory")

#define VLSE16(dst, base, stride_bytes) \
    asm volatile("vlse16.v " dst ", (%0), %1" :: "r"(base), "r"(stride_bytes) : dst, "memory")

#define VLSE8(dst, base, stride_bytes) \
    asm volatile("vlse8.v " dst ", (%0), %1" :: "r"(base), "r"(stride_bytes) : dst, "memory")

// ─── 1. FP32 row-major-A stride-load kernel ──────────────────────────────────
// A_row layout: A_row[i*K+k] = A[i][k]
// B layout:     B[k*N+j]     = B[k][j]
void gemm_fp32(float *C, const float *A_row, const float *B,
                       uint32_t M, uint32_t N, uint32_t K,
                       uint32_t TM, uint32_t TN)
{
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma" :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e32(TN, TM, 1);
    clear_altfmt();

    const uintptr_t a_stride = (uintptr_t)(K * sizeof(float));

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            asm volatile("vtzero mt0"  ::: "memory");
            asm volatile("vtzero mt4"  ::: "memory");
            asm volatile("vtzero mt8"  ::: "memory");
            asm volatile("vtzero mt12" ::: "memory");

            for (int k = 0; k < (int)K; k++) {
                const float *a0 = A_row + (ti    ) * (int)TM * (int)K + k;
                const float *a1 = A_row + (ti + 1) * (int)TM * (int)K + k;
                const float *b0 = B     + k * (int)N + tj * (int)TN;
                const float *b1 = B     + k * (int)N + (tj + 1) * (int)TN;

                VLSE32("v8",  a0, a_stride);
                VLSE32("v9",  a1, a_stride);
                asm volatile("vle32.v v16, (%0)" :: "r"(b0) : "v16", "memory");
                asm volatile("vle32.v v17, (%0)" :: "r"(b1) : "v17", "memory");

                asm volatile("vtfmm.tvv mt0,  v8, v16"  ::: "memory");
                asm volatile("vtfmm.tvv mt4,  v8, v17"  ::: "memory");
                asm volatile("vtfmm.tvv mt8,  v9, v16"  ::: "memory");
                asm volatile("vtfmm.tvv mt12, v9, v17"  ::: "memory");
            }

            STORE_4TILES(C + ti*(int)TM*(int)N + tj*(int)TN,
                                C + ti*(int)TM*(int)N + (tj+1)*(int)TN,
                                C + (ti+1)*(int)TM*(int)N + tj*(int)TN,
                                C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN);
        }
    }

    asm volatile("vtdiscard" ::: "memory");
}

// ─── 2. FP16 row-major-A stride-load kernel ──────────────────────────────────
void gemm_fp16(float *C, const uint16_t *A_row_fp16, const uint16_t *B_fp16,
                       uint32_t M, uint32_t N, uint32_t K,
                       uint32_t TM, uint32_t TN)
{
    asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e16(TN, TM, 2);
    clear_altfmt();

    const uintptr_t a_stride = (uintptr_t)(K * sizeof(uint16_t));

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            asm volatile("vtzero mt0"  ::: "memory");
            asm volatile("vtzero mt4"  ::: "memory");
            asm volatile("vtzero mt8"  ::: "memory");
            asm volatile("vtzero mt12" ::: "memory");

            for (int k = 0; k < (int)K; k += 2) {
                const uint16_t *a0k0 = A_row_fp16 + (ti    )*(int)TM*(int)K + k;
                const uint16_t *a1k0 = A_row_fp16 + (ti + 1)*(int)TM*(int)K + k;
                const uint16_t *b0k0 = B_fp16     + k*(int)N + tj*(int)TN;
                const uint16_t *b1k0 = B_fp16     + k*(int)N + (tj+1)*(int)TN;

                VLSE16("v8",  a0k0,     a_stride);
                VLSE16("v12", a0k0 + 1, a_stride);
                VLSE16("v9",  a1k0,     a_stride);
                VLSE16("v13", a1k0 + 1, a_stride);

                asm volatile("vle16.v v16, (%0)" :: "r"(b0k0)          : "v16", "memory");
                asm volatile("vle16.v v20, (%0)" :: "r"(b0k0 + (int)N) : "v20", "memory");
                asm volatile("vle16.v v17, (%0)" :: "r"(b1k0)          : "v17", "memory");
                asm volatile("vle16.v v21, (%0)" :: "r"(b1k0 + (int)N) : "v21", "memory");

                asm volatile("vtfmm.tvv mt0,  v8, v16"  ::: "memory");
                asm volatile("vtfmm.tvv mt4,  v8, v17"  ::: "memory");
                asm volatile("vtfmm.tvv mt8,  v9, v16"  ::: "memory");
                asm volatile("vtfmm.tvv mt12, v9, v17"  ::: "memory");
            }

            STORE_4TILES(C + ti*(int)TM*(int)N + tj*(int)TN,
                                C + ti*(int)TM*(int)N + (tj+1)*(int)TN,
                                C + (ti+1)*(int)TM*(int)N + tj*(int)TN,
                                C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN);
        }
    }

    asm volatile("vtdiscard" ::: "memory");
}

// ─── 3. BF16 row-major-A stride-load kernel ──────────────────────────────────
void gemm_bf16(float *C, const uint16_t *A_row_bf16, const uint16_t *B_bf16,
                       uint32_t M, uint32_t N, uint32_t K,
                       uint32_t TM, uint32_t TN)
{
    asm volatile("vsetvli zero, %0, e16, m1, ta, ma" :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e16(TN, TM, 2);
    set_altfmt();

    const uintptr_t a_stride = (uintptr_t)(K * sizeof(uint16_t));

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            asm volatile("vtzero mt0"  ::: "memory");
            asm volatile("vtzero mt4"  ::: "memory");
            asm volatile("vtzero mt8"  ::: "memory");
            asm volatile("vtzero mt12" ::: "memory");

            for (int k = 0; k < (int)K; k += 2) {
                const uint16_t *a0k0 = A_row_bf16 + (ti    )*(int)TM*(int)K + k;
                const uint16_t *a1k0 = A_row_bf16 + (ti + 1)*(int)TM*(int)K + k;
                const uint16_t *b0k0 = B_bf16     + k*(int)N + tj*(int)TN;
                const uint16_t *b1k0 = B_bf16     + k*(int)N + (tj+1)*(int)TN;

                VLSE16("v8",  a0k0,     a_stride);
                VLSE16("v12", a0k0 + 1, a_stride);
                VLSE16("v9",  a1k0,     a_stride);
                VLSE16("v13", a1k0 + 1, a_stride);

                asm volatile("vle16.v v16, (%0)" :: "r"(b0k0)          : "v16", "memory");
                asm volatile("vle16.v v20, (%0)" :: "r"(b0k0 + (int)N) : "v20", "memory");
                asm volatile("vle16.v v17, (%0)" :: "r"(b1k0)          : "v17", "memory");
                asm volatile("vle16.v v21, (%0)" :: "r"(b1k0 + (int)N) : "v21", "memory");

                asm volatile("vtfmm.alt.tvv mt0,  v8, v16"  ::: "memory");
                asm volatile("vtfmm.alt.tvv mt4,  v8, v17"  ::: "memory");
                asm volatile("vtfmm.alt.tvv mt8,  v9, v16"  ::: "memory");
                asm volatile("vtfmm.alt.tvv mt12, v9, v17"  ::: "memory");
            }

            STORE_4TILES(C + ti*(int)TM*(int)N + tj*(int)TN,
                        C + ti*(int)TM*(int)N + (tj+1)*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + tj*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN);
        }
    }

    clear_altfmt();
    asm volatile("vtdiscard" ::: "memory");
}

#define INNER_LOOP_FP8(A8, B8, INSN) do {                              \
    const uintptr_t _a_stride = (uintptr_t)(K * sizeof(uint8_t));              \
    for (int _k = 0; _k < (int)K; _k += 4) {                                   \
        const uint8_t *_a0 = (A8) + ti*(int)TM*(int)K + _k;                    \
        const uint8_t *_a1 = (A8) + (ti+1)*(int)TM*(int)K + _k;                \
        const uint8_t *_b0 = (B8) + _k*(int)N + tj*(int)TN;                    \
        const uint8_t *_b1 = (B8) + _k*(int)N + (tj+1)*(int)TN;                \
        VLSE8("v8",  _a0,     _a_stride);                                     \
        VLSE8("v10", _a0 + 1, _a_stride);                                     \
        VLSE8("v12", _a0 + 2, _a_stride);                                     \
        VLSE8("v14", _a0 + 3, _a_stride);                                     \
        VLSE8("v9",  _a1,     _a_stride);                                     \
        VLSE8("v11", _a1 + 1, _a_stride);                                     \
        VLSE8("v13", _a1 + 2, _a_stride);                                     \
        VLSE8("v15", _a1 + 3, _a_stride);                                     \
        asm volatile("vle8.v v16, (%0)" :: "r"(_b0          ) : "v16", "memory"); \
        asm volatile("vle8.v v18, (%0)" :: "r"(_b0+(int)N   ) : "v18", "memory"); \
        asm volatile("vle8.v v20, (%0)" :: "r"(_b0+2*(int)N ) : "v20", "memory"); \
        asm volatile("vle8.v v22, (%0)" :: "r"(_b0+3*(int)N ) : "v22", "memory"); \
        asm volatile("vle8.v v17, (%0)" :: "r"(_b1          ) : "v17", "memory"); \
        asm volatile("vle8.v v19, (%0)" :: "r"(_b1+(int)N   ) : "v19", "memory"); \
        asm volatile("vle8.v v21, (%0)" :: "r"(_b1+2*(int)N ) : "v21", "memory"); \
        asm volatile("vle8.v v23, (%0)" :: "r"(_b1+3*(int)N ) : "v23", "memory"); \
        asm volatile(INSN " mt0,  v8, v16"  ::: "memory");                    \
        asm volatile(INSN " mt4,  v8, v17"  ::: "memory");                    \
        asm volatile(INSN " mt8,  v9, v16"  ::: "memory");                    \
        asm volatile(INSN " mt12, v9, v17"  ::: "memory");                    \
    }                                                                           \
} while (0)

// ─── 4. E4M3×E4M3 row-major-A stride-load kernel ─────────────────────────────
void gemm_e4m3_e4m3(float *C, const uint8_t *A_row_e4m3, const uint8_t *B_e4m3,
                            uint32_t M, uint32_t N, uint32_t K,
                            uint32_t TM, uint32_t TN)
{
    asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e8(TN, TM, 4);
    clear_altfmt();

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            asm volatile("vtzero mt0"  ::: "memory");
            asm volatile("vtzero mt4"  ::: "memory");
            asm volatile("vtzero mt8"  ::: "memory");
            asm volatile("vtzero mt12" ::: "memory");
            INNER_LOOP_FP8(A_row_e4m3, B_e4m3, "vtfmm.tvv");
            STORE_4TILES(C + ti*(int)TM*(int)N + tj*(int)TN,
                        C + ti*(int)TM*(int)N + (tj+1)*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + tj*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN);
        }
    }

    asm volatile("vtdiscard" ::: "memory");
}

// ─── 5. E4M3×E5M2 row-major-A stride-load kernel ─────────────────────────────
void gemm_e4m3_e5m2(float *C, const uint8_t *A_row_e4m3, const uint8_t *B_e5m2,
                            uint32_t M, uint32_t N, uint32_t K,
                            uint32_t TM, uint32_t TN)
{
    asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e8(TN, TM, 4);
    set_altfmt();

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            asm volatile("vtzero mt0"  ::: "memory");
            asm volatile("vtzero mt4"  ::: "memory");
            asm volatile("vtzero mt8"  ::: "memory");
            asm volatile("vtzero mt12" ::: "memory");
            INNER_LOOP_FP8(A_row_e4m3, B_e5m2, "vtfmm.tvv");
            STORE_4TILES(C + ti*(int)TM*(int)N + tj*(int)TN,
                        C + ti*(int)TM*(int)N + (tj+1)*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + tj*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN);
        }
    }

    clear_altfmt();
    asm volatile("vtdiscard" ::: "memory");
}

// ─── 6. E5M2×E4M3 row-major-A stride-load kernel ─────────────────────────────
void gemm_e5m2_e4m3(float *C, const uint8_t *A_row_e5m2, const uint8_t *B_e4m3,
                            uint32_t M, uint32_t N, uint32_t K,
                            uint32_t TM, uint32_t TN)
{
    asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e8(TN, TM, 4);
    clear_altfmt();

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            asm volatile("vtzero mt0"  ::: "memory");
            asm volatile("vtzero mt4"  ::: "memory");
            asm volatile("vtzero mt8"  ::: "memory");
            asm volatile("vtzero mt12" ::: "memory");
            INNER_LOOP_FP8(A_row_e5m2, B_e4m3, "vtfmm.alt.tvv");
            STORE_4TILES(C + ti*(int)TM*(int)N + tj*(int)TN,
                        C + ti*(int)TM*(int)N + (tj+1)*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + tj*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN);
        }
    }

    clear_altfmt();
    asm volatile("vtdiscard" ::: "memory");
}

// ─── 7. E5M2×E5M2 row-major-A stride-load kernel ─────────────────────────────
void gemm_e5m2_e5m2(float *C, const uint8_t *A_row_e5m2, const uint8_t *B_e5m2,
                            uint32_t M, uint32_t N, uint32_t K,
                            uint32_t TM, uint32_t TN)
{
    asm volatile("vsetvli zero, %0, e8, m1, ta, ma" :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e8(TN, TM, 4);
    set_altfmt();

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            asm volatile("vtzero mt0"  ::: "memory");
            asm volatile("vtzero mt4"  ::: "memory");
            asm volatile("vtzero mt8"  ::: "memory");
            asm volatile("vtzero mt12" ::: "memory");
            INNER_LOOP_FP8(A_row_e5m2, B_e5m2, "vtfmm.alt.tvv");
            STORE_4TILES(C + ti*(int)TM*(int)N + tj*(int)TN,
                        C + ti*(int)TM*(int)N + (tj+1)*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + tj*(int)TN,
                        C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN);
        }
    }

    clear_altfmt();
    asm volatile("vtdiscard" ::: "memory");
}

#undef INNER_LOOP_FP8
#undef VLSE8
#undef VLSE16
#undef VLSE32
#undef STORE_4TILES