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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "conv2d_3x3.c"
#include "conv2d_5x5.c"
#include "conv2d_7x7.c"

#include "printf.h"
#ifdef MEMPOOL
#include "runtime.h"
#include "synchronization.h"
#endif

// Define Matrix dimensions:
// o = i ° f, with i=[MxN], f=[FxF], o=[MxN]
// The filter is a square matrix, and F is odd

// Matrices defined in data.S
extern int32_t i[] __attribute__((aligned(4))); // [ (M+floor(F/2)) * (N+floor(F/2)) ]
extern int32_t f[] __attribute__((aligned(4)));        // [ F*F ]
extern int32_t o[] __attribute__((aligned(4)));        // [ M*N ]
extern int32_t golden_o[] __attribute__((aligned(4))); // [ M*N ]
// M, N, F defined in data.S
extern int32_t M;
extern int32_t N;
extern int32_t F;

// Verify the matrices
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
      printf("%10d ", matrix[i * num_columns + j]);
    }
    printf("\n");
  }
}

int main() {
  printf("\n");
  printf("============\n");
  printf("=  CONV2D  =\n");
  printf("============\n");
  printf("\n");
  printf("\n");

  // Call the main kernel, and measure cycles
  uint32_t timer_start = timer_start = mempool_get_timer();
  if (F == 3)
    conv2d_3x3(o, i, f, M, N, F);
  else if (F == 5)
    conv2d_5x5(o, i, f, M, N, F);
  else if (F == 7)
    conv2d_7x7(o, i, f, M, N, F);
  else
    printf("Error: the filter size is different from 3 or 5 or 7.\n");

  // Performance metrics
  uint32_t timer_end = mempool_get_timer();
  uint32_t runtime = timer_end - timer_start;
  printf("The execution took %d cycles.\n", runtime);

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
