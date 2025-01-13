// Copyright 2025 ETH Zurich and University of Bologna.
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

// Author: Diyou Shen <dishen@iis.ee.ethz.ch>

#include <stdlib.h>
#include "../data/layer.h"

inline node_t* createNode (int id, const double data) __attribute__((always_inline));

inline int insertNode (node_t** head, node_t* newNode)  __attribute__((always_inline));

inline int deleteNode (node_t** head, int id) __attribute__((always_inline));

inline int traverseList (node_t* head) __attribute__((always_inline));

inline int traverseList_vec (node_t* head, uint32_t num_nodes, uint32_t num_lists) __attribute__((always_inline));
