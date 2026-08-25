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
    // d1 note: n_idx == gvl there (one index per element), which exceeds
    // the e8/m1 VLMAX -- m2 covers it; d >= 2 fits m1 but m2 is harmless.
    asm volatile("vsetvli zero, %[n_idx], e8, m2, ta, ma\n"
                 "vle8.v v4, (%[codes])\n"
                 "vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei8.v v8, (%[dict]), v4\n"
                 :
                 : [n_idx] "r"(n_idx), [gvl] "r"(gvl), [codes] "r"(codes),
                   [dict] "r"(dict)
                 : "v4", "v5", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
                   "v15", "memory");
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

// Software-pipelined vlxblk, flattened (user, 2026-08-25): the hot loop
// covers TWO chunks with fixed register roles -- no ping-pong flag, no
// per-gather branches (the branchy first version cost ~2 cyc/chunk of
// scalar control at 0x80000cc8..cec and LOST 12% at d32). VLSU stream per
// body: G(v4) L->v4 G(v6) L->v6 -- every adjacent pair independent, and
// each gather's index operand was loaded two VLSU ops earlier with a full
// gather in between (RAW fully covered). Preloads in the last body read
// up to 2*n_idx bytes past codes[] -- harmless TCDM reads, values unused;
// avoiding them would need the branches this version exists to remove.
// Requires a uniform even chunk count (all registered geometries: 16384
// elements = 64 chunks at VLEN=1024); anything else falls back to the
// plain kernel.
void vrfload_vlxblk_swp(const float *dict, const dict_code_t *codes,
                        unsigned int n_codes) {
  size_t vlmax;
  asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(vlmax));
  const unsigned int rem = n_codes << DICT_D_LOG2;
  const unsigned int nchunks = rem / (unsigned int)vlmax;
  if ((rem % (unsigned int)vlmax) != 0 || (nchunks & 1) || nchunks < 2) {
    vrfload_vlxblk(dict, codes, n_codes);
    return;
  }
  asm volatile("vsetblklen %0" ::"r"(DICT_D));
  const size_t n_idx = vlmax >> DICT_D_LOG2;

  // Prologue: idx(chunk 0) -> v4, idx(chunk 1) -> v6 (one vsetvli).
#if DICT_CODE_BYTES == 1
  asm volatile("vsetvli zero, %[n], e8, m2, ta, ma\n"
               "vle8.v v4, (%[c0])\n"
               "vle8.v v6, (%[c1])\n"
               :
               : [n] "r"(n_idx), [c0] "r"(codes), [c1] "r"(codes + n_idx)
               : "v4", "v6", "memory");
#else
  asm volatile("vsetvli zero, %[n], e16, m2, ta, ma\n"
               "vle16.v v4, (%[c0])\n"
               "vle16.v v6, (%[c1])\n"
               :
               : [n] "r"(n_idx), [c0] "r"(codes), [c1] "r"(codes + n_idx)
               : "v4", "v5", "v6", "v7", "memory");
