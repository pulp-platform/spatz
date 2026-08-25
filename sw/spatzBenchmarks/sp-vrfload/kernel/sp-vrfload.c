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

  // VLMAX-dynamic: one full m8 gather per iteration (gvl = min(rem, VLMAX)
  // at e32/m8 = VLEN/4 elements), so the register-group amortization scales
  // with the hardware VLEN instead of being pinned to a VLEN=512 literal.
  // Both instructions are loads, so the 2-slot VLSU buffer streams without
  // reordering; the scalar bookkeeping hides under the gather. Index vector
  // fits m1 at e8 / m2 at e16 for every D >= 2 up to VLEN=1024.
  while (rem > 0) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[rem], e32, m8, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [rem] "r"(rem));
    const size_t n_idx = gvl >> DICT_D_LOG2;

#if DICT_CODE_BYTES == 1
    asm volatile("vsetvli zero, %[n_idx], e8, m1, ta, ma\n"
                 "vle8.v v4, (%[codes])\n"
                 "vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei8.v v8, (%[dict]), v4\n"
                 :
                 : [n_idx] "r"(n_idx), [gvl] "r"(gvl), [codes] "r"(codes),
                   [dict] "r"(dict)
                 : "v4", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "memory");
#else
    asm volatile("vsetvli zero, %[n_idx], e16, m2, ta, ma\n"
                 "vle16.v v4, (%[codes])\n"
                 "vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei16.v v8, (%[dict]), v4\n"
                 :
                 : [n_idx] "r"(n_idx), [gvl] "r"(gvl), [codes] "r"(codes),
                   [dict] "r"(dict)
                 : "v4", "v5", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
                   "v15", "memory");
#endif

    codes += n_idx;
    rem -= gvl;
  }
}

// Software-pipelined vlxblk (user proposal, 2026-08-25): break the
// index-load -> gather RAW by preloading the NEXT chunk's indices before
// the current gather. Every adjacent VLSU instruction pair becomes
// independent, so the per-chunk serial term F (~12.5 cyc: the exposed
// index round trip) should collapse toward the ~1.6-cycle back-to-back
// handoff. Double-buffered indices v4/v6, gather destinations v8/v16.
void vrfload_vlxblk_swp(const float *dict, const dict_code_t *codes,
                        unsigned int n_codes) {
  asm volatile("vsetblklen %0" ::"r"(DICT_D));
  size_t vlmax;
  asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(vlmax));
  unsigned int rem = n_codes << DICT_D_LOG2;

  // Prologue: indices for chunk 0 into v4.
  size_t gvl = rem < vlmax ? rem : vlmax;
  size_t n_idx = gvl >> DICT_D_LOG2;
#if DICT_CODE_BYTES == 1
#define VRF_SWP_IDX_LOAD(reg)                                                  \
  asm volatile("vsetvli zero, %[n], e8, m1, ta, ma\n"                          \
               "vle8.v " reg ", (%[c])\n"                                      \
               :                                                               \
               : [n] "r"(n_idx), [c] "r"(cptr)                                 \
               : reg, "memory")
#else
#define VRF_SWP_IDX_LOAD(reg)                                                  \
  asm volatile("vsetvli zero, %[n], e16, m1, ta, ma\n"                         \
               "vle16.v " reg ", (%[c])\n"                                     \
               :                                                               \
               : [n] "r"(n_idx), [c] "r"(cptr)                                 \
               : reg, "memory")
#endif
  {
    const dict_code_t *cptr = codes;
    VRF_SWP_IDX_LOAD("v4");
  }

  unsigned int pong = 0;
  while (rem > 0) {
    const size_t cur_gvl = gvl;
    const dict_code_t *next_c = codes + n_idx;
    const unsigned int next_rem = rem - (unsigned int)cur_gvl;

    // Preload indices for the NEXT chunk (independent of the gather below;
    // enters the in-order VLSU first, streams in ~1 cycle, and its drain
    // overlaps the long gather that follows).
    if (next_rem > 0) {
      gvl = next_rem < vlmax ? next_rem : vlmax;
      n_idx = gvl >> DICT_D_LOG2;
      const dict_code_t *cptr = next_c;
      if (pong) {
        VRF_SWP_IDX_LOAD("v4");
      } else {
        VRF_SWP_IDX_LOAD("v6");
      }
    }

    // Gather the CURRENT chunk with indices loaded one iteration ago.
#if DICT_CODE_BYTES == 1
    if (pong)
      asm volatile("vsetvli zero, %[g], e32, m8, ta, ma\n"
                   "vlxblkei8.v v16, (%[dict]), v6\n"
                   :
                   : [g] "r"(cur_gvl), [dict] "r"(dict)
                   : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                     "memory");
    else
      asm volatile("vsetvli zero, %[g], e32, m8, ta, ma\n"
                   "vlxblkei8.v v8, (%[dict]), v4\n"
                   :
                   : [g] "r"(cur_gvl), [dict] "r"(dict)
                   : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                     "memory");
#else
    if (pong)
      asm volatile("vsetvli zero, %[g], e32, m8, ta, ma\n"
                   "vlxblkei16.v v16, (%[dict]), v6\n"
                   :
                   : [g] "r"(cur_gvl), [dict] "r"(dict)
                   : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                     "memory");
    else
      asm volatile("vsetvli zero, %[g], e32, m8, ta, ma\n"
                   "vlxblkei16.v v8, (%[dict]), v4\n"
                   :
                   : [g] "r"(cur_gvl), [dict] "r"(dict)
                   : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                     "memory");
#endif
    pong ^= 1;
    codes = next_c;
    rem = next_rem;
  }
