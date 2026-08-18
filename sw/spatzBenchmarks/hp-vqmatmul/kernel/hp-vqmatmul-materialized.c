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
