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

#pragma once

#include <stddef.h>
#include <stdint.h>

// Elements (fp32) per dictionary entry. Fixed: 4 x fp32 = 16 B block.
#ifndef DICT_D
#define DICT_D 4
#endif

// Bytes per code; provided by the generated data header (DATAHEADER).
#ifndef DICT_CODE_BYTES
#define DICT_CODE_BYTES 2
#endif

#if DICT_CODE_BYTES == 1
typedef uint8_t dict_code_t;
#else
typedef uint16_t dict_code_t;
#endif

void dictdecode_vlxblk(float *out, const float *dict, const dict_code_t *codes,
                       unsigned int n_codes);
void dictdecode_rvv(float *out, const float *dict, const dict_code_t *codes,
                    unsigned int n_codes);