#undef VRF_SWP_IDX_LOAD
}

void vrfload_rvv(const float *dict, const dict_code_t *codes,
                 unsigned int n_codes) {
  // Layout-matched element-gather baseline (v2): expand codes to
  // per-element e16 byte offsets (dict_expand_offsets_e16, shared with the
  // dictdecode check twin), then ONE vluxei16 per chunk materializes the
  // records PACKED in memory order in v8..v15 -- the same VRF image
  // vlxblk produces.
#if DICT_D == 1
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
    asm volatile("vsetvli zero, %[c], e32, m8, ta, ma\n"
                 "vluxei16.v v8, (%[d0]), v4\n"
                 :
                 : [c] "r"(c), [d0] "r"(dict)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "memory");
    codes += c;
    n_codes -= c;
  }
#else
  unsigned int rem = n_codes << DICT_D_LOG2;
  while (rem > 0) {
    size_t gvl;
    asm volatile("vsetvli %[g], %[r], e32, m8, ta, ma"
                 : [g] "=r"(gvl)
                 : [r] "r"(rem));
    const size_t n_idx = gvl >> DICT_D_LOG2;
    dict_expand_offsets_e16(codes, n_idx);
    asm volatile("vsetvli zero, %[g], e32, m8, ta, ma\n"
                 "vluxei16.v v8, (%[dict]), " DICT_EXPANDED_IDX "\n"
                 :
                 : [g] "r"(gvl), [dict] "r"(dict)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "memory");
    codes += n_idx;
    rem -= gvl;
  }
#endif
}

// Bounce buffer for the sub-register packing path; TCDM, allocated and
// assigned by main.c (VRFLOAD variant 2 only).
float *vrfload_vle_scratch;

