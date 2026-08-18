// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef HPVQGEMV_H
#define HPVQGEMV_H

#include <stdint.h>

inline void vq_gemv_vlxblk(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                           const __fp16 *b_cb1, const uint8_t *b_idx0,
                           const uint8_t *b_idx1, const __fp16 *b_scales,
                           const unsigned int K, const unsigned int N,
                           const unsigned int group_start,
                           const unsigned int group_end)
    __attribute__((always_inline));

inline void vq_gemv_rvv(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                        const __fp16 *b_cb1, const uint8_t *b_idx0,
                        const uint8_t *b_idx1, const __fp16 *b_scales,
                        const unsigned int K, const unsigned int N,
                        const unsigned int group_start,
                        const unsigned int group_end)
    __attribute__((always_inline));

#endif
