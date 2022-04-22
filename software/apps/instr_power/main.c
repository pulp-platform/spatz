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

#define SIZE 16
#define REPEAT 40

volatile uint32_t vector[SIZE] __attribute__((section(".l1")));

uint32_t block;

int main() {
  uint32_t core_id = mempool_get_core_id();

  if (core_id != 0) {
    mempool_wfi();
  }

  for (uint32_t i = 0; i < SIZE; i++) {
    vector[i] = (0xabababab >> (i%32)) + i;
  }

  int32_t vector_size;

  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vector_size) : "r"(SIZE));

  if (vector_size != SIZE) return SIZE-vector_size;

  for (int i = 0; i < REPEAT; i++) asm volatile("vle32.v v0, (%[vector])" :: [vector]"r"(vector));

  for (int i = 0; i < SIZE; i++) asm volatile("nop");

  block = 1;

  uint32_t insert = vector[0];

  for (int i = 0; i < REPEAT; i++) asm volatile("vslide1down.vx v8, v0, %0" :: "r"(insert));

  for (int i = 0; i < SIZE; i++) asm volatile("nop");

  for (int i = 0; i < REPEAT; i++) asm volatile("vadd.vv v16, v0, v8");

  for (int i = 0; i < SIZE; i++) asm volatile("nop");

  for (int i = 0; i < REPEAT; i++) asm volatile("vmul.vv v16, v8, v0");

  for (int i = 0; i < SIZE; i++) asm volatile("nop");

  for (int i = 0; i < REPEAT; i++) asm volatile("vmacc.vv v16, v8, v0");

  for (int i = 0; i < SIZE; i++) asm volatile("nop");

  return 0;
}
