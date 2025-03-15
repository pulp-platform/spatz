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

#ifndef MXFP8MATMUL_H
#define MXFP8MATMUL_H

#include <stdint.h>

// inner product: natural data layout

void mxfp8_matmul_fp32_inner_1x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K);

void mxfp8_matmul_fp32_inner_reducefirst_1x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K);

void mxfp8_matmul_fp32_inner_4x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K);

void mxfp8_matmul_fp32_inner_sdotp_4x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K);


// outer product: optimal data layout

void mxfp8_matmul_fp32_outer_lmul4_1x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K);

void mxfp8_matmul_fp32_outer_lmul4_2x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K);

void mxfp8_matmul_fp32_outer_lmul2_4x(float *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K);

#endif
