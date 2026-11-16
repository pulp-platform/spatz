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

#ifndef _COMPCHTVMSGS_
#define _COMPCHTVMSGS_
#include <stdbool.h>
#include <stdint.h>

inline void compute_check_to_var_msgs(int8_t        *this_check_to_var,
                               int8_t  const *this_var_to_check,
                               int8_t  const *min_var_to_check_dup,
                               int8_t  const *second_min_var_to_check_dup,
                               uint8_t const *min_var_to_check_index_dup,
                               uint8_t const *sign_prod_var_to_check_dup,
                               uint32_t       shift,
                               uint32_t       var_node,
                               uint32_t const lifting_size)
                                   __attribute__((always_inline));
#endif
