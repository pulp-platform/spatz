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
#include "kernel/compute_check_to_var_msgs.c"
 
// Reference check for one core's chunk [offset, offset+len).
// Mirrors ldpc_decoder_generic::compute_check_to_var_msgs (without scaling, as the
// kernel does not scale). tmp is the chunk-local un-rotated position (j - shift) mod Z.
int compute_check(uint32_t offset, uint32_t len, uint32_t vnode, uint32_t shift, uint32_t Z,
                  const uint8_t *index_src, const int8_t *min_src, const int8_t *second_src,
                  const uint8_t *sign_src, const int8_t *this_var_to_check,
                  const int8_t *this_check_to_var)
{
  int errors = 0;
 
  for (unsigned j = 0; j < len; ++j) {
    unsigned k   = offset + j;
    unsigned tmp = (j + Z - shift) % Z;   // un-rotation: (j - shift) mod Z
    unsigned a   = offset + tmp;           // aggregate element for output position j
 
    // magnitude = (vnode != argmin) ? min1 : min2
    int8_t mag = (vnode != index_src[a]) ? min_src[a] : second_src[a];
 
    // final sign = sign_prod XOR (v2c < 0), with the aggregate read un-rotated (a)
    // and the v2c read in the node frame (k).
    uint8_t sgn   = (sign_src[a] != 0) ? 1u : 0u;
    uint8_t neg   = (this_var_to_check[k] < 0) ? 1u : 0u;
    uint8_t fsign = (uint8_t)(sgn ^ neg);
 
    int8_t exp = fsign ? (int8_t)(-mag) : mag;
 
    if (this_check_to_var[k] != exp) {
      printf("c2v[%u]: shift=%u vnode=%u tmp=%u mag=%d fsign=%u -> got %d, exp %d\n",
             j, shift, vnode, tmp, mag, fsign, this_check_to_var[k], exp);
      ++errors;
    }
  }
 
  if (errors == 0)
    printf("compute_check_to_var_msgs @%u: OK (%u elements)\n", offset, len);
  else
    printf("compute_check_to_var_msgs @%u: %d errors\n", offset, errors);
  return errors;
}
 
int main()
{
  const unsigned num_cores = snrt_cluster_core_num();
  const unsigned cid       = snrt_cluster_core_idx();
  const unsigned total     = num_cores * lifting_size;
 
  const uint32_t vnode  = var_node;   // from data header
  const uint32_t vshift = shift;      // from data header, expected in [0, Z-1]
  // Inputs (per-core chunk of length lifting_size, num_cores chunks total).
  int8_t  *min_src;
  int8_t  *second_src;
  uint8_t *index_src;
  uint8_t *sign_src;
  int8_t  *this_var_to_check;
  
  // Duplicated (2*Z per core) buffers actually consumed by the kernel.
  int8_t  *min_dup;
  int8_t  *second_dup;
  uint8_t *index_dup;
  uint8_t *sign_dup;
  
  // Output.
  int8_t  *this_check_to_var;
 
  unsigned timer = (unsigned)-1;
 
  if (cid == 0) {
    min_src           = (int8_t  *)snrt_l1alloc(total * sizeof(int8_t));
    second_src        = (int8_t  *)snrt_l1alloc(total * sizeof(int8_t));
    index_src         = (uint8_t *)snrt_l1alloc(total * sizeof(uint8_t));
    sign_src          = (uint8_t *)snrt_l1alloc(total * sizeof(uint8_t));
    this_var_to_check = (int8_t  *)snrt_l1alloc(total * sizeof(int8_t));
 
    this_check_to_var = (int8_t  *)snrt_l1alloc(total * sizeof(int8_t));
 
    // Double-length buffers: 2*Z per core, second half is a copy of the first.
    min_dup    = (int8_t  *)snrt_l1alloc(2 * total * sizeof(int8_t));
    second_dup = (int8_t  *)snrt_l1alloc(2 * total * sizeof(int8_t));
    index_dup  = (uint8_t *)snrt_l1alloc(2 * total * sizeof(uint8_t));
    sign_dup   = (uint8_t *)snrt_l1alloc(2 * total * sizeof(uint8_t));
 
    snrt_dma_start_1d(min_src,           min_var_to_check_dram,        total * sizeof(int8_t));
    snrt_dma_start_1d(second_src,        second_min_var_to_check_dram, total * sizeof(int8_t));
    snrt_dma_start_1d(index_src,         min_var_to_check_index_dram,  total * sizeof(uint8_t));
    snrt_dma_start_1d(sign_src,          sign_prod_var_to_check_dram,  total * sizeof(uint8_t));
    snrt_dma_start_1d(this_var_to_check, this_var_to_check_dram,       total * sizeof(int8_t));
    snrt_dma_wait_all();
 
    // Build the duplicated buffers (setup, kept OUT of the timed region: this is the
    // scale+dup pre-step that in the real pipeline runs once per check node).
    for (unsigned c = 0; c < num_cores; ++c) {
      const int8_t  *ms = min_src    + c * lifting_size;
      const int8_t  *ss = second_src + c * lifting_size;
      const uint8_t *is = index_src  + c * lifting_size;
      const uint8_t *gs = sign_src   + c * lifting_size;
      int8_t  *md = min_dup    + c * 2 * lifting_size;
      int8_t  *sd = second_dup + c * 2 * lifting_size;
      uint8_t *id = index_dup  + c * 2 * lifting_size;
      uint8_t *gd = sign_dup   + c * 2 * lifting_size;
      for (unsigned i = 0; i < lifting_size; ++i) {
        md[i] = md[i + lifting_size] = ms[i];
        sd[i] = sd[i + lifting_size] = ss[i];
        id[i] = id[i + lifting_size] = is[i];
        gd[i] = gd[i + lifting_size] = gs[i];
      }
    }
  }
 
  snrt_cluster_hw_barrier();
 
  if (cid == 0) start_kernel();
  if (cid == 0) timer = benchmark_get_cycle();
 
  //snrt_cluster_hw_barrier();
  if (cid == 0)
    compute_check_to_var_msgs(this_check_to_var + cid * lifting_size,
                              this_var_to_check + cid * lifting_size,
                              min_dup    + cid * 2 * lifting_size,
                              second_dup + cid * 2 * lifting_size,
                              index_dup  + cid * 2 * lifting_size,
                              sign_dup   + cid * 2 * lifting_size,
                              vshift,
                              vnode,
                              lifting_size);
  //snrt_cluster_hw_barrier();

  if (cid == 0) timer = benchmark_get_cycle() - timer;
  if (cid == 0) stop_kernel();
 
  snrt_cluster_hw_barrier();
 
  if (cid == 0) {
    long unsigned performance = 1000 * 6 * lifting_size / timer;
    long unsigned utilization = performance / (4 * 4);
 
    printf("\n----- (%d) compute_check_to_var_msgs -----\n", lifting_size);
    printf("The execution took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }
 
  snrt_cluster_hw_barrier();
 
  if (cid == 0) {
    int errors = 0;
    errors += compute_check(0, lifting_size, vnode, vshift, lifting_size,
                            index_src, min_src, second_src, sign_src,
                            this_var_to_check, this_check_to_var);
    printf("TOTAL: %d errors\n", errors);
  }
 
  snrt_cluster_hw_barrier();
  return 0;
}