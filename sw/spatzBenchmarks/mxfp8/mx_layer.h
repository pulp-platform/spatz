// Copyright 2025 ETH Zurich and University of Bologna.
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

// Author: Max Wipfli <mwipfli@ethz.ch>

#pragma once

#include <stdint.h>

#define MX_BLOCK_SIZE 32

#define E8M0_BIAS 127
#define FP32_BIAS 127
#define BF16_BIAS 127

#define FCSR_MODE_DST     (1 << 8)
#define FCSR_MODE_SRC     (1 << 9)
#define FCSR_MODE_SRC_FP4 (1 << 11)

typedef enum { FP4 = 128, E8M0 = 64, FP16ALT = 32, FP8ALT = 16, FP64 = 8, FP32 = 4, FP16 = 2, FP8 = 1 } precision_t;

typedef struct mx_matmul_layer_struct {
  uint32_t M;
  uint32_t N;
  uint32_t K;
  precision_t dtype_elements;
  precision_t dtype_scales;
  precision_t dtype_results;
} mx_matmul_layer;

typedef struct mx_dotp_layer_struct {
  uint32_t N;
  precision_t dtype_elements;
  precision_t dtype_scales;
  precision_t dtype_results;
} mx_dotp_layer;
