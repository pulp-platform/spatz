// Copyright 2022 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>

#ifndef _DEMODULATE_SOFT_BPSK_
#define _DEMODULATE_SOFT_BPSK_
#include <stdbool.h>
#include <stdint.h>

inline void demodulate_soft_BPSK(int8_t *llrs, const float *symbols, const float *noise_vars, uint32_t nof_symbols)
    __attribute__((always_inline));
#endif
