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

// Author: Ihor Sachok <ihor.sachok@unibo.it>

#ifndef LDPC_DECODER_CTX_H
#define LDPC_DECODER_CTX_H

#include "ldpc_graph_tables.h"

typedef struct {
    uint16_t lifting_size; 
    uint16_t bg_M, bg_K, bg_N_short, bg_N_high_rate, max_deg;
    uint16_t message_length;
    uint16_t max_iterations;

    uint16_t *bg1_adjacency_l1;
    uint16_t *shift;
    uint16_t offset[BG1_M + 1];
    uint32_t total_edges;

    uint16_t codeblock_length;
    uint16_t nof_layers;
    uint16_t nof_significant_bits;

    int8_t*  soft;
    int8_t*  v2c;
    int8_t*  c2v;
    uint8_t* c2v_init;
    int8_t*  min;
    int8_t*  second;
    uint8_t* index;
    uint8_t* sign;
    int8_t*  min_dup;
    int8_t*  second_dup;
    uint8_t* index_dup;
    uint8_t* sign_dup;
    //uint16_t* vidx;
} ldpc_ctx_t;

#endif