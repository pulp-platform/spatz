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
//   out[i*D .. i*D+D-1] = dict[code[i]*D .. code[i]*D+D-1],  D x fp32
//   (D power of two in 4..32, i.e. 16 B up to 128 B record slots)
//
// Pure indexed-block-copy engine test (structurally VQ decode without
// arithmetic). Three variants:
//   - dictdecode_vlxblk: custom VLXBLK indexed block loads (one index per
//     D*4 B block, blocks fetched on the unit-stride fast path). At D=32
//     one block spans TWO vector registers inside the destination group.
//   - dictdecode_rvv: best-effort plain-RVV baseline. Spatz has no
//     VRGATHER/VRGATHEREI16 (verified against the vpu decoder,
//     spatz_decoder.sv), so per-element index replication in registers is
//     impossible. Instead the decode is column-decomposed: the byte offset
//     of each code's block (code * D*4, fits in 16 bit for K*D*4 <= 64 KiB;
//     at K=256/D=32 the max offset is 255*128 = 32640, still e16 - larger
//     K*D would force e32 offsets and halve the offset-build throughput)
//     is built once per chunk with vsll at e16, then reused by D vluxei16
//     gathers (one per record lane, base pointer absorbs the +4*j lane
//     offset) whose results are written back with D strided stores
//     (stride = D*4 B).
//   - dictdecode_vle: the honest large-block alternative - per code, a
//     scalar address computation plus one unit-stride vle32/vse32 pair
//     (vl = D, hoisted). No index vectors at all; expected to reach
//     parity with vlxblk once a block covers >= one vreg (D*4 >= 64 B).

#include "sp-dictdecode.h"

#if DICT_D == 1
#define DICT_D_LOG2 0
#define DICT_BLK_SHIFT "2"
#elif DICT_D == 2
#define DICT_D_LOG2 1
#define DICT_BLK_SHIFT "3" // log2(D*4): code -> byte offset, as asm literal
#elif DICT_D == 4
#define DICT_D_LOG2 2
#define DICT_BLK_SHIFT "4"
#elif DICT_D == 8
#define DICT_D_LOG2 3
#define DICT_BLK_SHIFT "5"
#elif DICT_D == 16
#define DICT_D_LOG2 4
#define DICT_BLK_SHIFT "6"
#elif DICT_D == 32
#define DICT_D_LOG2 5
#define DICT_BLK_SHIFT "7"
#elif DICT_D == 64
#define DICT_D_LOG2 6
#define DICT_BLK_SHIFT "8"
#elif DICT_D == 128
#define DICT_D_LOG2 7
#define DICT_BLK_SHIFT "9"
#else
#error "sp-dictdecode kernels support power-of-two DICT_D in {1, ..., 128}"
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

  // Steady state: 4 full m8 chunks (512 elements, 512/D codes, 2 KiB out)
  // per iteration; v4..v7 feed four gathers (one index vreg per chunk,
  // 128/D indices each); two m8 data groups (v8/v16) ping-pong, which is
  // free on the single in-order VLSU.
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
    // 32 x e16 indices fill exactly one vreg, so a single m4 load lands
    // one chunk's indices in each of v4..v7.
    asm volatile("vsetvli zero, %[n_idx], e16, m4, ta, ma\n"
                 "vle16.v v4, (%[c0])\n"
                 :
                 : [n_idx] "r"(128), [c0] "r"(codes)
                 : "v4", "v5", "v6", "v7", "memory");
