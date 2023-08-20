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

// Author: Enis Mustafa, ETH Zurich

#ifndef MXMATMUL_H
#define MXMATMUL_H

#include <stdint.h>
void matmul_tiled_load_store_test(double *c, const double *a, const double *b,
                 const unsigned int dim) __attribute__((always_inline));
void matmul_tiled_mxmacc_test(double *c, const double *a, const double *b,
                 const unsigned int dim) __attribute__((always_inline));
#endif
