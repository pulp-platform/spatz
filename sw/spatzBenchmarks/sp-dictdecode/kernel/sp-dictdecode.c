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

// Database dictionary decompression:
//   out[i*D .. i*D+D-1] = dict[code[i]*D .. code[i]*D+D-1],  D = 4 x fp32
//
// Pure indexed-block-copy engine test (structurally VQ decode without
// arithmetic). Two variants:
//   - dictdecode_vlxblk: custom VLXBLK indexed block loads (one index per
//     16 B block, blocks fetched on the unit-stride fast path).
//   - dictdecode_rvv: best-effort plain-RVV baseline. Spatz has no
//     VRGATHER/VRGATHEREI16 (verified against the vpu decoder,
//     spatz_decoder.sv), so per-element index replication in registers is
//     impossible. Instead the decode is column-decomposed: the byte offset
//     of each code's block (code * 16, fits in 16 bit for K*16 <= 64 KiB)
//     is built once per chunk with vsll at e16, then reused by four
//     vluxei16 gathers (one per record lane, base pointer absorbs the
//     +4*j lane offset) whose results are written back with four strided
//     stores (stride = 16 B).

#include "sp-dictdecode.h"

#if DICT_D != 4
#error "sp-dictdecode kernels are specialized for D=4 (16 B blocks)"
#endif

// Index EEW suffix of the VLXBLK instruction, matching the code width.
#if DICT_CODE_BYTES == 1
#define DICT_CODE_EI "8"
#else
#define DICT_CODE_EI "16"
#endif

void dictdecode_vlxblk(float *out, const float *dict, const dict_code_t *codes,
                       unsigned int n_codes) {
  // Block length in elements (power of two). Hoisted out of the chunk loop.
  asm volatile("vsetblklen %0" ::"r"(DICT_D));

  // Total element count; always a multiple of D, and every gvl returned at
  // e32/m8 (VLMAX = 128) keeps the remainder a multiple of D, so the VLXBLK
  // "vl multiple of blk_len" constraint holds for every chunk. m8 amortizes
  // the per-instruction VLSU setup cost over 512 B per gather.
  unsigned int rem = n_codes * DICT_D;

  // Steady state: 4 full m8 chunks (128 codes, 2 KiB out) per iteration.
  // One vreg holds exactly one chunk's indices (32 x e16 = 64 B, or
  // 32 x e8), so v4..v7 feed four gathers; two m8 data groups (v8/v16)
  // ping-pong, which is free on the single in-order VLSU.
  while (rem >= 4 * 128) {
#if DICT_CODE_BYTES == 1
    asm volatile("vsetvli zero, %[n_idx], e8, m1, ta, ma\n"
                 "vle8.v v4, (%[c0])\n"
                 "vle8.v v5, (%[c1])\n"
                 "vle8.v v6, (%[c2])\n"
                 "vle8.v v7, (%[c3])\n"
                 :
                 : [n_idx] "r"(32), [c0] "r"(codes), [c1] "r"(codes + 32),
                   [c2] "r"(codes + 64), [c3] "r"(codes + 96)
                 : "v4", "v5", "v6", "v7", "memory");
#else
    asm volatile("vsetvli zero, %[n_idx], e16, m4, ta, ma\n"
                 "vle16.v v4, (%[c0])\n"
                 :
                 : [n_idx] "r"(128), [c0] "r"(codes)
                 : "v4", "v5", "v6", "v7", "memory");
#endif
    asm volatile("vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei" DICT_CODE_EI ".v v8, (%[dict]), v4\n"
                 "vse32.v v8, (%[o0])\n"
                 "vlxblkei" DICT_CODE_EI ".v v16, (%[dict]), v5\n"
                 "vse32.v v16, (%[o1])\n"
                 "vlxblkei" DICT_CODE_EI ".v v8, (%[dict]), v6\n"
                 "vse32.v v8, (%[o2])\n"
                 "vlxblkei" DICT_CODE_EI ".v v16, (%[dict]), v7\n"
                 "vse32.v v16, (%[o3])\n"
                 :
                 : [gvl] "r"(128), [dict] "r"(dict), [o0] "r"(out),
                   [o1] "r"(out + 128), [o2] "r"(out + 256),
                   [o3] "r"(out + 384)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "memory");

    codes += 128;
    out += 512;
    rem -= 512;
  }

  while (rem > 0) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[rem], e32, m8, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [rem] "r"(rem));
    const size_t n_idx = gvl >> 2;

