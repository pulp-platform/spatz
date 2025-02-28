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

// MXFP8 matrix multiplication
// ---------------------------
// - block size = 32 fixed
// - a is an MxK matrix (row-major)
// - b is an KxN matrix (column-major)
// - c is an MxN matrix (row-major)
// - a_scale is an Mx(K/32) (row-major)
// - b_scale is an (K/32)xN (row-major)
// - element data format:     FP4
// - scale data format:       E8M0
// - accumulator data format: BF16 (FP16ALT)

void mxfp4_matmul_bf16_mxdotp_lmul1_8x(_Float16 *c,
    const char *a, const char *b, const char *a_scale, const char *b_scale,
    const uint32_t M, const uint32_t N, const uint32_t K);
