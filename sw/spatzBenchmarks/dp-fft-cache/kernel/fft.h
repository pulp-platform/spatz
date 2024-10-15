// Copyright 2021 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti, ETH Zurich

#include <stdint.h>
#include <string.h>

#ifndef _FFT_H_
#define _FFT_H_

// Single-core
inline void fft_sc(double *s, double *buf, const double *twi,
                   const uint16_t *seq_idx, const uint16_t *rev_idx,
                   const unsigned int nfft, const unsigned int log2_nfft,
                   const unsigned int cid) __attribute__((always_inline));

// Dual-core
inline void fft_2c(const double *s, double *buf, const double *twi,
                   const unsigned int nfft, const unsigned int cid)
    __attribute__((always_inline));

#endif
