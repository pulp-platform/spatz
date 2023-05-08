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

// Author: Domenic Wuthrich

#ifdef MEMPOOL
#include "runtime.h"
#include "synchronization.h"
#endif

#define SIZE 32
#define REPEAT 40

int main() {
  uint32_t core_id = mempool_get_core_id();

  if (core_id >= NUM_CORES_PER_TILE) {
    mempool_wfi();
  }

  uint32_t vector[SIZE];
  for (uint32_t i = 0; i < SIZE; i++) {
    vector[i] = (0xabababab >> (i % 32)) + i;
  }

  int32_t vector_size;

  asm volatile("vsetvli %0, %1, e32, m8, ta, ma"
               : "=r"(vector_size)
               : "r"(SIZE));

  if (vector_size != SIZE)
    return SIZE - vector_size;

  for (int i = 0; i < REPEAT / 4; i++) {
    asm volatile("vle32.v v0,  (%[vector])" ::[vector] "r"(vector));
    asm volatile("vle32.v v8,  (%[vector])" ::[vector] "r"(vector));
    asm volatile("vle32.v v16, (%[vector])" ::[vector] "r"(vector));
    asm volatile("vle32.v v24, (%[vector])" ::[vector] "r"(vector));
  }

  for (int i = 0; i < SIZE; i++)
    asm volatile("nop");

  for (int i = 0; i < REPEAT; i++)
    asm volatile("vadd.vv v16, v0, v8");

  for (int i = 0; i < SIZE; i++)
    asm volatile("nop");

  for (int i = 0; i < REPEAT; i++)
    asm volatile("vmul.vv v16, v8, v0");

  for (int i = 0; i < SIZE; i++)
    asm volatile("nop");

  for (int i = 0; i < REPEAT; i++)
    asm volatile("vmacc.vv v16, v8, v0");

  for (int i = 0; i < SIZE; i++)
    asm volatile("nop");

  return 0;
}
