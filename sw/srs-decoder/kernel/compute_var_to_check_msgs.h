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

#ifndef _COMPVTCMSGS_
#define _COMPVTCMSGS_
#include <stdbool.h>
#include <stdint.h>

inline void compute_var_to_check_msgs(int8_t *this_var_to_check, const int8_t *this_soft_bits, const int8_t *this_check_to_var, const uint32_t lifting_size)
    __attribute__((always_inline));
#endif
