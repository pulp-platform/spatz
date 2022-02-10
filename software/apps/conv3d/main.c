// Copyright 2020 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti
//         Domenic Wüthrich

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "conv3d_3x7x7.c"

#include "printf.h"
#ifdef MEMPOOL
#include "runtime.h"
#include "synchronization.h"
#endif

// Define Matrix dimensions:
// o = i ° f, with i=[(M+F-1)x(N+f-1)xCH], f=[FxFxCH], o=[MxN]
// The filter is a square matrix, and F is odd

// Matrices defined in data.S
extern int32_t i[] __attribute__((aligned(4))); // [ (M+floor(F/2)) * (N+floor(F/2)) * CH ]
extern int32_t f[] __attribute__((aligned(4)));        // [ F*F*CH ]
extern int32_t o[] __attribute__((aligned(4)));        // [ M*N ]
extern int32_t golden_o[] __attribute__((aligned(4))); // [ M*N ]
// M, N, F defined in data.S
extern int32_t M;
extern int32_t N;
extern int32_t CH;
extern int32_t F;

#define M 64
#define N 64
#define F 7
#define CH 3

int32_t i_l1[(M+F/2*2)*(N+F/2*2)*CH] __attribute__((aligned(4), section(".l1_prio"))); // [ (M+floor(F/2)) * (N+floor(F/2)) * CH ]
int32_t f_l1[F*F*CH] __attribute__((aligned(4), section(".l1_prio")));        // [ F*F*CH ]
int32_t o_l1[M*N] __attribute__((aligned(4), section(".l1_prio")));        // [ M*N ]

int verify_matrix(int32_t *matrix, int32_t *golden_matrix, int32_t R,
                  int32_t C) {
  for (int r = 0; r < R; ++r)
    for (int c = 0; c < C; ++c)
      if (matrix[c + C * r] != golden_matrix[c + C * r]) {
        printf("Error: o[%d][%d] = %ld, instead of %ld\n", r, c,
               matrix[c + C * r], golden_matrix[c + C * r]);
        return 1;
      }
  return 0;
}

void print_matrix(int32_t const *matrix, uint32_t num_rows,
                  uint32_t num_columns) {
  printf("0x%8X\n", (uint32_t)matrix);
  for (uint32_t i = 0; i < num_rows; ++i) {
    for (uint32_t j = 0; j < num_columns; ++j) {
      printf("%10f ", matrix[i * num_columns + j]);
    }
    printf("\n");
  }
}

int main() {
  printf("\n");
  printf("============\n");
  printf("=  CONV3D  =\n");
  printf("============\n");
  printf("\n");
  printf("\n");

  printf("Input Mtx size: %dx%d\n", M + F - 1, N + F - 1);
  printf("Output Mtx size: %dx%d\n", M, N);
  printf("Filter size: %dx%d\n", F, F);
  printf("Channels: %d\n", CH);

  for (int idx = 0; idx < (M+F/2*2)*(N+F/2*2)*CH; idx++) {
    i_l1[idx] = i[idx];
  }

  for (int idx = 0; idx < F*F*CH; idx++) {
    f_l1[idx] = f[idx];
  }

  // Call the main kernel, and measure cycles
  uint32_t timer_start = mempool_get_timer();
  if (F == 7)
    conv3d_CHx7x7(o, i_l1, f_l1);
  else
    printf("Error: the filter size is different from 7.\n");

  // Performance metrics
  uint32_t timer_end = mempool_get_timer();
  uint32_t runtime = timer_end - timer_start;
  uint32_t performance = 1000 * 2 * CH * F * F * M * N / runtime;
  uint32_t utilization = performance / (2 * 2);

  printf("The execution took %u cycles.\n", runtime);
  printf("The performance is %u OP/1000cycles (%u%%o utilization).\n",
         performance, utilization);

  // Verify correctness
  printf("Verifying result...\n");
  int error = verify_matrix(o, golden_o, M, N);
  if (error != 0) {
    printf("Fail.\n");
  } else {
    printf("Passed.\n");
  }

  return error;
}
