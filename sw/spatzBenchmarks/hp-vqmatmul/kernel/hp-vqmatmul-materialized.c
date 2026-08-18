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

#include "hp-vqmatmul-materialized.h"

#include "../../hp-fmatmul/kernel/hp-fmatmul.c"

#include <stdint.h>

#ifndef VQ_BLOCK_LEN
#define VQ_BLOCK_LEN 8
#endif

#define VLXBLK_EI8 "vlxblkei8.v"

#define CB_D VQ_BLOCK_LEN

void vq_dequantize_rvv_materialized(__fp16 *b, const __fp16 *b_cb0,
                                         const __fp16 *b_cb1,
                                         const uint8_t *b_idx0,
                                         const uint8_t *b_idx1,
                                         const __fp16 *b_scales,
                                         const unsigned int k_start,
                                         const unsigned int k_end,
                                         const unsigned int N) {
  const unsigned int groups = N / CB_D;

  if ((k_start >= k_end) || (groups == 0))
    return;

  asm volatile("vsetvli zero, %0, e16, m1, ta, ma" ::"r"(CB_D));

  for (unsigned int k = k_start; k < k_end; ++k) {
    const uint8_t *idx0 = b_idx0 + k * groups;
    const uint8_t *idx1 = b_idx1 + k * groups;
    __fp16 *b_row = b + k * N;
    float scale;
    asm volatile("flh %[s], 0(%[scale])" : [s] "=f"(scale)
                 : [scale] "r"(b_scales + k));

    for (unsigned int g = 0; g < groups; ++g) {
      const __fp16 *cb0 = b_cb0 + ((unsigned int)idx0[g] * CB_D);
      const __fp16 *cb1 = b_cb1 + ((unsigned int)idx1[g] * CB_D);
      __fp16 *dst = b_row + g * CB_D;

      asm volatile("vle16.v  v0, (%[cb0])\n"
                   "vle16.v  v1, (%[cb1])\n"
                   "vfadd.vv v2, v0, v1\n"
                   "vfmul.vf v2, v2, %[scale]\n"
                   "vse16.v  v2, (%[dst])\n"
                   :
                   : [cb0] "r"(cb0), [cb1] "r"(cb1), [dst] "r"(dst),
                     [scale] "f"(scale)
                   : "v0", "v1", "v2", "memory");
    }
  }
}

void vq_dequantize_vlxblk_materialized(__fp16 *b, const __fp16 *b_cb0,
                                       const __fp16 *b_cb1,
                                       const uint8_t *b_idx0,
                                       const uint8_t *b_idx1,
                                       const __fp16 *b_scales,
                                       const unsigned int k_start,
                                       const unsigned int k_end,
                                       const unsigned int N) {
  const unsigned int groups = N / CB_D;

  if ((k_start >= k_end) || (groups == 0))
    return;

  register const __fp16 *cb0_reg asm("t2") = b_cb0;
  register const __fp16 *cb1_reg asm("t3") = b_cb1;
  asm volatile("" ::"r"(cb0_reg), "r"(cb1_reg));
  asm volatile("vsetblklen %0" ::"r"(CB_D));

  for (unsigned int k = k_start; k < k_end; ++k) {
    const uint8_t *idx0 = b_idx0 + k * groups;
    const uint8_t *idx1 = b_idx1 + k * groups;
    __fp16 *b_row = b + k * N;
    float scale;
    asm volatile("flh %[s], 0(%[scale])" : [s] "=f"(scale)
                 : [scale] "r"(b_scales + k));

    for (unsigned int g = 0; g < groups;) {
      size_t gvl;
      asm volatile("vsetvli %[gvl], %[vl], e16, m4, ta, ma"
                   : [gvl] "=r"(gvl)
                   : [vl] "r"((groups - g) * CB_D));

      const unsigned int group_vl = gvl / CB_D;

      asm volatile("vsetvli zero, %[group_vl], e8, m2, ta, ma\n"
                   "vle8.v v28, (%[idx0])\n"
                   "vle8.v v30, (%[idx1])\n"
                   "vsetvli zero, %[gvl], e16, m4, ta, ma\n"
                   VLXBLK_EI8 " v16, (%[cb0]), v28\n"
                   VLXBLK_EI8 " v20, (%[cb1]), v30\n"
                   "vfadd.vv v16, v16, v20\n"
                   "vfmul.vf v16, v16, %[scale]\n"
                   "vse16.v v16, (%[dst])\n"
                   :
                   : [group_vl] "r"(group_vl), [gvl] "r"(gvl),
                     [idx0] "r"(idx0 + g), [idx1] "r"(idx1 + g),
                     [cb0] "r"(cb0_reg), [cb1] "r"(cb1_reg),
                     [scale] "f"(scale), [dst] "r"(b_row + g * CB_D)
                   : "v16", "v20", "v28", "v30", "memory");

      g += group_vl;
    }
  }
}