#endif

  const dict_code_t *pl = codes + 2 * n_idx; // next preload pointer
  unsigned int pairs = nchunks >> 1;
  while (pairs-- > 0) {
#if DICT_CODE_BYTES == 1
    asm volatile( // gather chunk 2k (idx in v4), preload idx(2k+2) -> v4
        "vsetvli zero, %[g], e32, m8, ta, ma\n"
        "vlxblkei8.v v8, (%[dict]), v4\n"
        "vsetvli zero, %[n], e8, m2, ta, ma\n"
        "vle8.v v4, (%[c0])\n"
        // gather chunk 2k+1 (idx in v6), preload idx(2k+3) -> v6
        "vsetvli zero, %[g], e32, m8, ta, ma\n"
        "vlxblkei8.v v16, (%[dict]), v6\n"
        "vsetvli zero, %[n], e8, m2, ta, ma\n"
        "vle8.v v6, (%[c1])\n"
        :
        : [g] "r"(vlmax), [n] "r"(n_idx), [dict] "r"(dict), [c0] "r"(pl),
          [c1] "r"(pl + n_idx)
        : "v4", "v6", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
          "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "memory");
#else
    asm volatile(
        "vsetvli zero, %[g], e32, m8, ta, ma\n"
        "vlxblkei16.v v8, (%[dict]), v4\n"
        "vsetvli zero, %[n], e16, m2, ta, ma\n"
        "vle16.v v4, (%[c0])\n"
        "vsetvli zero, %[g], e32, m8, ta, ma\n"
        "vlxblkei16.v v16, (%[dict]), v6\n"
        "vsetvli zero, %[n], e16, m2, ta, ma\n"
        "vle16.v v6, (%[c1])\n"
        :
        : [g] "r"(vlmax), [n] "r"(n_idx), [dict] "r"(dict), [c0] "r"(pl),
          [c1] "r"(pl + n_idx)
        : "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",
          "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22",
          "v23", "memory");
#endif
    pl += 2 * n_idx;
  }
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

#if DICT_D >= 2 && DICT_CODE_BYTES == 1
// Software-pipelined rvv (task-1 review, 2026-08-25): the expansion is
// vector-ALU work (VFU) while the gather is VLSU work -- separate units --
// so preloading the next chunk's codes and expanding them WHILE the
// current gather streams converts (expansion + gather) into
// max(expansion, gather). Two register-disjoint expansion chains avoid
// WAR against the in-flight gather's index operand:
//   chain A: codes v2, work v4..v7 / v24..v27, temp v28..v31 (shared)
//   chain B: codes v3, work v16..v19 / v20..v23, temp v28..v31 (shared)
//   gather dest: v8..v15 (WAW between consecutive gathers is free on the
//   in-order single-stream VLSU).
// Uniform even chunk count required (all registered geometries); anything
// else falls back to the plain expansion kernel.
#define RVVP_STEP(SRC, DST)                                                    \
  asm volatile("vsetvli zero, %[l], e16, m2, ta, ma\n"                         \
               "vwaddu.vx " DST ", " SRC ", zero\n"                            \
               "vsetvli zero, %[l], e32, m4, ta, ma\n"                         \
               "vsll.vi v28, " DST ", 16\n"                                    \
               "vadd.vv " DST ", " DST ", v28\n"                               \
               "vadd.vx " DST ", " DST ", %[dh]\n"                             \
               :                                                               \
               : [l] "r"(len), [dh] "r"(dhi)                                   \
               : "v28", "v29", "v30", "v31", "memory")

static inline void rvvp_expand_A(const dict_code_t *codes, size_t n_idx) {
  asm volatile("vsetvli zero, %[n], e8, m1, ta, ma\n"
               "vle8.v v2, (%[c])\n"
               "vwaddu.vx v4, v2, zero\n"
               "vsetvli zero, %[n], e16, m2, ta, ma\n"
               "vsll.vi v4, v4, " DICT_BLK_SHIFT "\n"
               :
               : [n] "r"(n_idx), [c] "r"(codes)
               : "v2", "v4", "v5", "memory");
  size_t len = n_idx;
  unsigned long delta = (DICT_D * 4) >> 1;
  for (unsigned int t = 0; t < DICT_D_LOG2; ++t) {
    const unsigned long dhi = delta << 16;
    if (!(t & 1)) {
      RVVP_STEP("v4", "v24");
      asm volatile("" ::: "v24", "v25", "v26", "v27");
    } else {
      RVVP_STEP("v24", "v4");
      asm volatile("" ::: "v4", "v5", "v6", "v7");
    }
    len <<= 1;
    delta >>= 1;
  }
}

static inline void rvvp_expand_B(const dict_code_t *codes, size_t n_idx) {
  asm volatile("vsetvli zero, %[n], e8, m1, ta, ma\n"
               "vle8.v v3, (%[c])\n"
               "vwaddu.vx v16, v3, zero\n"
               "vsetvli zero, %[n], e16, m2, ta, ma\n"
               "vsll.vi v16, v16, " DICT_BLK_SHIFT "\n"
               :
               : [n] "r"(n_idx), [c] "r"(codes)
               : "v3", "v16", "v17", "memory");
  size_t len = n_idx;
  unsigned long delta = (DICT_D * 4) >> 1;
  for (unsigned int t = 0; t < DICT_D_LOG2; ++t) {
    const unsigned long dhi = delta << 16;
    if (!(t & 1)) {
      RVVP_STEP("v16", "v20");
      asm volatile("" ::: "v20", "v21", "v22", "v23");
    } else {
      RVVP_STEP("v20", "v16");
      asm volatile("" ::: "v16", "v17", "v18", "v19");
    }
    len <<= 1;
    delta >>= 1;
  }
}

#if (DICT_D_LOG2 & 1)
#define RVVP_IDX_A "v24"
#define RVVP_IDX_B "v20"
#else
#define RVVP_IDX_A "v4"
#define RVVP_IDX_B "v16"
#endif

void vrfload_rvv_swp(const float *dict, const dict_code_t *codes,
                     unsigned int n_codes) {
  size_t vlmax;
  asm volatile("vsetvli %0, zero, e32, m8, ta, ma" : "=r"(vlmax));
  const unsigned int rem = n_codes << DICT_D_LOG2;
  const unsigned int nchunks = rem / (unsigned int)vlmax;
  if ((rem % (unsigned int)vlmax) != 0 || (nchunks & 1) || nchunks < 2) {
    vrfload_rvv(dict, codes, n_codes);
    return;
  }
  const size_t n_idx = vlmax >> DICT_D_LOG2;

  // Prologue: expand chunk 0 on chain A.
  rvvp_expand_A(codes, n_idx);

  const dict_code_t *pc = codes + n_idx; // codes of the NEXT chunk to expand
  unsigned int pairs = nchunks >> 1;
  while (pairs-- > 0) {
    // Gather chunk 2k (chain A index), then expand chunk 2k+1 on chain B
    // while the gather streams.
    asm volatile("vsetvli zero, %[g], e32, m8, ta, ma\n"
                 "vluxei16.v v8, (%[dict]), " RVVP_IDX_A "\n"
                 :
                 : [g] "r"(vlmax), [dict] "r"(dict)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "memory");
    rvvp_expand_B(pc, n_idx);
    pc += n_idx;
    // Gather chunk 2k+1 (chain B), expand chunk 2k+2 on chain A.
    asm volatile("vsetvli zero, %[g], e32, m8, ta, ma\n"
                 "vluxei16.v v8, (%[dict]), " RVVP_IDX_B "\n"
                 :
                 : [g] "r"(vlmax), [dict] "r"(dict)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "memory");
    rvvp_expand_A(pc, n_idx); // last iteration expands past the end:
    pc += n_idx;              // harmless ALU on stale/garbage codes,
  }                           // never gathered (loop exits).
}
#else
void vrfload_rvv_swp(const float *dict, const dict_code_t *codes,
                     unsigned int n_codes) {
  vrfload_rvv(dict, codes, n_codes);
}
#endif // DICT_D >= 2 && DICT_CODE_BYTES == 1

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
