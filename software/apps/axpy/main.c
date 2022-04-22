// Copyright 2022 ETH Zurich and University of Bologna.
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

// Author: Domenic Wüthrich

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "printf.h"
#ifdef MEMPOOL
#include "runtime.h"
#include "synchronization.h"
#endif

#ifndef N_IPU
#define N_IPU 2
#endif
#ifndef MATRIX_DIM
#define MATRIX_DIM 64
#endif
#ifndef KERNEL_M
#define KERNEL_M 1
#endif
#ifndef KERNEL_P
#define KERNEL_P 8
#endif

#define SIZE 8*KERNEL_P

int32_t vec_l1[SIZE*NUM_CORES] __attribute__((aligned(4), section(".l1_prio")));

int verify_vector(int32_t *vector, int32_t size, int32_t alpha) {
  for (int idx = 0; idx < size; idx++) {
    if (vector[idx] != idx*alpha + idx) {
      return idx == 0 ? -1 : idx;
    }
  }
  return 0;
}

void init_vector(int32_t *vector, int32_t size) {
  for (int idx = 0; idx < size; idx++) {
    vector[idx] = idx;
  }
}

int main() {
  uint32_t num_cores = mempool_get_core_count();
  uint32_t core_id = mempool_get_core_id();
  int32_t vec_stack[SIZE];
  int32_t *vector;

  int32_t alpha = 2;

  #if KERNEL_M == 1
    vector = vec_stack;
  #else
    vector = vec_l1+SIZE*core_id;
  #endif
  init_vector(vector, SIZE);

  #ifdef DISABLE_MULTICORE
    num_cores = 1;
  #else
    mempool_barrier_init(core_id);
    mempool_barrier(num_cores);
  #endif

  asm volatile("vsetvli zero, %0, e32, m8, ta, ma" :: "r"(SIZE));

  uint32_t timer_start = mempool_get_timer();

  asm volatile("vle32.v v0, (%0)" :: "r"(vector));
  asm volatile("vle32.v v8, (%0)" :: "r"(vector));
  asm volatile("vmacc.vx v0, %0, v8" :: "r"(alpha));
  asm volatile("vse32.v v0, (%0)" :: "r"(vector));

  #ifdef DISABLE_MULTICORE
    asm volatile("lw zero, 0(%0)" :: "r"(vector));
  #else
    mempool_barrier(num_cores);
  #endif

  // Performance metrics
  uint32_t timer_end = mempool_get_timer();

  if (core_id == 0) {
    uint32_t runtime = timer_end - timer_start - 1;
    uint32_t performance = 1000 * 2 * SIZE*num_cores / runtime;
    uint32_t perm_of_max = performance * 6 / (N_IPU*4 * num_cores);

    printf("The execution took %u cycles.\n", runtime);
    printf("The performance is %u OP/1000cycles (%u%%o permille of max).\n",
           performance, perm_of_max);
    // Verify correctness
    printf("Verifying result...\n");
  }

  int error = verify_vector(vector, SIZE, alpha);
  if (error != 0) {
    printf("Fail.\n");
    return (int)core_id*SIZE+error;
  }

  #ifndef DISABLE_MULTICORE
    mempool_barrier(num_cores);
  #endif

  return 0;
}
