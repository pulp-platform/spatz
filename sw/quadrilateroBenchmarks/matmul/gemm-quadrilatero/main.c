// Copyright 2023 ETH Zurich and University of Bologna.
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

// Author: Ettore Rayneri, ETH Zurich

// Generalized matmul test: any M, N, K (also not multiples of the tile), any kernel shape, any datatype,
// normal or transposed layout. One binary per (kernel, datatype, M, N, K), from CMake:
//   KERNEL_H     kernel header (matmul_2x2_KxM_KxN.h, matmul_2x2_MxK_NxK.h, matmul_1x4_KxM_KxN.h, ...): compiled here as gemm_kernel,
//                so each binary holds only its kernel
//   DTA DTB DTC  datatypes for mmac.dt (__INT8__, __FP16ALT__, ...); C is 32 bit
//   WIDENING     log2(32 / bits of A and B): 0, 1, 2
//   GEMM_FLOAT   1 if C is FP32 (compare with tolerance), 0 if INT32 / UINT32 (exact)
//   TRANS        0: A^T (K x M), B (K x N) packed along K (pack_vertical), C (M x N)
//                1: A (M x K), Bt (N x K), Ct (N x M)
//   DATAHEADER   data/<DT>/data_M_N_K.h (TRANS=0) or data_t/<DT>/data_M_N_K.h (TRANS=1), from
//                scripts/gen_gemm_data.sh; gemm_C_dram is always C (M x N)

#include <benchmark.h>
#include <quadrilatero.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER

#if !defined(KERNEL_H) || !defined(DTA) || !defined(DTB) || !defined(DTC) || !defined(WIDENING) || \
    !defined(GEMM_FLOAT) || !defined(TRANS)
  #error "define KERNEL_H, DTA, DTB, DTC, WIDENING, GEMM_FLOAT, TRANS (see CMakeLists.txt)"
#endif

#define FUNC_NAME gemm_kernel
#include KERNEL_H
#undef FUNC_NAME

#if GEMM_FLOAT
typedef float c_t;
#define TOL_ABS 0.02f                // as in the fmatmul tests, plus a relative term for long K
#define TOL_REL 1e-4f
#else
typedef int32_t c_t;
#endif

void *a, *b;
c_t *c;

// Returns 0 or 1 if the result is correct or not, respectively. Prints the first error found.
int check_results(int M, int N) {
  const c_t *exp = (const c_t *)gemm_C_dram;           // C (M x N)
  for (int i = 0; i < M; i++)
    for (int j = 0; j < N; j++) {
#if TRANS
      c_t got = c[j * M + i];                           // Ct (N x M)
#else
      c_t got = c[i * N + j];
#endif
      c_t e = exp[i * N + j];
#if GEMM_FLOAT
      float d = got - e, ae = e < 0 ? -e : e;
      if (d < 0) d = -d;
      if (d > TOL_ABS + TOL_REL * ae) {
        printf("Error: C[%d][%d] = %d/1000, expected %d/1000\n", i, j, (int)(got * 1000), (int)(e * 1000));
        return 1;
      }
#else
      if (got != e) {
        printf("Error: C[%d][%d] = %d, expected %d\n", i, j, got, e);
        return 1;
      }
#endif
    }
  return 0;
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();
  const unsigned int measure_iterations = 2;
  const int M = gemm_l.M, N = gemm_l.N, K = gemm_l.K;
  unsigned int timer_start, timer_end, timer = (unsigned int)-1;

  if (cid == 0) {
    c = (c_t *)snrt_l1alloc(M * N * sizeof(c_t));
    a = snrt_l1alloc(sizeof(gemm_A_dram));
    b = (char *)snrt_l1alloc(sizeof(gemm_B_dram) + QUAD_RLEN / 8) + QUAD_RLEN / 8;   // +L words
    snrt_dma_start_1d(a, gemm_A_dram, sizeof(gemm_A_dram));
    snrt_dma_start_1d(b, gemm_B_dram, sizeof(gemm_B_dram));
    snrt_dma_wait_all();
  }
  snrt_cluster_hw_barrier();

  for (unsigned int it = 0; it < measure_iterations; ++it) {
    timer_start = benchmark_get_cycle();
    if (cid == 1) {
      start_kernel();
      gemm_kernel(a, b, c, K, N, M, WIDENING);
    }
    snrt_cluster_hw_barrier();
    if (cid == 1) stop_kernel();
    snrt_cluster_hw_barrier();
    timer_end = benchmark_get_cycle();
    if (cid == 0 && timer_end - timer_start < timer) timer = timer_end - timer_start;
  }

  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * M * N * K / timer;
    long unsigned int utilization = performance / (2 * QUAD_RLEN/32 * QUAD_RLEN/32 * (1 << WIDENING));
    printf("\n----- (%dx%dx%d) gemm %s, dt %d %d -> %d, TRANS=%d -----\n", M, K, N, KERNEL_H, DTA, DTB, DTC, TRANS);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n", performance, utilization);
    int err = check_results(M, N);
    if (err) return err;
  }
  snrt_cluster_hw_barrier();
  return 0;
}
