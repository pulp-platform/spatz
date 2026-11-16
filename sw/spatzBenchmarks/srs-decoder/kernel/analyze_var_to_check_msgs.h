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

#ifndef _ANALIZEVTCMSGS_
#define _ANALIZEVTCMSGS_
#include <stdbool.h>
#include <stdint.h>

inline void analyze_var_to_check_msgs(int8_t *min_var_to_check, int8_t *second_min_var_to_check, uint8_t *min_var_to_check_index, uint8_t *sign_prod_var_to_check, const int8_t *rotated_node, const uint32_t var_node, const uint32_t lifting_size)
    __attribute__((always_inline));
#endif
