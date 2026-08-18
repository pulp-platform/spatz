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

// Author: Bowen Wang, ETH Zurich

#ifndef HPVQMATMULMATERIALIZED_H
#define HPVQMATMULMATERIALIZED_H

#include <stdint.h>

inline void vq_dequantize_rvv_materialized(__fp16 *b, const __fp16 *b_cb0,
                                                const __fp16 *b_cb1,
                                                const uint8_t *b_idx0,
                                                const uint8_t *b_idx1,
                                                const __fp16 *b_scales,
                                                const unsigned int k_start,
                                                const unsigned int k_end,
                                                const unsigned int N)
    __attribute__((always_inline));

inline void vq_dequantize_vlxblk_materialized(__fp16 *b, const __fp16 *b_cb0,
                                              const __fp16 *b_cb1,
                                              const uint8_t *b_idx0,
                                              const uint8_t *b_idx1,
                                              const __fp16 *b_scales,
                                              const unsigned int k_start,
                                              const unsigned int k_end,
                                              const unsigned int N)
    __attribute__((always_inline));

inline void vq_dequantize_vle_materialized(__fp16 *b, const __fp16 *b_cb0,
                                           const __fp16 *b_cb1,
                                           const uint8_t *b_idx0,
                                           const uint8_t *b_idx1,
                                           const __fp16 *b_scales,
                                           const unsigned int k_start,
                                           const unsigned int k_end,
                                           const unsigned int N)
    __attribute__((always_inline));

inline void vq_dense_matmul_materialized(__fp16 *c, const __fp16 *a,
                                         const __fp16 *b,
                                         const unsigned int m_start,
                                         const unsigned int m_end,
                                         const unsigned int N,
                                         const unsigned int P,
                                         const unsigned int p_start,
                                         const unsigned int p_end)
    __attribute__((always_inline));

inline void vq_dense_gemv_materialized(__fp16 *c, const __fp16 *a,
                                       const __fp16 *b,
                                       const unsigned int K,
                                       const unsigned int N,
                                       const unsigned int n_start,
                                       const unsigned int n_end)
    __attribute__((always_inline));

#endif
