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

#ifndef _FDOTPROD_H_
#define _FDOTPROD_H_

inline double fdotp_v64b_m8(const double *a, const double *b, unsigned int avl)
    __attribute__((always_inline));
inline double fdotp_v64b_m8_unrl(const double *a, const double *b, unsigned int avl)
    __attribute__((always_inline));
inline float fdotp_v32b_m8(const float *a, const float *b, unsigned int avl)
    __attribute__((always_inline));
inline _Float16 fdotp_v16b_m8(const _Float16 *a, const _Float16 *b,
                           unsigned int avl) __attribute__((always_inline));

#endif