// -------------------------------------------------------------------------
// large-block ablation: plain unit-stride vle16 scalar-loop dequantization.
//
// Claim under test: once one block fills >= 1 vector register
// (CB_D * SEW >= VLEN, here CB_D = 32 e16 = 64 B), a scalar loop of plain
// vle16 loads into *consecutive destination registers* packs blocks for
// free (the register number is the packing), so it should reach parity
// with vlxblk. Below one register (CB_D = 8 -> 16-B quarter-register
// blocks) the honest vle loop needs an explicit vslideup packing chain
// per block and is expected to collapse.
// -------------------------------------------------------------------------

#if CB_D == 32

// One 64-B block: lbu idx (compiler) / slli 6 (compiler) / add base
// (compiler) / vsetvli e16 m1 vl=32 / vle16 into a fixed m1 register.
#define VQ_VLE32_LOAD_BLOCK(VDST, PTR)                                        \
  asm volatile("vsetvli zero, %[vl], e16, m1, ta, ma\n"                       \
               "vle16.v " VDST ", (%[p])\n"                                   \
               :                                                              \
               : [vl] "r"(CB_D), [p] "r"(PTR)                                 \
               : VDST, "memory")

void vq_dequantize_vle_materialized(__fp16 *b, const __fp16 *b_cb0,
                                    const __fp16 *b_cb1,
                                    const uint8_t *b_idx0,
                                    const uint8_t *b_idx1,
                                    const __fp16 *b_scales,
                                    const unsigned int k_start,
                                    const unsigned int k_end,
                                    const unsigned int N) {
  const unsigned int groups = N / CB_D;

  if ((k_start >= k_end) || (groups == 0))
    return;

  for (unsigned int k = k_start; k < k_end; ++k) {
    const uint8_t *idx0 = b_idx0 + k * groups;
    const uint8_t *idx1 = b_idx1 + k * groups;
    __fp16 *b_row = b + k * N;
    float scale;
    asm volatile("flh %[s], 0(%[scale])" : [s] "=f"(scale)
                 : [scale] "r"(b_scales + k));

    unsigned int g = 0;

    // Main loop: 4 blocks = 4 consecutive m1 registers = one m4 group.
    for (; (g + 4) <= groups; g += 4) {
      // cb0 blocks land in v16..v19: consecutive destination registers do
      // the packing, no slides needed.
      VQ_VLE32_LOAD_BLOCK("v16", b_cb0 + ((unsigned int)idx0[g + 0] * CB_D));
      VQ_VLE32_LOAD_BLOCK("v17", b_cb0 + ((unsigned int)idx0[g + 1] * CB_D));
      VQ_VLE32_LOAD_BLOCK("v18", b_cb0 + ((unsigned int)idx0[g + 2] * CB_D));
      VQ_VLE32_LOAD_BLOCK("v19", b_cb0 + ((unsigned int)idx0[g + 3] * CB_D));
      // cb1 blocks land in v20..v23.
      VQ_VLE32_LOAD_BLOCK("v20", b_cb1 + ((unsigned int)idx1[g + 0] * CB_D));
      VQ_VLE32_LOAD_BLOCK("v21", b_cb1 + ((unsigned int)idx1[g + 1] * CB_D));
      VQ_VLE32_LOAD_BLOCK("v22", b_cb1 + ((unsigned int)idx1[g + 2] * CB_D));
      VQ_VLE32_LOAD_BLOCK("v23", b_cb1 + ((unsigned int)idx1[g + 3] * CB_D));

      // Arithmetic + store as one m4 group (128 e16 elements).
      asm volatile("vsetvli zero, %[vl4], e16, m4, ta, ma\n"
                   "vfadd.vv v16, v16, v20\n"
                   "vfmul.vf v16, v16, %[scale]\n"
                   "vse16.v v16, (%[dst])\n"
                   :
                   : [vl4] "r"(4 * CB_D), [scale] "f"(scale),
                     [dst] "r"(b_row + g * CB_D)
                   : "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                     "memory");
    }

    // Tail: leftover blocks one m1 register at a time (not hit at N=128).
    for (; g < groups; ++g) {
      VQ_VLE32_LOAD_BLOCK("v16", b_cb0 + ((unsigned int)idx0[g] * CB_D));
      VQ_VLE32_LOAD_BLOCK("v20", b_cb1 + ((unsigned int)idx1[g] * CB_D));
      asm volatile("vfadd.vv v16, v16, v20\n"
                   "vfmul.vf v16, v16, %[scale]\n"
                   "vse16.v v16, (%[dst])\n"
                   :
                   : [scale] "f"(scale), [dst] "r"(b_row + g * CB_D)
                   : "v16", "v20", "memory");
    }
  }
}

#elif CB_D == 8

// One 16-B quarter-register block: lbu idx / slli 4 / add base (compiler),
// then vle16 vl=8 into a scratch register and a serializing vslideup.vx by
// 8*j into the accumulating register (vl = 8*(j+1), tail-undisturbed so
// already-packed lower blocks survive).
#define VQ_VLE8_PACK_BLOCK(VACC, VSCR, PTR, J)                                \
  asm volatile("vsetvli zero, %[vl8], e16, m1, ta, ma\n"                      \
               "vle16.v " VSCR ", (%[p])\n"                                   \
               "vsetvli zero, %[vl], e16, m1, tu, ma\n"                       \
               "vslideup.vx " VACC ", " VSCR ", %[off]\n"                     \
               :                                                              \
               : [vl8] "r"(CB_D), [p] "r"(PTR),                               \
                 [vl] "r"(CB_D * ((J) + 1)), [off] "r"(CB_D * (J))            \
               : VACC, VSCR, "memory")

// Pack 4 consecutive 8-element blocks of one codebook into one m1 register
// (32 e16 elements).
#define VQ_VLE8_PACK_REG(VACC, VSCR, CB, IDXP)                                \
  do {                                                                        \
    VQ_VLE8_PACK_BLOCK(VACC, VSCR, (CB) + ((unsigned int)(IDXP)[0] * CB_D),   \
                       0);                                                    \
    VQ_VLE8_PACK_BLOCK(VACC, VSCR, (CB) + ((unsigned int)(IDXP)[1] * CB_D),   \
                       1);                                                    \
    VQ_VLE8_PACK_BLOCK(VACC, VSCR, (CB) + ((unsigned int)(IDXP)[2] * CB_D),   \
                       2);                                                    \
    VQ_VLE8_PACK_BLOCK(VACC, VSCR, (CB) + ((unsigned int)(IDXP)[3] * CB_D),   \
                       3);                                                    \
  } while (0)

void vq_dequantize_vle_materialized(__fp16 *b, const __fp16 *b_cb0,
                                    const __fp16 *b_cb1,
                                    const uint8_t *b_idx0,
                                    const uint8_t *b_idx1,
                                    const __fp16 *b_scales,
                                    const unsigned int k_start,
                                    const unsigned int k_end,
                                    const unsigned int N) {
  const unsigned int groups = N / CB_D;

  if ((k_start >= k_end) || (groups == 0))
    return;

  for (unsigned int k = k_start; k < k_end; ++k) {
    const uint8_t *idx0 = b_idx0 + k * groups;
    const uint8_t *idx1 = b_idx1 + k * groups;
    __fp16 *b_row = b + k * N;
    float scale;
    asm volatile("flh %[s], 0(%[scale])" : [s] "=f"(scale)
                 : [scale] "r"(b_scales + k));

    unsigned int g = 0;

    // Main loop: one m1 register (32 elems = 4 blocks) per codebook,
    // packed with a vslideup chain, then m1 arithmetic + store.
    for (; (g + 4) <= groups; g += 4) {
      VQ_VLE8_PACK_REG("v16", "v24", b_cb0, idx0 + g);
      VQ_VLE8_PACK_REG("v20", "v25", b_cb1, idx1 + g);

      asm volatile("vsetvli zero, %[vl32], e16, m1, ta, ma\n"
                   "vfadd.vv v16, v16, v20\n"
                   "vfmul.vf v16, v16, %[scale]\n"
                   "vse16.v v16, (%[dst])\n"
                   :
                   : [vl32] "r"(4 * CB_D), [scale] "f"(scale),
                     [dst] "r"(b_row + g * CB_D)
                   : "v16", "v20", "memory");
    }

    // Tail: leftover blocks per-block at vl=8 (not hit at N=128).
    for (; g < groups; ++g) {
      const __fp16 *cb0 = b_cb0 + ((unsigned int)idx0[g] * CB_D);
      const __fp16 *cb1 = b_cb1 + ((unsigned int)idx1[g] * CB_D);
      asm volatile("vsetvli zero, %[vl8], e16, m1, ta, ma\n"
                   "vle16.v  v24, (%[cb0])\n"
                   "vle16.v  v25, (%[cb1])\n"
                   "vfadd.vv v26, v24, v25\n"
                   "vfmul.vf v26, v26, %[scale]\n"
                   "vse16.v  v26, (%[dst])\n"
                   :
                   : [vl8] "r"(CB_D), [cb0] "r"(cb0), [cb1] "r"(cb1),
                     [scale] "f"(scale), [dst] "r"(b_row + g * CB_D)
                   : "v24", "v25", "v26", "memory");
    }
  }
}

#else

// Other block lengths: the vle scalar-loop variant is only implemented for
// CB_D in {8, 32}; fall back to the plain rvv variant.
void vq_dequantize_vle_materialized(__fp16 *b, const __fp16 *b_cb0,
                                    const __fp16 *b_cb1,
                                    const uint8_t *b_idx0,
                                    const uint8_t *b_idx1,
                                    const __fp16 *b_scales,
                                    const unsigned int k_start,
                                    const unsigned int k_end,
                                    const unsigned int N) {
  vq_dequantize_rvv_materialized(b, b_cb0, b_cb1, b_idx0, b_idx1, b_scales,
                                 k_start, k_end, N);
}

#endif

static inline void vq_dense_matmul_materialized_scalar_tail(
    __fp16 *c, const __fp16 *a, const __fp16 *b, const unsigned int m_start,
    const unsigned int m_end, const unsigned int N, const unsigned int P,
    const unsigned int p_start, const unsigned int p_end) {
  for (unsigned int m = m_start; m < m_end; ++m) {
    for (unsigned int p = p_start; p < p_end; ++p) {
      float acc = 0.0f;
      for (unsigned int n = 0; n < N; ++n) {
        acc += (float)a[m * N + n] * (float)b[n * P + p];
      }
      c[m * P + p] = (__fp16)acc;
    }
  }
}

void vq_dense_matmul_materialized(__fp16 *c, const __fp16 *a, const __fp16 *b,
                                  const unsigned int m_start,
                                  const unsigned int m_end,
                                  const unsigned int N, const unsigned int P,
                                  const unsigned int p_start,
                                  const unsigned int p_end) {
  unsigned int m = m_start;

  while ((m + 8) <= m_end) {
    matmul_8xVL(c, a, b, m, m + 8, N, P, p_start, p_end);
    m += 8;
  }

  while ((m + 4) <= m_end) {
    matmul_4xVL(c, a, b, m, m + 4, N, P, p_start, p_end);
    m += 4;
  }

  while ((m + 2) <= m_end) {
    matmul_2xVL(c, a, b, m, m + 2, N, P, p_start, p_end);
    m += 2;
  }

  if (m < m_end) {
    vq_dense_matmul_materialized_scalar_tail(c, a, b, m, m_end, N, P, p_start,
                                             p_end);
  }
}

void vq_dense_gemv_materialized(__fp16 *c, const __fp16 *a, const __fp16 *b,
                                const unsigned int K, const unsigned int N,
                                const unsigned int n_start,
                                const unsigned int n_end) {
  unsigned int n = n_start;

  while (n < n_end) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e16, m4, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(n_end - n));

    asm volatile("vmv.v.x v0, zero" ::: "v0");

    for (unsigned int k = 0; k < K; ++k) {
      float av;
      const __fp16 *b_row = b + k * N + n;

      asm volatile("flh %[av], 0(%[a])" : [av] "=f"(av) : [a] "r"(a + k));
      asm volatile("vle16.v v16, (%[b])\n"
                   "vfmacc.vf v0, %[av], v16\n"
                   :
                   : [b] "r"(b_row), [av] "f"(av)
                   : "v16", "memory");
    }

    asm volatile("vse16.v v0, (%[c])" : : [c] "r"(c + n) : "memory");
    n += gvl;
  }
}
