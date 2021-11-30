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

// clang-format off

#include <stdint.h>

#define VEC_LENGTH 33

uint32_t vector1[VEC_LENGTH] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01};
uint32_t vector2[VEC_LENGTH] = {0xfe, 0xdc, 0xbd, 0x98, 0x76, 0x54, 0x32, 0x10, 0xfe, 0xdc, 0xbd, 0x98, 0x76, 0x54, 0x32, 0x10, 0xfe, 0xdc, 0xbd, 0x98, 0x76, 0x54, 0x32, 0x10, 0xfe, 0xdc, 0xbd, 0x98, 0x76, 0x54, 0x32, 0x10, 0xfe};
uint32_t vector_res[VEC_LENGTH];

int main() {

  uint32_t vlen = 128;
  uint32_t actual_vlen = 0;
  uint32_t a = 0x10;
  uint32_t csr;
  uint32_t val = 64;

  asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(actual_vlen) : "r"(vlen));
  asm volatile("csrrs %[csr], vl, x0" : [csr]"=r"(csr));
  if (actual_vlen != csr) return 1;
  asm volatile("vrsub.vx v8, v0, %[a]" :: [a]"r"(a));
  asm volatile("vadd.vi v4, v8, 5");
  // Shorten vl by one

  // Remove
  asm volatile("vle8.v v12, (%[vector1])" :: [vector1]"r"(vector1));
  asm volatile("vle8.v v13, (%[vector2])" :: [vector2]"r"(vector2));

  vlen -= 1;
  asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(actual_vlen) : "r"(vlen));
  asm volatile("csrrs %[csr], vl, x0" : [csr]"=r"(csr));

  // Remove
  asm volatile("vse8.v v12, (%[vector_res])" :: [vector_res]"r"(vector_res));
  asm volatile("vse8.v v13, (%[vector_res])" :: [vector_res]"r"(vector_res+1));

  if (actual_vlen != csr) return 2;
  // Set vstart to one (second element)
  asm volatile("csrrwi x0, vstart, 1");
  // Clear registers and execute macc
  asm volatile("vadd.vi v0, v0, 2");
  asm volatile("vadd.vx v12, v12, %[val]" :: [val]"r"(val));
  asm volatile("vadd.vi v28, v28, 5");
  asm volatile("vmacc.vv v28, v0, v12");
  asm volatile("vadd.vi v20, v28, 0");
  // Execute instruction with vs1 and vs2 as source, and vd as destination
  asm volatile("vsub.vv v16, v20, v0");
  // Stripmining loop
  /*uint32_t remaining_elem = VEC_LENGTH;
  uint32_t *ptr_vec1 = vector1;
  uint32_t *ptr_vec2 = vector2;
  uint32_t *ptr_vec_res = vector_res;
  while (remaining_elem != 0) {
    uint32_t actual_elem;
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(actual_elem) : "r"(remaining_elem));
    asm volatile("vle8.v v12, (%[vector1])" :: [vector1]"r"(ptr_vec1));
    asm volatile("vle8.v v13, (%[vector2])" :: [vector2]"r"(ptr_vec2));
    asm volatile("vadd.vv v14, v12, v13");
    asm volatile("vse8.v v14, (%[vector_res])" :: [vector_res]"r"(ptr_vec_res));
    remaining_elem -= actual_elem;
    ptr_vec1 += actual_elem;
    ptr_vec2 += actual_elem;
    ptr_vec_res += actual_elem;
  }
  for (int i = 0; i < VEC_LENGTH; i++) {
    if (vector1[i] + vector2[i] != vector_res[i]) return 1;
  }*/

  // Remove
  if (vector1[0] != vector_res[0]) return 2;
  if (vector2[0] != vector_res[1]) return 3;

  // Buffer instruction to make sure last instruction will finish within simulation
  asm volatile("csrrs %[csr], vl, x0" : [csr]"=r"(csr));

  return 0;
}

// clang-format on
