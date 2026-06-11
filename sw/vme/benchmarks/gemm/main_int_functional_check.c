// SPDX-License-Identifier: Apache-2.0
//
// main_int_matmul_check.c
//
// Functional checker for integer VME GEMM kernels:
//   vtmmu: uint8_t x uint8_t -> int32_t C
//   vtmms: int8_t  x int8_t  -> int32_t C
//
// A is always loaded with RVV stride-load vlse8.v inside the kernel.

#include <snrt.h>
#include <stdint.h>
#include <string.h>
#include <printf.h>

#include DATAHEADER
#include "kernel/int-gemm.c"

#ifndef PRINT_C_MATRIX
#define PRINT_C_MATRIX 0
#endif

static void print_matrix_i32(const char *name,
                             const int32_t *X,
                             uint32_t rows,
                             uint32_t cols)
{
    printf("\n%s = [\n", name);
    for (uint32_t i = 0; i < rows; i++) {
        printf("  ");
        for (uint32_t j = 0; j < cols; j++) {
            printf("%ld", (long)X[i * cols + j]);
            if (j + 1 < cols) printf(", ");
        }
        if (i + 1 < rows) printf(";\n");
        else             printf("\n");
    }
    printf("]\n");
}

static int check_i32(const int32_t *C,
                     const int32_t *Cref,
                     uint32_t M,
                     uint32_t N,
                     int32_t *max_abs_err,
                     uint32_t *max_idx)
{
    int n_err = 0;
    *max_abs_err = 0;
    *max_idx = 0;

    for (uint32_t idx = 0; idx < M * N; idx++) {
        int32_t diff = C[idx] - Cref[idx];
        int32_t abs_err = diff < 0 ? -diff : diff;

        if (abs_err > *max_abs_err) {
            *max_abs_err = abs_err;
            *max_idx = idx;
        }

        if (diff != 0) n_err++;
    }

    return n_err;
}

static void print_header(void)
{
    printf("\n=== Integer VME GEMM functional check ===\n");
    printf("%-10s %8s %10s %10s %12s %10s %10s %8s\n",
           "Instr", "altfmt", "CInit", "Errors", "MaxAbsErr",
           "WorstI", "WorstJ", "Status");
    printf("%-10s %8s %10s %10s %12s %10s %10s %8s\n",
           "-----", "------", "-----", "------", "---------",
           "------", "------", "------");
}

static void print_row(const char *instr,
                      int altfmt,
                      const char *cinit,
                      int errors,
                      int32_t max_abs_err,
                      uint32_t worst_i,
                      uint32_t worst_j)
{
    printf("%-10s %8d %10s %10d %12ld %10u %10u %8s\n",
           instr,
           altfmt,
           cinit,
           errors,
           (long)max_abs_err,
           (unsigned)worst_i,
           (unsigned)worst_j,
           errors == 0 ? "PASS" : "FAIL");
}

