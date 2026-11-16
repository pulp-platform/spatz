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

// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include DATAHEADER
#include "kernel/layer-map-precoding.c"
unsigned int timer;

uint16_t  *port_re;

static inline float bf16_to_f32(uint16_t x) {
    uint32_t bits = ((uint32_t)x) << 16;
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}


static inline int fp_check(uint16_t *port_re, const int8_t *a, const float *b) {
    const float threshold = 0.01f;
    unsigned nof_re = dotp_l.M / 2; // dotp_l.M = int8 samples per port (re+im), no layer factor

    for (int i_port = 0; i_port < nof_ports; i_port++) {
        for (int i_re = 0; i_re < nof_re; i_re++) {
            float gold_re = 0.0f;
            float gold_im = 0.0f;

            for (int l = 0; l < nof_layers; l++) {
                // layout: [RE][layer][re, im], stride nof_layers*2 per RE
                int base = i_re * nof_layers * 2 + l * 2;
                float in_re = (float)a[base];
                float in_im = (float)a[base + 1];

                // weights: [port][layer][re, im]
                float w_re = b[i_port * nof_layers * 2 + l * 2];
                float w_im = b[i_port * nof_layers * 2 + l * 2 + 1];

                gold_re += in_re * w_re - in_im * w_im;
                gold_im += in_re * w_im + in_im * w_re;
            }

            int out_idx = i_port * dotp_l.M + i_re * 2;
            float got_re = bf16_to_f32(port_re[out_idx]);
            float got_im = bf16_to_f32(port_re[out_idx + 1]);

            if (fabsf(got_re - gold_re) > threshold)
                printf("Error port %d re[%d]: got %f, gold %f\n", i_port, i_re, got_re, gold_re);
            if (fabsf(got_im - gold_im) > threshold)
                printf("Error port %d im[%d]: got %f, gold %f\n", i_port, i_re, got_im, gold_im);
        }
    }
    return 0;
}

int main() {
  //const unsigned int num_cores = snrt_cluster_core_num();
 // const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  timer = (unsigned int)-1;
  uint32_t num_cores = snrt_cluster_core_num();
  uint32_t cid = snrt_cluster_core_idx();
  const unsigned int dim = (dotp_l.M / num_cores)/2;
  // Start dump
  if (cid == 0)
    start_kernel();

  // Start timer
  if (cid == 0)
    timer = benchmark_get_cycle();
  snrt_cluster_hw_barrier();
  if (cid == 0) {
    port_re = (uint16_t  *)snrt_l1alloc(dotp_l.M * sizeof(uint16_t ));
  }

  snrt_cluster_hw_barrier();
  // Calculate dotp
  layer_map_precoding_v32b(input_re_in_dram, port_weights_in_dram, port_re);
  // Wait for all cores to finish

  snrt_cluster_hw_barrier();
  // End dump
  if (cid == 0)
    stop_kernel();

  // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  // Check and display results
  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * dotp_l.M / (timer);
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    printf("\n----- (%d) sp fdotp -----\n", dotp_l.M);
    //printf("The execution took %u cycles.\n", timer);
    printf("The memory allocation and data copy took %u cycles.\n", timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  if (cid == 0)
    if (fp_check(port_re, input_re_in_dram, port_weights_in_dram)) {
      return -1;
    }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}
