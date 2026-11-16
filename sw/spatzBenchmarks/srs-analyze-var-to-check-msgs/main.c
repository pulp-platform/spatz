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

// Author: Ihor Sachok <ihor.sachok@unibo.it>

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/analyze_var_to_check_msgs.c"

static int8_t llr_abs(int8_t v) { return (v < 0) ? (int8_t)(-v) : v; }

int analyze_check(const uint32_t offset, const uint32_t len, const uint32_t vnode,
                  const int8_t  *min_var_to_check,
                  const int8_t  *second_min_var_to_check,
                  const uint8_t *min_var_to_check_index,
                  const uint8_t *sign_prod_var_to_check)
{
  int errors = 0;

  for (unsigned j = 0; j < len; ++j) {
    unsigned k = offset + j;

    int8_t  old_min    = min_var_to_check_dram[k];
    int8_t  old_second = second_min_var_to_check_dram[k];
    uint8_t old_index  = min_var_to_check_index_dram[k];
    uint8_t old_sign   = sign_prod_var_to_check_dram[k];
    int8_t  rot        = rotated_node_dram[k];

    int8_t  abs_v      = llr_abs(rot);
    int     is_min     = (abs_v < old_min);
    int8_t  new_second = is_min ? old_min : abs_v;
    int     is_best2   = (abs_v < old_second);

    int8_t  e_min    = is_min   ? abs_v      : old_min;
    int8_t  e_second = is_best2 ? new_second : old_second;
    uint8_t e_index  = is_min   ? (uint8_t)vnode : old_index;
    uint8_t e_sign   = old_sign ^ ((rot >= 0) ? 0U : 1U);

    if (min_var_to_check[k] != e_min) {
      printf("min[%u]: rot=%d old_min=%d -> got %d, exp %d\n",
             j, rot, old_min, min_var_to_check[k], e_min);
      ++errors;
    }
    if (second_min_var_to_check[k] != e_second) {
      printf("second[%u]: rot=%d old_min=%d old_sec=%d -> got %d, exp %d\n",
             j, rot, old_min, old_second, second_min_var_to_check[k], e_second);
      ++errors;
    }
    if (min_var_to_check_index[k] != e_index) {
      printf("index[%u]: rot=%d old_idx=%u -> got %u, exp %u\n",
             j, rot, old_index, min_var_to_check_index[k], e_index);
      ++errors;
    }
    if (sign_prod_var_to_check[k] != e_sign) {
      printf("sign[%u]: rot=%d old_sign=%u -> got %u, exp %u\n",
             j, rot, old_sign, sign_prod_var_to_check[k], e_sign);
      ++errors;
    }
  }

  if (errors == 0)
    printf("analyze_var_to_check_msgs @%u: OK (%u elements)\n", offset, len);
  else
    printf("analyze_var_to_check_msgs @%u: %d errors\n", offset, len ? errors : 0);
  return errors;
}

int main()
{
  const unsigned num_cores = snrt_cluster_core_num();
  const unsigned cid       = snrt_cluster_core_idx();
  const unsigned total     = num_cores * lifting_size;

  const uint32_t vnode = var_node;

  unsigned timer = (unsigned)-1;
  int8_t  *min_var_to_check;
  int8_t  *second_min_var_to_check;
  uint8_t *min_var_to_check_index;
  uint8_t *sign_prod_var_to_check;
  int8_t  *rotated_node;

  if (cid == 0) {
    min_var_to_check        = (int8_t  *)snrt_l1alloc(total * sizeof(int8_t));
    second_min_var_to_check = (int8_t  *)snrt_l1alloc(total * sizeof(int8_t));
    min_var_to_check_index  = (uint8_t *)snrt_l1alloc(total * sizeof(uint8_t));
    sign_prod_var_to_check  = (uint8_t *)snrt_l1alloc(total * sizeof(uint8_t));
    rotated_node            = (int8_t  *)snrt_l1alloc((total + 16) * sizeof(int8_t));

    snrt_dma_start_1d(min_var_to_check,        min_var_to_check_dram,        total * sizeof(int8_t));
    snrt_dma_start_1d(second_min_var_to_check, second_min_var_to_check_dram, total * sizeof(int8_t));
    snrt_dma_start_1d(min_var_to_check_index,  min_var_to_check_index_dram,  total * sizeof(uint8_t));
    snrt_dma_start_1d(sign_prod_var_to_check,  sign_prod_var_to_check_dram,  total * sizeof(uint8_t));
    snrt_dma_start_1d(rotated_node,            rotated_node_dram,            total * sizeof(int8_t));
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) start_kernel();
  if (cid == 0) timer = benchmark_get_cycle();

  //snrt_cluster_hw_barrier();
  if (cid == 0){
  analyze_var_to_check_msgs(min_var_to_check        + cid * lifting_size,
                            second_min_var_to_check + cid * lifting_size,
                            min_var_to_check_index  + cid * lifting_size,
                            sign_prod_var_to_check  + cid * lifting_size,
                            rotated_node            + cid * lifting_size,
                            vnode,
                            lifting_size);
  }

  //snrt_cluster_hw_barrier();
  if (cid == 0) timer = benchmark_get_cycle() - timer;
  if (cid == 0) stop_kernel();
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    long unsigned performance = 1000 * 10 * lifting_size / timer;
    long unsigned utilization = performance / (4 * 4);

    printf("\n----- (%d) analyze_var_to_check_msgs -----\n", lifting_size);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    analyze_check(0, lifting_size, vnode,
                  min_var_to_check, second_min_var_to_check,
                  min_var_to_check_index, sign_prod_var_to_check);
    //if (num_cores == 1)
    //  analyze_check(lifting_size, lifting_size, vnode,
    //                min_var_to_check, second_min_var_to_check,
    //                min_var_to_check_index, sign_prod_var_to_check);
  }

  snrt_cluster_hw_barrier();
  return 0;
}