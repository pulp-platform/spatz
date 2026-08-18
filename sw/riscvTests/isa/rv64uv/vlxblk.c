// Copyright 2026 ETH Zurich and University of Bologna.
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

// Author: Fatih Özdemir, ETH Zurich
#include "vector_macros.h"

#include <stdint.h>
#include <string.h>

#define MAX_BLOCK_LEN 16
#define MAX_GROUPS 4
#define MAX_ELEMS (MAX_BLOCK_LEN * MAX_GROUPS)
#define NUM_CODEBOOK_BLOCKS 4

static const uint16_t codebook_init[NUM_CODEBOOK_BLOCKS * MAX_BLOCK_LEN] = {
    0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107,
    0x0108, 0x0109, 0x010a, 0x010b, 0x010c, 0x010d, 0x010e, 0x010f,
    0x0200, 0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206, 0x0207,
    0x0208, 0x0209, 0x020a, 0x020b, 0x020c, 0x020d, 0x020e, 0x020f,
    0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306, 0x0307,
    0x0308, 0x0309, 0x030a, 0x030b, 0x030c, 0x030d, 0x030e, 0x030f,
    0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
    0x0408, 0x0409, 0x040a, 0x040b, 0x040c, 0x040d, 0x040e, 0x040f,
};

static const uint8_t indices_init[MAX_GROUPS] = {2, 0, 3, 1};

static uint16_t actual_init[MAX_ELEMS];
static uint16_t golden_init[MAX_ELEMS];

static uint16_t *codebook;
static uint8_t *indices;
static uint16_t *actual;
static uint16_t *golden;

static void init_buffers(void) {
#ifdef __SPIKE__
  codebook = (uint16_t *)codebook_init;
  indices = (uint8_t *)indices_init;
  actual = actual_init;
  golden = golden_init;
#else
  codebook = (uint16_t *)snrt_l1alloc(sizeof(codebook_init));
  indices = (uint8_t *)snrt_l1alloc(sizeof(indices_init));
  actual = (uint16_t *)snrt_l1alloc(sizeof(actual_init));
  golden = (uint16_t *)snrt_l1alloc(sizeof(golden_init));
#endif

  memcpy(codebook, codebook_init, sizeof(codebook_init));
  memcpy(indices, indices_init, sizeof(indices_init));
}

static void fill_golden(unsigned int block_len, unsigned int groups) {
  for (unsigned int g = 0; g < groups; ++g) {
    const uint16_t *block = codebook + ((unsigned int)indices[g] * block_len);
    for (unsigned int i = 0; i < block_len; ++i) {
      golden[g * block_len + i] = block[i];
    }
  }
}

static void run_vlxblkei8_case(unsigned int case_id, unsigned int block_len,
                               unsigned int groups) {
  const unsigned int elem_count = block_len * groups;

  memset(actual, 0, sizeof(actual_init));
  memset(golden, 0, sizeof(golden_init));
  fill_golden(block_len, groups);

  register const uint16_t *codebook_reg asm("t2") = codebook;
  asm volatile("" ::"r"(codebook_reg));

  asm volatile("vsetblklen %[block_len]\n"
               "vsetvli zero, %[groups], e8, m2, ta, ma\n"
               "vle8.v v28, (%[indices])\n"
               "vsetvli zero, %[elem_count], e16, m4, ta, ma\n"
               "vlxblkei8.v v16, (%[codebook]), v28\n"
               "vse16.v v16, (%[actual])\n"
               :
               : [block_len] "r"(block_len), [groups] "r"(groups),
                 [elem_count] "r"(elem_count), [indices] "r"(indices),
                 [codebook] "r"(codebook_reg), [actual] "r"(actual)
               : "v16", "v28", "memory");

  VMCMP(uint16_t, %hu, case_id, actual, golden, elem_count);
}

void TEST_CASE1(void) { run_vlxblkei8_case(1, 4, 4); }

void TEST_CASE2(void) { run_vlxblkei8_case(2, 8, 4); }

void TEST_CASE3(void) { run_vlxblkei8_case(3, 16, 2); }

int main(void) {
  INIT_CHECK();
  enable_vec();
  init_buffers();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();

  EXIT_CHECK();
}
