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

// Load-only variants of the sp-dictdecode kernels: materialize indexed
// blocks in the VRF and never store them back. Models fused consumers
// (VQ-LLM inference dequant) where the decompressed data feeds the FPUs
// straight from the VRF. Loaded groups are overwritten by later loads;
// correctness of the identical load path is established by the caller's
// untimed load+store check stage (the sp-dictdecode kernels).
//
// This file must be included AFTER kernel/sp-dictdecode.c, whose
// DICT_D_LOG2 / DICT_BLK_SHIFT / DICT_CODE_EI macros it reuses. The same
// VLSU scheduling rules apply: with no stores there are no RAW pairs, so
// rotating destination groups plus interleaved scalar address computation
// keep the 2-slot buffer fed.

void vrfload_vlxblk(const float *dict, const dict_code_t *codes,
                    unsigned int n_codes) {
  asm volatile("vsetblklen %0" ::"r"(DICT_D));
  unsigned int rem = n_codes * DICT_D;

  const unsigned int idx_per_chunk = 128 >> DICT_D_LOG2;
  while (rem >= 4 * 128) {
#if DICT_CODE_BYTES == 1
    asm volatile("vsetvli zero, %[n_idx], e8, m1, ta, ma\n"
                 "vle8.v v4, (%[c0])\n"
                 "vle8.v v5, (%[c1])\n"
                 "vle8.v v6, (%[c2])\n"
                 "vle8.v v7, (%[c3])\n"
                 :
                 : [n_idx] "r"(idx_per_chunk), [c0] "r"(codes),
                   [c1] "r"(codes + idx_per_chunk),
                   [c2] "r"(codes + 2 * idx_per_chunk),
                   [c3] "r"(codes + 3 * idx_per_chunk)
                 : "v4", "v5", "v6", "v7", "memory");
#elif DICT_D == 4
    asm volatile("vsetvli zero, %[n_idx], e16, m4, ta, ma\n"
                 "vle16.v v4, (%[c0])\n"
                 :
                 : [n_idx] "r"(128), [c0] "r"(codes)
                 : "v4", "v5", "v6", "v7", "memory");
#else
    asm volatile("vsetvli zero, %[n_idx], e16, m1, ta, ma\n"
                 "vle16.v v4, (%[c0])\n"
                 "vle16.v v5, (%[c1])\n"
                 "vle16.v v6, (%[c2])\n"
                 "vle16.v v7, (%[c3])\n"
                 :
                 : [n_idx] "r"(idx_per_chunk), [c0] "r"(codes),
                   [c1] "r"(codes + idx_per_chunk),
                   [c2] "r"(codes + 2 * idx_per_chunk),
                   [c3] "r"(codes + 3 * idx_per_chunk)
                 : "v4", "v5", "v6", "v7", "memory");
#endif
    // Four m8 gathers, no stores: WAW reuse of v8 is harmless on the
    // in-order VLSU.
    asm volatile("vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei" DICT_CODE_EI ".v v8, (%[dict]), v4\n"
                 "vlxblkei" DICT_CODE_EI ".v v16, (%[dict]), v5\n"
                 "vlxblkei" DICT_CODE_EI ".v v24, (%[dict]), v6\n"
                 "vlxblkei" DICT_CODE_EI ".v v8, (%[dict]), v7\n"
                 :
                 : [gvl] "r"(128), [dict] "r"(dict)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                   "memory");

    codes += 4 * idx_per_chunk;
    rem -= 512;
  }

  while (rem > 0) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[rem], e32, m8, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [rem] "r"(rem));
    const size_t n_idx = gvl >> DICT_D_LOG2;

#if DICT_CODE_BYTES == 1
    asm volatile("vsetvli zero, %[n_idx], e8, m1, ta, ma\n"
                 "vle8.v v28, (%[codes])\n"
                 "vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei8.v v16, (%[dict]), v28\n"
                 :
                 : [n_idx] "r"(n_idx), [gvl] "r"(gvl), [codes] "r"(codes),
                   [dict] "r"(dict)
                 : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "v28", "memory");
#else
    asm volatile("vsetvli zero, %[n_idx], e16, m1, ta, ma\n"
                 "vle16.v v28, (%[codes])\n"
                 "vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei16.v v16, (%[dict]), v28\n"
                 :
                 : [n_idx] "r"(n_idx), [gvl] "r"(gvl), [codes] "r"(codes),
                   [dict] "r"(dict)
                 : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "v28", "memory");
