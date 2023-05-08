// Copyright 2020 ETH Zurich and University of Bologna.
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

// Author: Matteo Perotti

#ifndef _FCONV2D_H_
#define _FCONV2D_H_

#include <stdint.h>

void conv3d_CHx7x7(double *o, const double *i, const double *f,
                   const unsigned int num_rows, const unsigned int M,
                   const unsigned int N, const unsigned int F)
    __attribute__((always_inline));
void conv3d_CHx7x7_block(double *o, const double *i, unsigned int num_rows,
                         const double *f, const unsigned int n_,
                         const unsigned int M, const unsigned int N,
                         const unsigned int F, const unsigned int C)
    __attribute__((always_inline));

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#endif