#if DICT_CODE_BYTES == 1
    asm volatile("vsetvli zero, %[n_idx], e8, m1, ta, ma\n"
                 "vle8.v v28, (%[codes])\n"
                 "vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei8.v v16, (%[dict]), v28\n"
                 "vse32.v v16, (%[out])\n"
                 :
                 : [n_idx] "r"(n_idx), [gvl] "r"(gvl), [codes] "r"(codes),
                   [dict] "r"(dict), [out] "r"(out)
                 : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "v28", "memory");
#else
    asm volatile("vsetvli zero, %[n_idx], e16, m1, ta, ma\n"
                 "vle16.v v28, (%[codes])\n"
                 "vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei16.v v16, (%[dict]), v28\n"
                 "vse32.v v16, (%[out])\n"
                 :
                 : [n_idx] "r"(n_idx), [gvl] "r"(gvl), [codes] "r"(codes),
                   [dict] "r"(dict), [out] "r"(out)
                 : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "v28", "memory");
#endif

    codes += n_idx;
    out += gvl;
    rem -= gvl;
  }
}

void dictdecode_rvv(float *out, const float *dict, const dict_code_t *codes,
                    unsigned int n_codes) {
  // Byte stride between consecutive records in the output column.
  const unsigned int stride_b = DICT_D * sizeof(float); // 16 B

  while (n_codes > 0) {
    // Chunk size = codes per gather column (VLMAX = 128 at e32/m8).
    size_t c;
    asm volatile("vsetvli %[c], %[n], e32, m8, ta, ma"
                 : [c] "=r"(c)
                 : [n] "r"(n_codes));

    // Build 16-bit byte offsets: offs[i] = code[i] * 16. Requires
    // K * 16 <= 64 KiB (checked at compile time in main.c).
#if DICT_CODE_BYTES == 1
    asm volatile("vsetvli zero, %[c], e8, m2, ta, ma\n"
                 "vle8.v v2, (%[codes])\n"
                 "vwaddu.vx v4, v2, zero\n" // zero-extend u8 -> u16
                 "vsetvli zero, %[c], e16, m4, ta, ma\n"
                 "vsll.vi v4, v4, 4\n" // * 16 B per entry
                 :
                 : [c] "r"(c), [codes] "r"(codes)
                 : "v2", "v3", "v4", "v5", "v6", "v7", "memory");
#else
    asm volatile("vsetvli zero, %[c], e16, m4, ta, ma\n"
                 "vle16.v v4, (%[codes])\n"
                 "vsll.vi v4, v4, 4\n" // * 16 B per entry
                 :
                 : [c] "r"(c), [codes] "r"(codes)
                 : "v4", "v5", "v6", "v7", "memory");
#endif

    // Gather one record lane per vluxei16 (the base pointer absorbs the
    // lane offset), then interleave via strided stores. Only three m8
    // register groups exist beside the offsets; reusing v8 for the last
    // lane is free because the single VLSU executes these in order anyway.
    asm volatile("vsetvli zero, %[c], e32, m8, ta, ma\n"
                 "vluxei16.v v8,  (%[d0]), v4\n"
                 "vsse32.v v8,  (%[o0]), %[str]\n"
                 "vluxei16.v v16, (%[d1]), v4\n"
                 "vsse32.v v16, (%[o1]), %[str]\n"
                 "vluxei16.v v24, (%[d2]), v4\n"
                 "vsse32.v v24, (%[o2]), %[str]\n"
                 "vluxei16.v v8,  (%[d3]), v4\n"
                 "vsse32.v v8,  (%[o3]), %[str]\n"
                 :
                 : [c] "r"(c), [d0] "r"(dict), [d1] "r"(dict + 1),
                   [d2] "r"(dict + 2), [d3] "r"(dict + 3), [o0] "r"(out),
                   [o1] "r"(out + 1), [o2] "r"(out + 2), [o3] "r"(out + 3),
                   [str] "r"(stride_b)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                   "memory");

    codes += c;
    out += c * DICT_D;
    n_codes -= c;
  }
}
