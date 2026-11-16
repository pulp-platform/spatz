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
 
#include "kernel/ldpc_decoder_ctx.h"
#include DATAHEADER
 
#include "kernel/compute_var_to_check_msgs.c"
#include "kernel/analyze_var_to_check_msgs.c"
#include "kernel/compute_check_to_var_msgs.c"
#include "kernel/scale_llr.c"
#include "kernel/compute_soft_bits.c"
#include "kernel/load_soft_bits.c"
#include "kernel/hard_decision.c"
//#include "kernel/reset_accumulators.c"
#include "kernel/ldpc_init.c"
#include "kernel/ldpc_decoder.c"   // update_variable_to_check, update_check_to_variable_messages, update_soft_bits
#include "kernel/crc.h"
#include "kernel/crc.c"

#define N_FULL 68
 
static int cmp(const char *tag, unsigned layer, const int8_t *got,
               const int8_t *exp, unsigned len) {
  for (unsigned j = 0; j < len; ++j){
    if (got[j] != exp[j]) {
      printf("MISMATCH %s layer=%u pos=%u got=%d exp=%d\n",
             tag, layer, j, (int)got[j], (int)exp[j]);
      return 1;
    }
  }
  return 0;
}
 
 
int main() {
  const unsigned cid = snrt_cluster_core_idx();
  ldpc_ctx_t ctx;
  if (cid == 0){
  unsigned int timer = (unsigned int)-1;
  unsigned int timer1 = 0;
  unsigned int timer2 = 0;
  unsigned int timer3 = 0;
  unsigned int timer_load = 0;
  unsigned int timer_hard = 0;
  const uint16_t Z = GL_Z;
  ldpc_init(&ctx, Z, 1);
  static crc_t crc_ctx;
  crc_init(&crc_ctx, GL_CRC_POLY, GL_CRC_ORDER);
  
  uint8_t *hard = (uint8_t*)snrt_l1alloc(22 * Z);
  const unsigned nof_significant = ctx.bg_K * Z - GL_NOF_FILLER;
  int8_t *gl_input_llrs_l1 = (int8_t*)snrt_l1alloc((size_t)GL_INPUT_LEN);
  snrt_dma_start_1d(gl_input_llrs_l1, gl_input_llrs, GL_INPUT_LEN);
  snrt_dma_wait_all();
  // 1) load + verify soft_init
  timer = benchmark_get_cycle();
  load_soft_bits(gl_input_llrs_l1, GL_INPUT_LEN, ctx.soft, Z, N_FULL);
  asm volatile("fence" ::: "memory");
  timer_load = benchmark_get_cycle() - timer;
  printf("load_soft_bits took %u cycles.\n", timer_load);
  // int8_t *gold_soft = (int8_t*)snrt_l1alloc((size_t)Z);
  // snrt_dma_start_1d(gold_soft, gl_soft_init + 2*Z, Z);
  // snrt_dma_wait_all();
 
 // for (unsigned i = 0; i < (unsigned)ctx.total_edges * Z; ++i) ctx.c2v[i] = 0;
  // int8_t *golden_c2v = (int8_t*)snrt_l1alloc((size_t)ctx.max_deg * Z);
  // int failed = 0;
  // uint8_t *touch_l1 = (uint8_t*)snrt_l1alloc((size_t)N_FULL);
  // uint16_t *off_l1 = (uint16_t*)snrt_l1alloc((size_t)GL_NOF_LAYERS * sizeof(uint16_t));
  // uint8_t  *deg_l1 = (uint8_t *)snrt_l1alloc((size_t)GL_NOF_LAYERS);
  // snrt_dma_start_1d(off_l1, gl_c2v_off, (size_t)GL_NOF_LAYERS * sizeof(uint16_t));
  // snrt_dma_start_1d(deg_l1, gl_c2v_deg, (size_t)GL_NOF_LAYERS);
  // snrt_dma_wait_all();
  // const int8_t **c2v_ptr  = (const int8_t**)snrt_l1alloc((size_t)GL_NOF_LAYERS * sizeof(void*));
  // const int8_t **soft_ptr = (const int8_t**)snrt_l1alloc((size_t)GL_NOF_LAYERS * sizeof(void*));
  // const uint8_t **tch_ptr = (const uint8_t**)snrt_l1alloc((size_t)GL_NOF_LAYERS * sizeof(void*));
  // snrt_dma_start_1d(c2v_ptr,  gl_c2v_layers,   (size_t)GL_NOF_LAYERS * sizeof(void*));
  // snrt_dma_start_1d(soft_ptr, gl_soft_layers,  (size_t)GL_NOF_LAYERS * sizeof(void*));
  // snrt_dma_start_1d(tch_ptr,  gl_soft_touched, (size_t)GL_NOF_LAYERS * sizeof(void*));
  // snrt_dma_wait_all();
  // 2) per-layer compare, one iteration
  for (unsigned m = 0; m < GL_NOF_LAYERS; ++m) {
    // Start timer
    timer = benchmark_get_cycle();
    update_variable_to_check(&ctx, m);
    asm volatile("fence" ::: "memory");
    timer1 = benchmark_get_cycle() - timer;
    update_check_to_variable_messages(&ctx, m);
    asm volatile("fence" ::: "memory");
    timer2 = benchmark_get_cycle() - timer - timer1;
    update_soft_bits(&ctx, m);
    asm volatile("fence" ::: "memory");
    timer3 = benchmark_get_cycle() - timer - timer1 - timer2;

   // unsigned off = off_l1[m], deg = deg_l1[m];

    //snrt_dma_start_1d(golden_c2v, c2v_ptr[m], (size_t)deg * Z);
    //snrt_dma_wait_all();
    //if (cmp("c2v", m, ctx.c2v + (size_t)off * Z, golden_c2v, deg * Z)) {
     // printf("STOP at layer %u (c2v)\n", m);
    //  failed = 1;
      //break;
    //}

    //snrt_dma_start_1d(touch_l1, tch_ptr[m], N_FULL);
    //snrt_dma_wait_all();

    // for (unsigned n = 0; n < N_FULL; ++n) {
    //   if (!touch_l1[n]) continue;
    //   snrt_dma_start_1d(gold_soft, soft_ptr[m] + (size_t)n * Z, Z);
    //   snrt_dma_wait_all();
    //   if (cmp("soft", m, ctx.soft + (size_t)n * Z, gold_soft, Z)) {
    //    // printf("STOP at layer %u (soft node %u)\n", m, n);
    //     failed = 1;
    //     //break;
    //   }
    // }
    // if (failed) {
    //   printf("layer %u NOT ok\n", m);
    // } else{
    //   printf("layer %u OK\n", m);
    // }
    printf("Update_variable_to_check took %u cycles.\n", timer1);
    printf("Update_check_to_variable_messages took %u cycles.\n", timer2);
    printf("Update_soft_bits took %u cycles.\n", timer3);
    printf("The whole iteration took %u cycles.\n", timer1 + timer2 + timer3);
  }
  //if (failed) printf("FAILED\n");
  //else printf("ALL LAYERS OK\n");

  timer = benchmark_get_cycle();
  bool valid = hard_decision_packed(hard, ctx.soft, nof_significant);
  asm volatile("fence" ::: "memory");
  timer_hard = benchmark_get_cycle() - timer;
  printf("hard_decision took %u cycles.\n", timer_hard);

  uint32_t crc_rem = crc_calculate(&crc_ctx, hard, nof_significant);
  asm volatile("fence" ::: "memory");
  unsigned timer_crc = benchmark_get_cycle() - timer;
  printf("crc took %u cycles, rem=0x%X, %s\n",
       timer_crc, (unsigned)crc_rem, crc_rem == 0 ? "CRC OK" : "CRC FAIL");

  int8_t *gold_msg = (int8_t*)snrt_l1alloc((size_t)GL_MSG_LEN);
  snrt_dma_start_1d(gold_msg, gl_golden_msg, GL_MSG_LEN);
  snrt_dma_wait_all();

  int bad = 0;
  for (unsigned i = 0; i < nof_significant && i < GL_MSG_LEN; i++) {
    int got_hard = (hard[i >> 3] >> (7 - (i & 7))) & 1;
    if (got_hard != gold_msg[i]) {
      printf("MSG mismatch idx=%u got=%d exp=%d\n", i, (int)got_hard, (int)gold_msg[i]);
      bad++;
    }
  }
  
  if (bad) printf("DECODE FAIL bad=%d\n", bad);
  else     printf("DECODE OK\n");
}
  snrt_cluster_hw_barrier();
  
  return 0;
}