void vrfload_vle(const float *dict, const dict_code_t *codes,
                 unsigned int n_codes) {
  // Layout-matched vle baseline (v2): must leave records PACKED in v8..v15
  // exactly like vlxblk. At D >= 1 register the packing is free (register
  // numbering); below it the packed image is assembled in a TCDM scratch
  // with unit-stride per-record stores and materialized by one m8 load
  // (vslideup placement serializes on the destination group; Spatz has no
  // vrgather).
  size_t epr; // e32 elements per single register (VLMAX at m1)
  asm volatile("vsetvli %0, zero, e32, m1, ta, ma" : "=r"(epr));

  if (DICT_D >= epr) {
    const unsigned int r = DICT_D / epr; // registers per record
    unsigned int i = 0;
    if (r == 1) {
      asm volatile("vsetvli zero, %0, e32, m1, ta, ma" ::"r"(DICT_D));
      for (; i + 8 <= n_codes; i += 8) {
        const float *s0 = dict + ((size_t)codes[i + 0] << DICT_D_LOG2);
        const float *s1 = dict + ((size_t)codes[i + 1] << DICT_D_LOG2);
        const float *s2 = dict + ((size_t)codes[i + 2] << DICT_D_LOG2);
        const float *s3 = dict + ((size_t)codes[i + 3] << DICT_D_LOG2);
        asm volatile("vle32.v v8, (%[s0])\n"
                     "vle32.v v9, (%[s1])\n"
                     "vle32.v v10, (%[s2])\n"
                     "vle32.v v11, (%[s3])\n"
                     :
                     : [s0] "r"(s0), [s1] "r"(s1), [s2] "r"(s2), [s3] "r"(s3)
                     : "v8", "v9", "v10", "v11", "memory");
        const float *s4 = dict + ((size_t)codes[i + 4] << DICT_D_LOG2);
        const float *s5 = dict + ((size_t)codes[i + 5] << DICT_D_LOG2);
        const float *s6 = dict + ((size_t)codes[i + 6] << DICT_D_LOG2);
        const float *s7 = dict + ((size_t)codes[i + 7] << DICT_D_LOG2);
        asm volatile("vle32.v v12, (%[s4])\n"
                     "vle32.v v13, (%[s5])\n"
                     "vle32.v v14, (%[s6])\n"
                     "vle32.v v15, (%[s7])\n"
                     :
                     : [s4] "r"(s4), [s5] "r"(s5), [s6] "r"(s6), [s7] "r"(s7)
                     : "v12", "v13", "v14", "v15", "memory");
      }
      for (; i < n_codes; ++i) {
        const float *s0 = dict + ((size_t)codes[i] << DICT_D_LOG2);
        asm volatile("vle32.v v8, (%[s])\n" ::[s] "r"(s0) : "v8", "memory");
      }
    } else if (r == 2) {
      asm volatile("vsetvli zero, %0, e32, m2, ta, ma" ::"r"(DICT_D));
      for (; i + 4 <= n_codes; i += 4) {
        const float *s0 = dict + ((size_t)codes[i + 0] << DICT_D_LOG2);
        const float *s1 = dict + ((size_t)codes[i + 1] << DICT_D_LOG2);
        const float *s2 = dict + ((size_t)codes[i + 2] << DICT_D_LOG2);
        const float *s3 = dict + ((size_t)codes[i + 3] << DICT_D_LOG2);
        asm volatile("vle32.v v8, (%[s0])\n"
                     "vle32.v v10, (%[s1])\n"
                     "vle32.v v12, (%[s2])\n"
                     "vle32.v v14, (%[s3])\n"
                     :
                     : [s0] "r"(s0), [s1] "r"(s1), [s2] "r"(s2), [s3] "r"(s3)
                     : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                       "memory");
      }
      for (; i < n_codes; ++i) {
        const float *s0 = dict + ((size_t)codes[i] << DICT_D_LOG2);
        asm volatile("vle32.v v8, (%[s])\n" ::[s] "r"(s0)
                     : "v8", "v9", "memory");
      }
    } else if (r == 4) {
      asm volatile("vsetvli zero, %0, e32, m4, ta, ma" ::"r"(DICT_D));
      for (; i + 2 <= n_codes; i += 2) {
        const float *s0 = dict + ((size_t)codes[i + 0] << DICT_D_LOG2);
        const float *s1 = dict + ((size_t)codes[i + 1] << DICT_D_LOG2);
        asm volatile("vle32.v v8, (%[s0])\n"
                     "vle32.v v12, (%[s1])\n"
                     :
                     : [s0] "r"(s0), [s1] "r"(s1)
                     : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                       "memory");
      }
      for (; i < n_codes; ++i) {
        const float *s0 = dict + ((size_t)codes[i] << DICT_D_LOG2);
        asm volatile("vle32.v v8, (%[s])\n" ::[s] "r"(s0)
                     : "v8", "v9", "v10", "v11", "memory");
      }
    } else {
      asm volatile("vsetvli zero, %0, e32, m8, ta, ma" ::"r"(DICT_D));
      for (; i < n_codes; ++i) {
        const float *s0 = dict + ((size_t)codes[i] << DICT_D_LOG2);
        asm volatile("vle32.v v8, (%[s])\n" ::[s] "r"(s0)
                     : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                       "memory");
      }
    }
    return;
  }

  size_t vlmax8;
  asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(vlmax8));
  float *const scr = vrfload_vle_scratch;
  unsigned int rem = n_codes << DICT_D_LOG2;
  while (rem > 0) {
    const size_t gvl = rem < vlmax8 ? rem : vlmax8;
    const size_t n_rec = gvl >> DICT_D_LOG2;
    asm volatile("vsetvli zero, %0, e32, m1, ta, ma" ::"r"(DICT_D));
    float *sp = scr;
    size_t i = 0;
    for (; i + 2 <= n_rec; i += 2) {
      const float *s0 = dict + ((size_t)codes[i + 0] << DICT_D_LOG2);
      const float *s1 = dict + ((size_t)codes[i + 1] << DICT_D_LOG2);
      asm volatile("vle32.v v1, (%[s0])\n"
                   "vle32.v v2, (%[s1])\n"
                   "vse32.v v1, (%[o0])\n"
                   "vse32.v v2, (%[o1])\n"
                   :
                   : [s0] "r"(s0), [s1] "r"(s1), [o0] "r"(sp),
                     [o1] "r"(sp + DICT_D)
                   : "v1", "v2", "memory");
      sp += 2 * (size_t)DICT_D;
    }
    for (; i < n_rec; ++i) {
      const float *s0 = dict + ((size_t)codes[i] << DICT_D_LOG2);
      asm volatile("vle32.v v1, (%[s])\n"
                   "vse32.v v1, (%[o])\n"
                   :
                   : [s] "r"(s0), [o] "r"(sp)
                   : "v1", "memory");
      sp += DICT_D;
    }
    asm volatile("vsetvli zero, %[g], e32, m8, ta, ma\n"
                 "vle32.v v8, (%[sc])\n"
                 :
                 : [g] "r"(gvl), [sc] "r"(scr)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "memory");
    codes += n_rec;
    rem -= gvl;
  }
}