#endif

    codes += n_idx;
    rem -= gvl;
  }
}

void vrfload_rvv(const float *dict, const dict_code_t *codes,
                 unsigned int n_codes) {
  while (n_codes > 0) {
    size_t c;
    asm volatile("vsetvli %[c], %[n], e32, m8, ta, ma"
                 : [c] "=r"(c)
                 : [n] "r"(n_codes));

#if DICT_CODE_BYTES == 1
    asm volatile("vsetvli zero, %[c], e8, m2, ta, ma\n"
                 "vle8.v v2, (%[codes])\n"
                 "vwaddu.vx v4, v2, zero\n"
                 "vsetvli zero, %[c], e16, m4, ta, ma\n"
                 "vsll.vi v4, v4, " DICT_BLK_SHIFT "\n"
                 :
                 : [c] "r"(c), [codes] "r"(codes)
                 : "v2", "v3", "v4", "v5", "v6", "v7", "memory");
#else
    asm volatile("vsetvli zero, %[c], e16, m4, ta, ma\n"
                 "vle16.v v4, (%[codes])\n"
                 "vsll.vi v4, v4, " DICT_BLK_SHIFT "\n"
                 :
                 : [c] "r"(c), [codes] "r"(codes)
                 : "v4", "v5", "v6", "v7", "memory");
#endif

#if DICT_D == 2
    asm volatile("vsetvli zero, %[c], e32, m8, ta, ma\n"
                 "vluxei16.v v8,  (%[d0]), v4\n"
                 "vluxei16.v v16, (%[d1]), v4\n"
                 :
                 : [c] "r"(c), [d0] "r"(dict), [d1] "r"(dict + 1)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "memory");
#else
    for (unsigned int j = 0; j < DICT_D; j += 4) {
      asm volatile("vsetvli zero, %[c], e32, m8, ta, ma\n"
                   "vluxei16.v v8,  (%[d0]), v4\n"
                   "vluxei16.v v16, (%[d1]), v4\n"
                   "vluxei16.v v24, (%[d2]), v4\n"
                   "vluxei16.v v8,  (%[d3]), v4\n"
                   :
                   : [c] "r"(c), [d0] "r"(dict + j), [d1] "r"(dict + j + 1),
                     [d2] "r"(dict + j + 2), [d3] "r"(dict + j + 3)
                   : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                     "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                     "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                     "memory");
    }
#endif

    codes += c;
    n_codes -= c;
  }
}

void vrfload_vle(const float *dict, const dict_code_t *codes,
                 unsigned int n_codes) {
#if DICT_D == 64
  asm volatile("vsetvli zero, %[d], e32, m4, ta, ma" ::[d] "r"(DICT_D));
#elif DICT_D == 32
  asm volatile("vsetvli zero, %[d], e32, m2, ta, ma" ::[d] "r"(DICT_D));
#else
  asm volatile("vsetvli zero, %[d], e32, m1, ta, ma" ::[d] "r"(DICT_D));
#endif

  while (n_codes >= 4) {
    const float *s0 = dict + ((size_t)codes[0] << DICT_D_LOG2);
    asm volatile("vle32.v v0, (%[s])" ::[s] "r"(s0) : "v0", "v1", "memory");
    const float *s1 = dict + ((size_t)codes[1] << DICT_D_LOG2);
    asm volatile("vle32.v v8, (%[s])" ::[s] "r"(s1) : "v8", "v9", "memory");
    const float *s2 = dict + ((size_t)codes[2] << DICT_D_LOG2);
    asm volatile("vle32.v v16, (%[s])" ::[s] "r"(s2) : "v16", "v17", "memory");
    const float *s3 = dict + ((size_t)codes[3] << DICT_D_LOG2);
    asm volatile("vle32.v v24, (%[s])" ::[s] "r"(s3) : "v24", "v25", "memory");
    codes += 4;
    n_codes -= 4;
  }

  while (n_codes > 0) {
    const float *src = dict + ((size_t)codes[0] << DICT_D_LOG2);
    asm volatile("vle32.v v0, (%[s])" ::[s] "r"(src) : "v0", "v1", "memory");
    codes++;
    n_codes--;
  }
}
