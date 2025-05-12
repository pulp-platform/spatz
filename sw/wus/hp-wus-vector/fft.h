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
inline void fft_sc(__fp16 *s, __fp16 *buf, __fp16 *twi, __fp16 *out,
                   uint16_t *seq_idx, uint16_t *rev_idx,
                   const unsigned int nfft) __attribute__((always_inline));


#endif
