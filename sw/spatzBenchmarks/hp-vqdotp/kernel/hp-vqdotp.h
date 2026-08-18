// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef HPVQDOTP_H
#define HPVQDOTP_H

#include <stdint.h>

inline void vqdotp_rvv(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                       const __fp16 *b_cb1, const uint8_t *b_idx0,
                       const uint8_t *b_idx1, const __fp16 *b_scales,
                       const unsigned int K, const unsigned int N,
                       const unsigned int n_start,
                       const unsigned int n_end)
    __attribute__((always_inline));

inline void vqdotp_vlxblk(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                          const __fp16 *b_cb1, const uint8_t *b_idx0,
                          const uint8_t *b_idx1, const __fp16 *b_scales,
                          const unsigned int K, const unsigned int N,
                          const unsigned int n_start,
                          const unsigned int n_end)
    __attribute__((always_inline));

#endif
