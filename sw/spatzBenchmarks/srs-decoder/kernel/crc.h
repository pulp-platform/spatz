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

// Author: Ihor Sachok <ihor.sachok@unibo.it>

#ifndef CRC_H
#define CRC_H
 
#include <stdint.h>
#include <stddef.h>
 
typedef struct {
    uint32_t table[256];
    unsigned order;
    uint32_t mask;
    uint32_t poly_low;
    uint32_t polynom;
} crc_t;
 
inline void crc_init(crc_t *crc, uint32_t polynom, unsigned order) __attribute__((always_inline));
 
inline uint32_t crc_calculate(const crc_t *crc, const uint8_t *data, size_t num_bits) __attribute__((always_inline));
 
#endif
 