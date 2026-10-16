// Copyright 2026 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0

#include <printf.h>
#include <snrt.h>

#include "gemm.h"
#include DATAHEADER
#include "kernel/gemm.c"

static inline uint32_t get_cycle(void) {
    uint32_t cycle;
    asm volatile("csrr %0, mcycle" : "=r"(cycle) : : "memory");
    return cycle;
}

static inline uint32_t float_bits(float value) {
    union { float f; uint32_t u; } bits = {.f = value};
    return bits.u;
}

int main(void) {
    if (snrt_cluster_core_idx() != 0)
        return 0;

    if (gemm_l.M != 64 || gemm_l.N != 64 ||
        (gemm_l.K != 128 && gemm_l.K != 256 && gemm_l.K != 512)) {
        printf("Unsupported GEMM shape: expected M64_N64_K128, K256, or K512\n");
        return 2;
    }

    enum {
        A_BYTES = sizeof(Atpack_dram),
        B_BYTES = sizeof(Bpack_dram),
        C_BYTES = sizeof(Caddend_dram),
        SCALER_BYTES = sizeof(scaler_dram),
        BUFFER_BYTES = A_BYTES + B_BYTES + C_BYTES + SCALER_BYTES
    };
    _Static_assert(BUFFER_BYTES <= SNRT_TCDM_SIZE, "GEMM buffers exceed TCDM");

    uint8_t *const buffer = (uint8_t *)snrt_l1alloc(BUFFER_BYTES);
    if (buffer == 0)
        return 2;

    __fp16 *const Atpack = (__fp16 *)buffer;
    __fp16 *const Bpack = (__fp16 *)(buffer + A_BYTES);
    float *const C = (float *)(buffer + A_BYTES + B_BYTES);
    float *const scaler = (float *)(buffer + A_BYTES + B_BYTES + C_BYTES);

    snrt_dma_start_1d(Atpack, Atpack_dram, A_BYTES);
    snrt_dma_start_1d(Bpack, Bpack_dram, B_BYTES);
    snrt_dma_start_1d(C, Caddend_dram, C_BYTES);
    snrt_dma_start_1d(scaler, &scaler_dram, SCALER_BYTES);
    snrt_dma_wait_all();

    const uint32_t start = get_cycle();
    gemm_fp16_fp32_alpha_vle(Atpack, Bpack, C, scaler);
    const uint32_t cycles = get_cycle() - start;

    uint32_t errors = 0;
    float max_abs = 0.0f;
    for (uint32_t row = 0; row < gemm_l.M; ++row) {
        uint32_t row_errors = 0;
        for (uint32_t col = 0; col < gemm_l.N; ++col) {
            const uint32_t index = row * gemm_l.N + col;
            const float expected = Cref_dram[index];
            const float delta = C[index] > expected ? C[index] - expected
                                                     : expected - C[index];
            const float tolerance = 0.001f + 0.001f *
                (expected < 0.0f ? -expected : expected);
            if (delta > max_abs)
                max_abs = delta;
            if (!(delta <= tolerance)) {
                if (row_errors == 0)
                    printf("row %u: errors at cols %u", row, col);
                else
                    printf(",%u", col);
                ++row_errors;
                ++errors;
            }
        }
        if (row_errors != 0)
            printf(" (%u errors)\n", row_errors);
    }

    const uint64_t useful_macs =
        (uint64_t)gemm_l.M * gemm_l.N * gemm_l.K;
    const uint64_t peak_macs = (uint64_t)cycles * CE * CE * 2;
    const uint32_t utilization =
        peak_macs ? useful_macs * 10000 / peak_macs : 0;
    printf("\n=== TEW32 FP16 scaled GEMM M%u_N%u_K%u "
           "(TE=16, 1 core) ===\n",
           gemm_l.M, gemm_l.N, gemm_l.K);
    printf("Scaler: 0x%08x\n", float_bits(*scaler));
    printf("Cycles: %u\nUtilization: %u.%02u%%\n",
           cycles, utilization / 100, utilization % 100);
    printf("MaxAbsErr: 0x%08x\nErrors: %u\nStatus: %s\n",
           float_bits(max_abs), errors, errors ? "FAIL" : "PASS");
    return errors ? 1 : 0;
}
