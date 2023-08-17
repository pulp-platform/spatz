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

// Author: Enis Mustafa, ETH Zurich

#include "mxmatmul.h"
#ifdef MEMPOOL
#include "runtime.h"
#endif
#define MIN(a, b) ((a) < (b) ? (a) : (b))


#define TILE_LD_A
#define TILE_LD_B
#define MXMACC_TEST

void matmul_tiled_load_store_test(uint32_t *c, const uint32_t *a, const uint32_t *b,
                 const unsigned int dim) {

  /* Vector Length Setting*/
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(16));

  /* Tile Load Settings*/
  //asm volatile("msettilem t1, %0" ::"r" (8));
  //asm volatile("msettilen t2, %0" ::"r" (8));
  //asm volatile("msettilek t3, %0" ::"r" (8));

  /* Clean up result register*/
  asm volatile("vmv.v.i v0, 0");

  /* Input and Output address*/
  const uint32_t *a_ = a;
  const uint32_t *c_ = c;

  /* Load and Store Testing*/
  // Load test (Tile load + Vector store)
  //asm volatile("mle32.v.a v0, (%0), %1;" ::"r"(a_), "r"(dim));
  //asm volatile("vse32.v v0, (%0);" ::"r"(c_));

  // Store test (Vector load + Tile store)
  asm volatile("vle32.v v0, (%0);" ::"r"(a_));
  asm volatile("mse32.v.c v0, (%0), %1;" ::"r"(c_), "r"(8));
}

void matmul_tiled_mxmacc_test(uint32_t *c, const uint32_t *a, const uint32_t *b,
                 const unsigned int dim) {

  /* Vector Length Setting*/
  asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(32));
  const uint32_t *a_ = a;
  const uint32_t *b_ = b;
  const uint32_t *c_ = c;


  /* Tile Load A*/
#ifdef TILE_LD_A
  asm volatile("msettilem t1, %0" ::"r" (8));
  asm volatile("msettilen t2, %0" ::"r" (4));
  asm volatile("vmv.v.i v0, 0");
  asm volatile("mle32.v.a v0, (%0), %1;" ::"r"(a_), "r"(4));
  //asm volatile("vse32.v v0, (%0);" ::"r"(c_)); // Optional Print
#endif

  /* Tile Load B*/
#ifdef TILE_LD_B
  asm volatile("msettilem t1, %0" ::"r" (8));
  asm volatile("msettilen t2, %0" ::"r" (4));
  asm volatile("vmv.v.i v4, 0");
  asm volatile("mle32.v.b v4, (%0), %1;" ::"r"(b_), "r"(4));
  asm volatile("vse32.v v4, (%0);" ::"r"(c_)); // Optional Print
#endif

  /* mxmacc Testing*/
#ifdef MXMACC_TEST
  asm volatile("vmv.v.i v8, 0");
  asm volatile("mxmacc.vv v8, v0, v4");
  //asm volatile("mse32.v.c v8, (%0), %1;" ::"r"(c_), "r"(8));
  //asm volatile("vse32.v v8, (%0);" ::"r"(c_)); // Optional Print
#endif
}
