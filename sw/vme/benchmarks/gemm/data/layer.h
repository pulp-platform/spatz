// Copyright 2024 ETH Zurich and University of Bologna.
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
//
// Author: Pei-Yu Lin <peilin@ethz.ch>

#pragma once

#include <stdint.h>

/**
 * @brief VME GEMM layer configuration.
 *
 * Describes a C = A * B problem with:
 *   A : M × K  (stored row-major; kernel receives pre-transposed At : K × M)
 *   B : K × N  (stored row-major)
 *   C : M × N  (fp32 accumulator, tew=32)
 *
 * TM / TN are the micro-tile sizes used by the 2×2 blocked VME kernel.
 * Typical values: TM = TN = 16.
 */
typedef struct {
    uint32_t M;   /**< Rows of A / rows of C                */
    uint32_t N;   /**< Columns of B / columns of C          */
    uint32_t K;   /**< Inner (reduction) dimension           */
    uint32_t TM;  /**< Micro-tile height (M direction)       */
    uint32_t TN;  /**< Micro-tile width  (N direction)       */
} vme_gemm_layer;
