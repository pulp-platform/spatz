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

// Author: Yichao Zhang, ETH Zurich
// Author: Enis Mustafa, ETH Zurich

#ifndef MXMATMUL_H
#define MXMATMUL_H

#include <stdint.h>

void matmul_tiled_Bx2(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int m_end, const unsigned int n_end, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) __attribute__((always_inline));

void matmul_tiled_Bx4(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int m_end, const unsigned int n_end, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) __attribute__((always_inline));

void matmul_tiled_Bx8(double *c, const double *a, const double *b,
                 const unsigned int kernel_m, const unsigned int kernel_n, const unsigned int kernel_k,
                 const unsigned int N, const unsigned int K, const unsigned int inner_loops,
                 const unsigned int m_start, const unsigned int m_end, const unsigned int n_end, const unsigned int vl,
                 const unsigned int nrelem_a, const unsigned int nrelem_b, const unsigned int nrelem_c) __attribute__((always_inline));

#endif
