// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include "hp-vqgemv.h"
#include <stddef.h>

#ifndef VQ_BLOCK_LEN
#define VQ_BLOCK_LEN 8
#endif

#define VLXBLK_EI8 "vlxblkei8.v"

#define CB_D VQ_BLOCK_LEN

void vq_gemv_vlxblk(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                   const __fp16 *b_cb1, const uint8_t *b_idx0,
                   const uint8_t *b_idx1, const __fp16 *b_scales,
                   const unsigned int K, const unsigned int N,
                   const unsigned int group_start,
                   const unsigned int group_end) {
  const unsigned int groups = N / CB_D;

  if ((group_start >= group_end) || (groups == 0))
    return;

  register const __fp16 *cb0_reg asm("t2") = b_cb0;
  register const __fp16 *cb1_reg asm("t3") = b_cb1;
  asm volatile("" ::"r"(cb0_reg), "r"(cb1_reg));
  asm volatile("vsetblklen %0" ::"r"(CB_D));

  for (unsigned int g = group_start; g < group_end;) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e16, m4, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"((group_end - g) * CB_D));

    const unsigned int group_vl = gvl / CB_D;
    asm volatile("vmv.v.x v0, zero" ::: "v0");

    for (unsigned int k = 0; k < K; ++k) {
      float av;
      float scale;
      asm volatile("flh %[av], 0(%[a])" : [av] "=f"(av) : [a] "r"(a + k));
      asm volatile("flh %[s], 0(%[scale])"
                   : [s] "=f"(scale)
                   : [scale] "r"(b_scales + k));

      asm volatile("vsetvli zero, %[group_vl], e8, m2, ta, ma\n"
                   "vle8.v v28, (%[idx0])\n"
                   "vle8.v v30, (%[idx1])\n"
                   "vsetvli zero, %[gvl], e16, m4, ta, ma\n"
                   VLXBLK_EI8 " v16, (%[cb0]), v28\n"
                   VLXBLK_EI8 " v20, (%[cb1]), v30\n"
                   "vfadd.vv v16, v16, v20\n"
                   "vfmul.vf v16, v16, %[scale]\n"
                   "vfmacc.vf v0, %[av], v16\n"
                   :
                   : [group_vl] "r"(group_vl), [gvl] "r"(gvl),
                     [idx0] "r"(b_idx0 + k * groups + g),
                     [idx1] "r"(b_idx1 + k * groups + g),
                     [cb0] "r"(cb0_reg), [cb1] "r"(cb1_reg),
                     [scale] "f"(scale), [av] "f"(av)
                   : "v16", "v20", "v28", "v30", "memory");
    }

    asm volatile("vse16.v v0, (%0)" ::"r"(c + g * CB_D) : "memory");
    g += group_vl;
  }
}

void vq_gemv_rvv(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                 const __fp16 *b_cb1, const uint8_t *b_idx0,
                 const uint8_t *b_idx1, const __fp16 *b_scales,
                 const unsigned int K, const unsigned int N,
                 const unsigned int group_start,
                 const unsigned int group_end) {
  const unsigned int groups = N / CB_D;

  if ((group_start >= group_end) || (groups == 0))
    return;

  asm volatile("vsetvli zero, %0, e16, m1, ta, ma" ::"r"(CB_D));

  for (unsigned int g = group_start; g < group_end; ++g) {
    asm volatile("vmv.v.x v0, zero" ::: "v0");

    for (unsigned int k = 0; k < K; ++k) {
      const unsigned int idx = k * groups + g;
      const __fp16 *cb0 = b_cb0 + ((unsigned int)b_idx0[idx] * CB_D);
      const __fp16 *cb1 = b_cb1 + ((unsigned int)b_idx1[idx] * CB_D);
      float av;
      float scale;

      asm volatile("flh %[av], 0(%[a])" : [av] "=f"(av) : [a] "r"(a + k));
      asm volatile("flh %[s], 0(%[scale])"
                   : [s] "=f"(scale)
                   : [scale] "r"(b_scales + k));

      asm volatile("vle16.v v16, (%[cb0])\n"
                   "vle16.v v20, (%[cb1])\n"
                   "vfadd.vv v16, v16, v20\n"
                   "vfmul.vf v16, v16, %[scale]\n"
                   "vfmacc.vf v0, %[av], v16\n"
                   :
                   : [cb0] "r"(cb0), [cb1] "r"(cb1),
                     [scale] "f"(scale), [av] "f"(av)
                   : "v16", "v20", "memory");
    }

    asm volatile("vse16.v v0, (%0)" ::"r"(c + g * CB_D) : "memory");
  }
}