#else
    // D > 4: fewer than a vreg of indices per chunk; index vector operands
    // must start at element 0, so load each chunk's indices separately.
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
    // The VLSU buffers only two unexecuted instructions and runs in order:
    // a store issued right after its gather RAW-blocks the buffer head.
    // Three m8 groups are free (v0's group holds the indices), so issue
    // three gathers before their stores; only the fourth pair stays
    // adjacent.
    asm volatile("vsetvli zero, %[gvl], e32, m8, ta, ma\n"
                 "vlxblkei" DICT_CODE_EI ".v v8, (%[dict]), v4\n"
                 "vlxblkei" DICT_CODE_EI ".v v16, (%[dict]), v5\n"
                 "vlxblkei" DICT_CODE_EI ".v v24, (%[dict]), v6\n"
                 "vse32.v v8, (%[o0])\n"
                 "vse32.v v16, (%[o1])\n"
                 "vse32.v v24, (%[o2])\n"
                 "vlxblkei" DICT_CODE_EI ".v v8, (%[dict]), v7\n"
                 "vse32.v v8, (%[o3])\n"
                 :
                 : [gvl] "r"(128), [dict] "r"(dict), [o0] "r"(out),
                   [o1] "r"(out + 128), [o2] "r"(out + 256),
                   [o3] "r"(out + 384)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                   "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                   "memory");

    codes += 4 * idx_per_chunk;
    out += 512;
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

#if DICT_D >= 2
// Layout-matched offset expansion (v2 baselines): expand n_idx codes into
// n_idx*D consecutive e16 BYTE offsets -- record base = code << BLK_SHIFT,
// lanes +4 B apart -- via DICT_D_LOG2 in-register widening-doubling steps
// ({x} -> {x, x+delta} through an e32 pack). Spatz's VRF is bit-linear, so
// cross-SEW register aliasing is byte-ordered as the spec requires (on
// Ara's lane-striped VRF this trick is INVALID and a vrgather construction
// is used instead). Result: e16 offsets in v4..v7 when DICT_D_LOG2 is
// even, v24..v27 when odd. Clobbers v2..v7, v24..v31. Requires
// K*D*4 <= 64 KiB (16-bit offsets, checked in main.c).
static inline void dict_expand_offsets_e16(const dict_code_t *codes,
                                           size_t n_idx) {
#if DICT_CODE_BYTES == 1
  asm volatile("vsetvli zero, %[n], e8, m1, ta, ma\n"
               "vle8.v v2, (%[codes])\n"
               "vwaddu.vx v4, v2, zero\n"
               "vsetvli zero, %[n], e16, m2, ta, ma\n"
               "vsll.vi v4, v4, " DICT_BLK_SHIFT "\n"
               :
               : [n] "r"(n_idx), [codes] "r"(codes)
               : "v2", "v3", "v4", "v5", "memory");
#else
  asm volatile("vsetvli zero, %[n], e16, m2, ta, ma\n"
               "vle16.v v4, (%[codes])\n"
               "vsll.vi v4, v4, " DICT_BLK_SHIFT "\n"
               :
               : [n] "r"(n_idx), [codes] "r"(codes)
               : "v4", "v5", "memory");
#endif
  size_t len = n_idx;
  unsigned long delta = (DICT_D * 4) >> 1;
  for (unsigned int t = 0; t < DICT_D_LOG2; ++t) {
    const unsigned long dhi = delta << 16;
    if (!(t & 1))
      asm volatile("vsetvli zero, %[l], e16, m2, ta, ma\n"
                   "vwaddu.vx v24, v4, zero\n"
                   "vsetvli zero, %[l], e32, m4, ta, ma\n"
                   "vsll.vi v28, v24, 16\n"
                   "vadd.vv v24, v24, v28\n"
                   "vadd.vx v24, v24, %[dh]\n"
                   :
                   : [l] "r"(len), [dh] "r"(dhi)
                   : "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
    else
      asm volatile("vsetvli zero, %[l], e16, m2, ta, ma\n"
                   "vwaddu.vx v4, v24, zero\n"
                   "vsetvli zero, %[l], e32, m4, ta, ma\n"
                   "vsll.vi v28, v4, 16\n"
                   "vadd.vv v4, v4, v28\n"
                   "vadd.vx v4, v4, %[dh]\n"
                   :
                   : [l] "r"(len), [dh] "r"(dhi)
                   : "v4", "v5", "v6", "v7", "v28", "v29", "v30", "v31");
    len <<= 1;
    delta >>= 1;
  }
}
#if (DICT_D_LOG2 & 1)
#define DICT_EXPANDED_IDX "v24"
#else
#define DICT_EXPANDED_IDX "v4"
#endif
#endif // DICT_D >= 2

void dictdecode_rvv(float *out, const float *dict, const dict_code_t *codes,
                    unsigned int n_codes) {
  // Layout-matched element-gather twin (v2): expand codes to per-element
  // offsets, gather PACKED records with one vluxei16, store with one
  // unit-stride vse32. Mirrors vrfload_rvv so the golden check verifies
  // the expansion arithmetic bit-exactly. Replaces the per-column
  // decomposition (native vluxseg semantics: deinterleaved SoA image +
  // strided stores).
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
                 "vse32.v v8, (%[o0])\n"
                 :
                 : [c] "r"(c), [d0] "r"(dict), [o0] "r"(out)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "memory");
    codes += c;
    out += c;
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
                 "vse32.v v8, (%[o0])\n"
                 :
                 : [g] "r"(gvl), [dict] "r"(dict), [o0] "r"(out)
                 : "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                   "memory");
    codes += n_idx;
    out += gvl;
    rem -= gvl;
  }
#endif
}
void dictdecode_vle(float *out, const float *dict, const dict_code_t *codes,
                    unsigned int n_codes) {
  // The plain unit-stride alternative: per code, scalar-load the code,
  // compute the slot address (slli+add), then one vle32/vse32 pair of D
  // elements. vl = D is hoisted out of the loop; m2 fits a 128 B slot
  // (D=32) in a single instruction (two vregs per group), m1 suffices for
  // D <= 16.
#if DICT_D == 128
  asm volatile("vsetvli zero, %[d], e32, m8, ta, ma" ::[d] "r"(DICT_D));
#elif DICT_D == 64
  asm volatile("vsetvli zero, %[d], e32, m4, ta, ma" ::[d] "r"(DICT_D));
#elif DICT_D == 32
  asm volatile("vsetvli zero, %[d], e32, m2, ta, ma" ::[d] "r"(DICT_D));
#else
  asm volatile("vsetvli zero, %[d], e32, m1, ta, ma" ::[d] "r"(DICT_D));
#endif

  // Unroll x4 with rotating destination registers. Two scheduling rules
  // (the VLSU buffers only TWO unexecuted instructions, so Snitch stalls
  // on the third pending one): (1) all four loads issue before any store,
  // so no store RAW-blocks the buffer head while its load drains; (2) each
  // scalar address computation sits between two vector issues, turning the
  // buffer-full stall cycles into useful work. Separate asm blocks keep
  // that interleave — asm volatile statements are not reordered.
  while (n_codes >= 4) {
    const float *s0 = dict + ((size_t)codes[0] << DICT_D_LOG2);
    asm volatile("vle32.v v0, (%[s])" ::[s] "r"(s0) : "v0", "v1", "memory");
    const float *s1 = dict + ((size_t)codes[1] << DICT_D_LOG2);
    asm volatile("vle32.v v8, (%[s])" ::[s] "r"(s1) : "v8", "v9", "memory");
    const float *s2 = dict + ((size_t)codes[2] << DICT_D_LOG2);
    asm volatile("vle32.v v16, (%[s])" ::[s] "r"(s2) : "v16", "v17", "memory");
    const float *s3 = dict + ((size_t)codes[3] << DICT_D_LOG2);
    asm volatile("vle32.v v24, (%[s])" ::[s] "r"(s3) : "v24", "v25", "memory");
    float *o0 = out;
    asm volatile("vse32.v v0, (%[o])" ::[o] "r"(o0) : "memory");
    float *o1 = out + DICT_D;
    asm volatile("vse32.v v8, (%[o])" ::[o] "r"(o1) : "memory");
    float *o2 = out + 2 * DICT_D;
    asm volatile("vse32.v v16, (%[o])" ::[o] "r"(o2) : "memory");
    float *o3 = out + 3 * DICT_D;
    asm volatile("vse32.v v24, (%[o])" ::[o] "r"(o3) : "memory");
    codes += 4;
    out += 4 * DICT_D;
    n_codes -= 4;
  }

  while (n_codes > 0) {
    const float *src = dict + ((size_t)codes[0] << DICT_D_LOG2);
    asm volatile("vle32.v v0, (%[s])\n"
                 "vse32.v v0, (%[o])\n"
                 :
                 : [s] "r"(src), [o] "r"(out)
                 : "v0", "v1", "memory");
    codes++;
    out += DICT_D;
    n_codes--;
  }
}
