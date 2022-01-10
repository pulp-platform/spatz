// Copyright 2021 ETH Zurich and University of Bologna.
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

// Author: Domenic Wüthrich, ETH Zurich

#include <stdint.h>
#include <string.h>

#include "kernel/matmul.c"
#include "printf.h"
#ifdef MEMPOOL
#include "runtime.h"
#include "synchronization.h"
#endif

// Define Matrix dimensions:
// C = AB with A=[MxN], B=[NxP], C=[MxP]
#define M 64
#define N 64
#define P 64
// Specify how the matrices A and B should be initialized
// The entries will follow this format:
// a(i,j) = A_a*i + A_b*j + A_c
// b(i,j) = B_a*i + B_b*j + B_c
// The result will be the following matrix
// c(i,j) = (A_a*B_b*i*j + A_a*B_c*i + A_c*B_b*j + A_c*B_c) * N
//        + (A_a*B_a*i + A_b*B_b*j + A_b*B_c + B_a*A_c) * (N*(N-1))/2
//        + (A_b*B_a) * (N*(N-1)*(2*N-1))/6
// Note: To keep the code simpler, we use indices that go from 0 to N-1 instead
// of 1 to N as the mathematicians do. Hence, for A, i=[0,M-1] j=[0,M-1]
#define A_a 1
#define A_b 1
#define A_c -32
#define B_a 2
#define B_b 1
#define B_c 16

int32_t a[M * N] __attribute__((aligned(32), section(".l1_prio")));
int32_t b[N * P] __attribute__((aligned(32), section(".l1_prio")));
int32_t c[M * P] __attribute__((aligned(32), section(".l1_prio")));

// Initialize the matrices
void init_matrix(int32_t *matrix, uint32_t num_rows, uint32_t num_columns,
                 int32_t a, int32_t b, int32_t c) {
  for (uint32_t i = 0; i < num_rows; ++i) {
    for (uint32_t j = 0; j < num_columns; ++j) {
      matrix[i * num_columns + j] = a * (int32_t)i + b * (int32_t)j + c;
    }
  }
}

// Verify the matrices
int verify_matrix(int32_t *matrix, int32_t m, int32_t n, int32_t p, int32_t aa,
                  int32_t ab, int32_t ac, int32_t ba, int32_t bb, int32_t bc,
                  const uint32_t threadId, const uint32_t numThreads,
                  const uint32_t blockSizeM, const uint32_t blockSizeP) {
  uint32_t increment = numThreads/(m/blockSizeM);
  increment = increment == 0 ? 1 : increment;
  for (int32_t k = threadId*blockSizeM; k < m; k += blockSizeM*numThreads) {
    for (int32_t i = k; i < k+blockSizeM; i++) {
      for (int32_t l = (threadId%(m/blockSizeM))*blockSizeP; l < p; l += blockSizeP*increment) {
        for (int32_t j = l; j < l+blockSizeP; j++) {
          int32_t lin = (aa * bb * i * j + aa * bc * i + ac * bb * j + ac * bc) * n;
          int32_t qua =
              ((aa * ba * i + ab * bb * j + ab * bc + ba * ac) * (n * (n - 1))) / 2;
          int32_t cub = ((ab * ba) * (n * (n - 1) * (2 * n - 1))) / 6;
          int32_t golden = lin + qua + cub;
          if (matrix[i * (int32_t)p + j] != golden) {
            return (i + j) == 0 ? -1 : i * (int32_t)p + j;
          }
          matrix[i * (int32_t)p + j] = 0;
        }
      }
    }
  }
  return 0;
}

void print_matrix(int32_t const *matrix, uint32_t num_rows,
                  uint32_t num_columns) {
  printf("0x%8X\n", (uint32_t)matrix);
  for (uint32_t i = 0; i < num_rows; ++i) {
    for (uint32_t j = 0; j < num_columns; ++j) {
      printf("%5d ", matrix[i * num_columns + j]);
    }
    printf("\n");
  }
}

int main() {
  uint32_t core_id = mempool_get_core_id();
#ifdef DISABLE_MULTICORE
  uint32_t num_cores = 1;
#else
  uint32_t num_cores = mempool_get_core_count();
#endif

#ifndef DISABLE_MULTICORE
  mempool_barrier_init(core_id);
#endif

  if (core_id == 0) {
    printf("\n");
    printf("============\n");
    printf("=  MATMUL  =\n");
    printf("============\n");
    printf("\n");
    printf("\n");
  }

  for (uint32_t s = 4; s <= M; s *= 2) {
    if (core_id == 0) {
      printf("\n");
      printf("------------------------------------------------------------\n");
      printf("Calculating a (%d x %d) x (%d x %d) matrix multiplication...\n", s,
             s, s, s);
      printf("------------------------------------------------------------\n");
      printf("\n");

      // Initialize Matrices
      printf("Initializing matrices...\n");
    }
    init_matrix(a, s, s, A_a, A_b, A_c);
    init_matrix(b, s, s, B_a, B_b, B_c);

    // Matrices are initialized --> Start calculating
    if (core_id == 0) {
      printf("Calculating matmul...\n");
    }

#ifndef DISABLE_MULTICORE
    mempool_barrier(num_cores);
#endif

    #ifdef MEMPOOL
    uint32_t start = mempool_get_timer();
    #endif

    uint32_t blockSizeP = matmul(c, a, b, s, s, s, core_id, num_cores);

#ifndef DISABLE_MULTICORE
    mempool_barrier(num_cores);
#endif

    #ifdef MEMPOOL
    uint32_t end = mempool_get_timer();
    #endif

    if (core_id == 0) {
      // Metrics
      uint32_t runtime = end - start;
      uint32_t performance = 1000 * 2 * s * s * s / runtime;
      uint32_t utilization =  performance / (2 * num_cores * N_IPU);

      printf("The execution took %d cycles.\n", runtime);
      printf("The performance is %d OP/1000cycle (%d%%o utilization).\n", performance,
             utilization);
    }

    // Verify the result
    if (core_id == 0) {
      printf("Verifying result...\n");
    }

#ifndef DISABLE_MULTICORE
    mempool_barrier(num_cores);
#endif

#ifdef DISABLE_MULTICORE
    int error = verify_matrix(c, s, s, s, A_a, A_b, A_c, B_a, B_b, B_c, core_id, num_cores, s <= 32 ? 4 : 8, blockSizeP);
#else
    int error = verify_matrix(c, s, s, s, A_a, A_b, A_c, B_a, B_b, B_c, core_id, num_cores, s <= 8 ? 2 : s <= 16 ? 4 : 8, blockSizeP);
#endif
    if (error != 0) {
      printf("Error core %d: c[%d]=%d\n", core_id, error, c[error]);
      return error;
    } else {
      //printf("Passed.\n");
    }
  }

  return 0;
}
