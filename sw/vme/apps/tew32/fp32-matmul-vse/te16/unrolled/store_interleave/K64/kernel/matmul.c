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
//
// Author: Pei-Yu Lin, ETH Zürich

// fp32 optimal kernel for TE=16, CE=8, and four accumulator tiles.

#include "matmul.h"

__attribute__((noinline, aligned(64))) void matmul_fp32(const float *At, const float *B, float *C, uint32_t M, uint32_t N, uint32_t K)
{
    const uint32_t tile_stride = TE * K;
    uintptr_t a0p = (uintptr_t)At;
    uintptr_t a1p = (uintptr_t)(At + tile_stride);
    uintptr_t b0p = (uintptr_t)B;
    uintptr_t b1p = (uintptr_t)(B + tile_stride);
    uintptr_t tss;
    const uintptr_t operand_bytes = sizeof(float) * TE;
    uintptr_t c00 = (uintptr_t)(C);
    uintptr_t c01 = c00 + operand_bytes;
    uintptr_t c10 = (uintptr_t)(C + TE * CHUNK_SIZE);
    uintptr_t c11 = (uintptr_t)(C + TE * CHUNK_SIZE + TE);
    uintptr_t loop_counter;
    uintptr_t vl_counter;
    const uintptr_t c_stride = CHUNK_SIZE * sizeof(float);
    const uintptr_t matrix_mtype = (TE << 10) | (2u << 5) | 1u;
    const uintptr_t matrix_vtype = 0xD0;  // e32, m1, ta, ma

    asm volatile(

        // Block 1, K-Group 1
        "vsetvli %[vl], x0, e32, m8, ta, ma\n"
        "msetmtype %[mtype], %[vtype]\n"  // set sew=32, set tm=TE, tk=1, twiden=1
        "msettn x0, %[vl]\n"              // msetmtype resets tn, so set full tn=TE

        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtzero mt0\n"
        
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtzero mt4\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtzero mt8\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtzero mt12\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 1, K-Group 2
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 1, K-Group 3
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 1, K-Group 4
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 1, K-Group 5
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 1, K-Group 6
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vle32.v v8,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // Block 1, K-Group 7+8
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v9,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v11,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v12,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v14,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vle32.v v15,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vle32.v v17,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vle32.v v18,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vle32.v v20,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vle32.v v21,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vle32.v v23,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v15, v31\n"
        
        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vse32.v v0,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v1, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vse32.v v1,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vse32.v v2,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v3, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vse32.v v3,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vse32.v v4,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v5, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vse32.v v5,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v7, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vse32.v v6,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vse32.v v7,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v0,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v3,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vse32.v v8,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v9, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vse32.v v9,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vse32.v v10,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v11, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vse32.v v11,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v13, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v15, v31\n"
        "vse32.v v13,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v14, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v15, %[tss]\n"  "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n" // mt4
        "vse32.v v14,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vse32.v v15,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtmv.v.t v16, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vse32.v v16,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v17, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"
        "vse32.v v17,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v18, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vse32.v v18,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v19, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v5,  v21\n"
        "vse32.v v19,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v20, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vse32.v v20,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v21, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v7,  v23\n"
        "vse32.v v21,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v22, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vse32.v v22,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v23, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v23,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtmv.v.t v24, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vse32.v v24,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v25, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vse32.v v25,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v26, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vse32.v v26,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v27, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vse32.v v27,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v28, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vse32.v v28,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v29, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vse32.v v29,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v30, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v31, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0,  v16\n"
        "vse32.v v30,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vse32.v v31,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtmv.v.t v16, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vse32.v v16,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v17, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"
        "vse32.v v17,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v18, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vse32.v v18,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v19, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v5,  v21\n"
        "vse32.v v19,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v20, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vse32.v v20,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v21, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v7,  v23\n"
        "vse32.v v21,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v22, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vse32.v v22,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v23, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v23,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtmv.v.t v24, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vse32.v v24,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v25, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"
        "vse32.v v25,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v26, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vse32.v v26,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v27, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"
        "vse32.v v27,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v28, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vse32.v v28,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v29, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vse32.v v29,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v30, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v31, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v30,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vse32.v v31,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        
        "sub %[loop], %[b1p], %[b0p]\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "sub %[a1p], %[a1p], %[loop]\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "add %[b1p], %[b1p], %[loop]\n"
        
        // Block 2, K-Group 1
        // vtfmm mt0
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "lui %[tss], 0x40000\n"  // mt8
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vse32.v v8,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v9, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vse32.v v9,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vse32.v v10,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtmv.v.t v11, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vse32.v v11,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vse32.v v12,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v13, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v7, v23\n"
        "vse32.v v13,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"

        // vtfmm mt4
        "vtzero mt4\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtmv.v.t v14, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vse32.v v14,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v15, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vse32.v v15,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vse32.v v0,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v1, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vse32.v v1,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vse32.v v2,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v3, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v31\n"
        "vse32.v v3,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"

        // vtfmm mt12
        "vtzero mt12\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vse32.v v4,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v5, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vse32.v v5,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vse32.v v6,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v7, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vse32.v v7,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15,  v31\n"

        // vtfmm mt8
        "vtzero mt8\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "sub %[c00], %[c00], %[loop]\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "sub %[c01], %[c01], %[loop]\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vtfmm.tvv mt8, v13, v21\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15, v23\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        
        // Block 2, K-Group 2
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 2, K-Group 3
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 2, K-Group 4
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 2, K-Group 5
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 2, K-Group 6
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vle32.v v8,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // Block 2, K-Group 7+8
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v9,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v11,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v12,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v14,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vle32.v v15,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vle32.v v17,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vle32.v v18,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vle32.v v20,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vle32.v v21,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vle32.v v23,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v15, v31\n"
        
        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vse32.v v0,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v1, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vse32.v v1,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vse32.v v2,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v3, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vse32.v v3,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vse32.v v4,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v5, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vse32.v v5,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v7, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vse32.v v6,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vse32.v v7,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v0,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v3,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vse32.v v8,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v9, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vse32.v v9,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vse32.v v10,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v11, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vse32.v v11,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v13, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v15, v31\n"
        "vse32.v v13,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v14, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v15, %[tss]\n"  "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n" // mt4
        "vse32.v v14,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vse32.v v15,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtmv.v.t v16, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vse32.v v16,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v17, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"
        "vse32.v v17,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v18, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vse32.v v18,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v19, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v5,  v21\n"
        "vse32.v v19,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v20, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vse32.v v20,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v21, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v7,  v23\n"
        "vse32.v v21,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v22, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vse32.v v22,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v23, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v23,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtmv.v.t v24, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vse32.v v24,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v25, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vse32.v v25,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v26, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vse32.v v26,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v27, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vse32.v v27,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v28, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vse32.v v28,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v29, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vse32.v v29,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v30, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v31, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0,  v16\n"
        "vse32.v v30,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vse32.v v31,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtmv.v.t v16, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vse32.v v16,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v17, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"
        "vse32.v v17,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v18, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vse32.v v18,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v19, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v5,  v21\n"
        "vse32.v v19,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v20, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vse32.v v20,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v21, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v7,  v23\n"
        "vse32.v v21,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v22, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vse32.v v22,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v23, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v23,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtmv.v.t v24, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vse32.v v24,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v25, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"
        "vse32.v v25,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v26, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vse32.v v26,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v27, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"
        "vse32.v v27,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v28, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vse32.v v28,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v29, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vse32.v v29,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v30, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v31, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v30,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vse32.v v31,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"

        "sub %[loop], %[b1p], %[b0p]\n"
        "add %[a0p], %[a0p], %[loop]\n"
        "add %[a1p], %[a1p], %[loop]\n"
        "sub %[b0p], %[b0p], %[loop]\n"
        "sub %[b1p], %[b1p], %[loop]\n"
        "sub %[b0p], %[b0p], %[loop]\n"
        "sub %[b1p], %[b1p], %[loop]\n"
        "sub %[b0p], %[b0p], %[loop]\n"
        "sub %[b1p], %[b1p], %[loop]\n"

        // Block 3, K-Group 1
        // vtfmm mt0
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "lui %[tss], 0x40000\n"  // mt8
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vse32.v v8,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v9, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vse32.v v9,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vse32.v v10,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtmv.v.t v11, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vse32.v v11,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vse32.v v12,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v13, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v7, v23\n"
        "vse32.v v13,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"

        // vtfmm mt4
        "vtzero mt4\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtmv.v.t v14, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vse32.v v14,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v15, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vse32.v v15,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vse32.v v0,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v1, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vse32.v v1,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vse32.v v2,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v3, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v31\n"
        "vse32.v v3,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"

        // vtfmm mt12
        "vtzero mt12\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vse32.v v4,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v5, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vse32.v v5,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vse32.v v6,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v7, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vse32.v v7,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15,  v31\n"

        // vtfmm mt8
        "vtzero mt8\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "add %[c00], %[c00], %[loop]\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "add %[c01], %[c01], %[loop]\n"
        "add %[c10], %[c10], %[loop]\n"
        "add %[c11], %[c11], %[loop]\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13, v21\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15, v23\n"
        
        // Block 3, K-Group 2
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 3, K-Group 3
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 3, K-Group 4
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 3, K-Group 5
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // K-Group 6
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vle32.v v8,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // Block 3, K-Group 7+8
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v9,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v11,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v12,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v14,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vle32.v v15,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vle32.v v17,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vle32.v v18,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vle32.v v20,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vle32.v v21,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vle32.v v23,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v15, v31\n"
        
        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vse32.v v0,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v1, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vse32.v v1,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vse32.v v2,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v3, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vse32.v v3,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vse32.v v4,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v5, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vse32.v v5,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v7, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vse32.v v6,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vse32.v v7,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v0,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v3,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vse32.v v8,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v9, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vse32.v v9,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vse32.v v10,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v11, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vse32.v v11,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v13, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v15, v31\n"
        "vse32.v v13,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v14, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v15, %[tss]\n"  "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n" // mt4
        "vse32.v v14,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vse32.v v15,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtmv.v.t v16, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vse32.v v16,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v17, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"
        "vse32.v v17,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v18, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vse32.v v18,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v19, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v5,  v21\n"
        "vse32.v v19,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v20, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vse32.v v20,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v21, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v7,  v23\n"
        "vse32.v v21,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v22, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vse32.v v22,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v23, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v23,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtmv.v.t v24, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vse32.v v24,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v25, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vse32.v v25,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v26, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vse32.v v26,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v27, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vse32.v v27,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v28, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vse32.v v28,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v29, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vse32.v v29,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v30, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v31, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0,  v16\n"
        "vse32.v v30,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vse32.v v31,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtmv.v.t v16, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vse32.v v16,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v17, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"
        "vse32.v v17,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v18, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vse32.v v18,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v19, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v5,  v21\n"
        "vse32.v v19,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v20, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vse32.v v20,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v21, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v7,  v23\n"
        "vse32.v v21,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v22, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vse32.v v22,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v23, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v23,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtmv.v.t v24, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vse32.v v24,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v25, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"
        "vse32.v v25,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v26, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vse32.v v26,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v27, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"
        "vse32.v v27,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v28, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vse32.v v28,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v29, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vse32.v v29,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v30, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v31, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v30,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vse32.v v31,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"

        "sub %[loop], %[b1p], %[b0p]\n"
        "sub %[a0p], %[a0p], %[loop]\n"
        "sub %[a1p], %[a1p], %[loop]\n"
        "add %[b0p], %[b0p], %[loop]\n"
        "add %[b1p], %[b1p], %[loop]\n"

        // Block 4, K-Group 1
        // vtfmm mt0
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "lui %[tss], 0x40000\n"  // mt8
        "vtzero mt0\n"
        "vtfmm.tvv mt0, v0, v16\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v1, v17\n"
        "vse32.v v8,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v9, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v2, v18\n"
        "vse32.v v9,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v3, v19\n"
        "vse32.v v10,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"
        "vtmv.v.t v11, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v5, v21\n"
        "vse32.v v11,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v6, v22\n"
        "vse32.v v12,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v13, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt0, v7, v23\n"
        "vse32.v v13,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"

        // vtfmm mt4
        "vtzero mt4\n"
        "vtfmm.tvv mt4, v0, v24\n"
        "vtmv.v.t v14, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v1, v25\n"
        "vse32.v v14,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v15, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v26\n"
        "vse32.v v15,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v28\n"
        "vse32.v v0,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v1, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v29\n"
        "vse32.v v1,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v30\n"
        "vse32.v v2,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v3, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v31\n"
        "vse32.v v3,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"

        // vtfmm mt12
        "vtzero mt12\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vse32.v v4,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v5, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vse32.v v5,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vse32.v v6,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v7, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vse32.v v7,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15,  v31\n"

        // vtfmm mt8
        "vtzero mt8\n"
        "vtfmm.tvv mt8, v8,  v16\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10, v18\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11, v19\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12, v20\n"
        "slli %[loop], %[c_stride], 4\n"
        "addi %[loop], %[loop], -128\n"
        "sub %[c00], %[c00], %[loop]\n"
        "vtfmm.tvv mt8, v13, v21\n"
        "sub %[c01], %[c01], %[loop]\n"
        "sub %[c10], %[c10], %[loop]\n"
        "sub %[c11], %[c11], %[loop]\n"
        "vtfmm.tvv mt8, v14, v22\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15, v23\n"
        
        // Block 4, K-Group 2
        // vtfmm mt0
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 4, K-Group 3
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 4, K-Group 4
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 4, K-Group 5
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"

        // Block 4, K-Group 6
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"

        // vtfmm mt4
        "vtfmm.tvv mt4, v0, v24\n"  // mt4 += v0*v24
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v25\n"  // mt4 += v1*v25
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v2, v26\n"  // mt4 += v2*v26
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v3, v27\n"  // mt4 += v3*v27
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v4, v28\n"  // mt4 += v4*v28
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v5, v29\n"  // mt4 += v5*v29
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v6, v30\n"  // mt4 += v6*v30
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v7, v31\n"  // mt4 += v7*v31
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        
        // vtfmm mt8
        "vtfmm.tvv mt8, v8,  v16\n"  // mt8 += v8*v16
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v9,  v17\n"  // mt8 += v9*v17
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v10,  v18\n"  // mt8 += v10*v18
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v11,  v19\n"  // mt8 += v11*v19
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v12,  v20\n"  // mt8 += v12*v20
        "vle32.v v0,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v13,  v21\n"  // mt8 += v13*v21
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v14,  v22\n"  // mt8 += v14*v22
        "vle32.v v1,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v15,  v23\n"  // mt8 += v15*v23
        "vle32.v v2,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // vtfmm mt12
        "vtfmm.tvv mt12, v8,  v24\n"
        "vle32.v v3,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v10, v26\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v11, v27\n"
        "vle32.v v5,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v12, v28\n"
        "vle32.v v6,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v13, v29\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v14, v30\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v15, v31\n"
        "vle32.v v8,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"

        // Block 4, K-Group 7+8
        // vtfmm mt0
        "vtfmm.tvv mt0, v0, v16\n"  // mt0 += v0*v16
        "vle32.v v9,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v1, v17\n"  // mt0 += v1*v17
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v2, v18\n"  // mt0 += v2*v18
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v3, v19\n"  // mt0 += v3*v19
        "vle32.v v11,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v4, v20\n"  // mt0 += v4*v20
        "vle32.v v12,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v5, v21\n"  // mt0 += v5*v21
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v6, v22\n"  // mt0 += v6*v22
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v7, v23\n"  // mt0 += v7*v23
        "vle32.v v14,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v8, v24\n"
        "vle32.v v15,  (%[a0p])\n" "add %[a0p], %[a0p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v9, v25\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v16,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v10, v26\n"
        "vle32.v v17,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v11, v27\n"
        "vle32.v v18,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v12, v28\n"
        "vle32.v v20,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v13, v29\n"
        "vle32.v v21,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v14, v30\n"
        "vle32.v v23,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt0, v15, v31\n"
        
        // vtfmm mt4  
        // vtse mt0      
        "mv %[tss], x0\n" // mt0
        "vtfmm.tvv mt4, v0, v16\n"
        "vle32.v v24,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b1p])\n" "add %[b1p], %[b1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v1, v17\n"
        "vtmv.v.t v0, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v2, v18\n"
        "vse32.v v0,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v1, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v3, v19\n"
        "vse32.v v1,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v4, v20\n"
        "vse32.v v2,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v3, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v5, v21\n"
        "vse32.v v3,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v4, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v6, v22\n"
        "vse32.v v4,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v5, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v7, v23\n"
        "vse32.v v5,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v7, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v8, v24\n"
        "vse32.v v6,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vse32.v v7,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v0,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v1,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v2,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v3,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v4,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v5,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v6,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v7,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt4, v9, v25\n"
        "vtmv.v.t v8, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v10, v26\n"
        "vse32.v v8,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v9, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v11, v27\n"
        "vse32.v v9,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v12, v28\n"
        "vse32.v v10,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v11, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v13, v29\n"
        "vse32.v v11,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v14, v30\n"  
        "vse32.v v12,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v13, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt4, v15, v31\n"
        "vse32.v v13,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vtmv.v.t v14, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v15, %[tss]\n"  "addi %[tss], %[tss], 1\n"

        // vtfmm mt12
        // vtse mt4
        "lui %[tss], 0x20000\n" // mt4
        "vse32.v v14,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vse32.v v15,  (%[c00])\n" "add %[c00], %[c00], %[c_stride]\n"
        "vle32.v v8,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v9,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v10,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v11,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v12,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v13,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v14,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vle32.v v15,  (%[a1p])\n" "add %[a1p], %[a1p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v0,  v16\n"
        "vtfmm.tvv mt12, v1,  v17\n"
        "vtmv.v.t v16, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v2,  v18\n"
        "vse32.v v16,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v17, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v3,  v19\n"
        "vse32.v v17,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v18, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v4,  v20\n"
        "vse32.v v18,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v19, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v5,  v21\n"
        "vse32.v v19,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v20, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v6,  v22\n"
        "vse32.v v20,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v21, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v7,  v23\n"
        "vse32.v v21,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v22, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v8,  v24\n"
        "vse32.v v22,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v23, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v23,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "addi %[b0p], %[b0p], -1024\n"
        "vle32.v v16,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v17,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v18,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v19,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v20,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v21,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v22,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v23,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt12, v9,  v25\n"
        "vtmv.v.t v24, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v10,  v26\n"
        "vse32.v v24,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v25, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v11,  v27\n"
        "vse32.v v25,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v26, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v12,  v28\n"
        "vse32.v v26,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v27, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v13,  v29\n"
        "vse32.v v27,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v28, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v14,  v30\n"
        "vse32.v v28,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v29, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt12, v15,  v31\n"
        "vse32.v v29,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vtmv.v.t v30, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v31, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        
        // vtfmm mt8
        // vtse mt12
        "lui %[tss], 0x60000\n" // mt12
        "vtfmm.tvv mt8, v0,  v16\n"
        "vse32.v v30,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vse32.v v31,  (%[c01])\n" "add %[c01], %[c01], %[c_stride]\n"
        "vle32.v v24,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v25,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v26,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v27,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v28,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v29,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v30,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vle32.v v31,  (%[b0p])\n" "add %[b0p], %[b0p], %[operand_bytes]\n"
        "vtfmm.tvv mt8, v1,  v17\n"
        "vtmv.v.t v16, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v2,  v18\n"
        "vse32.v v16,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v17, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v3,  v19\n"
        "vse32.v v17,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v18, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v4,  v20\n"
        "vse32.v v18,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v19, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v5,  v21\n"
        "vse32.v v19,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v20, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v6,  v22\n"
        "vse32.v v20,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v21, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v7,  v23\n"
        "vse32.v v21,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v22, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v8,  v24\n"
        "vse32.v v22,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v23, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v23,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtfmm.tvv mt8, v9,  v25\n"
        "vtmv.v.t v24, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v10,  v26\n"
        "vse32.v v24,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v25, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v11,  v27\n"
        "vse32.v v25,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v26, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v12,  v28\n"
        "vse32.v v26,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v27, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v13,  v29\n"
        "vse32.v v27,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v28, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v14,  v30\n"
        "vse32.v v28,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v29, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtfmm.tvv mt8, v15,  v31\n"
        "vse32.v v29,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vtmv.v.t v30, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v31, %[tss]\n"  "addi %[tss], %[tss], 1\n"
        "vse32.v v30,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"
        "vse32.v v31,  (%[c11])\n" "add %[c11], %[c11], %[c_stride]\n"

        // Final block: compact 16-row mt8 store loop.
        "lui %[tss], 0x40000\n"
        "vtmv.v.t v0, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v1, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vse32.v v0,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vse32.v v1,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v2, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v3, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vse32.v v2,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vse32.v v3,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v4, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v5, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vse32.v v4,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vse32.v v5,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v6, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v7, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vse32.v v6,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vse32.v v7,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"

        "vtmv.v.t v8, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v9, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vse32.v v8,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vse32.v v9,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v10, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v11, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vse32.v v10,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vse32.v v11,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v12, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v13, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vse32.v v12,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vse32.v v13,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vtmv.v.t v14, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vtmv.v.t v15, %[tss]\n" "addi %[tss], %[tss], 1\n"
        "vse32.v v14,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        "vse32.v v15,  (%[c10])\n" "add %[c10], %[c10], %[c_stride]\n"
        :
          [a0p] "+r"(a0p),
          [a1p] "+r"(a1p),
          [b0p] "+r"(b0p),
          [b1p] "+r"(b1p),
          [tss] "=&r"(tss),
          [c00] "+r"(c00),
          [c01] "+r"(c01),
          [c10] "+r"(c10),
          [c11] "+r"(c11),
          [loop] "=&r"(loop_counter),
          [vl] "=&r"(vl_counter)
        :
          [c_stride] "r"(c_stride),
          [operand_bytes] "r"(operand_bytes),
          [mtype] "r"(matrix_mtype),
          [vtype] "r"(matrix_vtype)
        : "memory");
}