int main(void)
{
    if (snrt_cluster_core_idx() != 0) {
        snrt_cluster_hw_barrier();
        return 0;
    }

    enable_vec();
    enable_vme();

    const uint32_t M  = gemm_l.M;
    const uint32_t N  = gemm_l.N;
    const uint32_t K  = gemm_l.K;
    const uint32_t TM = gemm_l.TM;
    const uint32_t TN = gemm_l.TN;

    if (M != 64 || N != 64 || K != 64 || TM != 16 || TN != 16) {
        printf("ERROR: expected M=N=K=64, TM=TN=16.\n");
        printf("Got M=%u N=%u K=%u TM=%u TN=%u\n",
               (unsigned)M, (unsigned)N, (unsigned)K,
               (unsigned)TM, (unsigned)TN);
        snrt_cluster_hw_barrier();
        return 1;
    }

    uint8_t *A_u8 = (uint8_t *)snrt_l1alloc(M * K * sizeof(uint8_t));
    uint8_t *B_u8 = (uint8_t *)snrt_l1alloc(K * N * sizeof(uint8_t));
    int8_t  *A_s8 = (int8_t  *)snrt_l1alloc(M * K * sizeof(int8_t));
    int8_t  *B_s8 = (int8_t  *)snrt_l1alloc(K * N * sizeof(int8_t));
    int32_t *C    = (int32_t *)snrt_l1alloc(M * N * sizeof(int32_t));

    if (!A_u8 || !B_u8 || !A_s8 || !B_s8 || !C) {
        printf("ERROR: snrt_l1alloc failed.\n");
        snrt_cluster_hw_barrier();
        return 1;
    }

    snrt_dma_start_1d(A_u8, gemm_A_u8_dram, M * K * sizeof(uint8_t));
    snrt_dma_start_1d(B_u8, gemm_B_u8_dram, K * N * sizeof(uint8_t));
    snrt_dma_start_1d(A_s8, gemm_A_s8_dram, M * K * sizeof(int8_t));
    snrt_dma_start_1d(B_s8, gemm_B_s8_dram, K * N * sizeof(int8_t));
    snrt_dma_wait_all();

    printf("\n=== DATAHEADER integer GEMM ===\n");
    printf("M=%u N=%u K=%u TM=%u TN=%u\n",
           (unsigned)M, (unsigned)N, (unsigned)K,
           (unsigned)TM, (unsigned)TN);
    printf("A-load policy: ALWAYS vlse8.v stride-load from row-major A[M x K]\n");
    printf("B-load policy: contiguous vle8.v from row-major B[K x N]\n");
    printf("PRINT_C_MATRIX=%d\n", PRINT_C_MATRIX);

    print_header();

    int total_err = 0;
    int32_t max_abs_err = 0;
    uint32_t max_idx = 0;
    int err = 0;

#define RUN_CHECK(INSTR, ALTFMT, CINIT_NAME, KERNEL_CALL, REF) do {            \
        memset(C, 0, M * N * sizeof(int32_t));                                  \
        KERNEL_CALL;                                                            \
        err = check_i32(C, (REF), M, N, &max_abs_err, &max_idx);                \
        total_err += err;                                                       \
        print_row((INSTR), (ALTFMT), (CINIT_NAME), err, max_abs_err,            \
                  max_idx / N, max_idx % N);                                    \
        if (PRINT_C_MATRIX) print_matrix_i32((INSTR), C, M, N);                 \
    } while (0)

    RUN_CHECK("vtmmu", 0, "vtzero",
              gemm_vtmmu(C, A_u8, B_u8, M, N, K, TM, TN, 0, 0),
              gemm_C_vtmmu_u8u8_ref);

    RUN_CHECK("vtmmu", 0, "vtle",
              gemm_vtmmu(C, A_u8, B_u8, M, N, K, TM, TN, 0, 1),
              gemm_C_vtmmu_u8u8_ref);

    RUN_CHECK("vtmmu", 1, "vtzero",
              gemm_vtmmu(C, A_u8, B_s8, M, N, K, TM, TN, 1, 0),
              gemm_C_vtmmu_u8s8_ref);

    RUN_CHECK("vtmmu", 1, "vtle",
              gemm_vtmmu(C, A_u8, B_s8, M, N, K, TM, TN, 1, 1),
              gemm_C_vtmmu_u8s8_ref);

    RUN_CHECK("vtmms", 0, "vtzero",
              gemm_vtmms(C, A_s8, B_u8, M, N, K, TM, TN, 0, 0),
              gemm_C_vtmms_s8u8_ref);

    RUN_CHECK("vtmms", 0, "vtle",
              gemm_vtmms(C, A_s8, B_u8, M, N, K, TM, TN, 0, 1),
              gemm_C_vtmms_s8u8_ref);

    RUN_CHECK("vtmms", 1, "vtzero",
              gemm_vtmms(C, A_s8, B_s8, M, N, K, TM, TN, 1, 0),
              gemm_C_vtmms_s8s8_ref);

    RUN_CHECK("vtmms", 1, "vtle",
              gemm_vtmms(C, A_s8, B_s8, M, N, K, TM, TN, 1, 1),
              gemm_C_vtmms_s8s8_ref);

#undef RUN_CHECK

    printf("\nOverall Status: %s\n", total_err == 0 ? "PASS" : "FAIL");
    printf("Total integer GEMM errors: %d\n", total_err);

    snrt_cluster_hw_barrier();
    return total_err == 0 ? 0 : 1;
}