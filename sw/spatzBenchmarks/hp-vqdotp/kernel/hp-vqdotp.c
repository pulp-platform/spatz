// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include "hp-vqdotp.h"

#ifndef VQ_BLOCK_LEN
#define VQ_BLOCK_LEN 8
#endif

#ifndef VQDOTP_USE_VLXBLK
#define VQDOTP_USE_VLXBLK 0
#endif

#ifndef VQDOTP_PROFILE_REDUCE
#define VQDOTP_PROFILE_REDUCE 0
#endif

#ifndef VQDOTP_VLXBLK_UNROLL
#define VQDOTP_VLXBLK_UNROLL 4
#endif

#define VLXBLK_EI8 "vlxblkei8.v"
#define CB_D VQ_BLOCK_LEN

#if VQDOTP_PROFILE_REDUCE
extern volatile unsigned int vqdotp_reduce_cycles;
extern volatile unsigned int vqdotp_reduce_count;
extern volatile unsigned int vqdotp_reduce_timer_overhead;
#define VQDOTP_PROFILE_REDUCE_START()                                         \
  unsigned int vqdotp_timer_start = benchmark_get_cycle();                    \
  unsigned int vqdotp_timer_end = benchmark_get_cycle();                      \
  vqdotp_reduce_timer_overhead += vqdotp_timer_end - vqdotp_timer_start;      \
  unsigned int vqdotp_reduce_start = benchmark_get_cycle()
#define VQDOTP_PROFILE_REDUCE_STOP(count)                                      \
  do {                                                                         \
    unsigned int vqdotp_reduce_end = benchmark_get_cycle();                    \
    vqdotp_reduce_cycles += vqdotp_reduce_end - vqdotp_reduce_start;           \
    vqdotp_reduce_count += (count);                                            \
  } while (0)
#else
#define VQDOTP_PROFILE_REDUCE_START()                                          \
  do {                                                                         \
  } while (0)
#define VQDOTP_PROFILE_REDUCE_STOP(count)                                      \
  do {                                                                         \
  } while (0)
#endif

void vqdotp_rvv(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                const __fp16 *b_cb1, const uint8_t *b_idx0,
                const uint8_t *b_idx1, const __fp16 *b_scales,
                const unsigned int K, const unsigned int N,
                const unsigned int n_start, const unsigned int n_end) {
  (void)N;
  (void)b_cb1;
  (void)b_idx1;
  const unsigned int kblocks = K / CB_D;

  if ((n_start >= n_end) || (kblocks == 0))
    return;

  for (unsigned int n = n_start; n < n_end;) {
    const unsigned int cols =
        ((n_end - n) < VQDOTP_VLXBLK_UNROLL) ? (n_end - n)
                                             : VQDOTP_VLXBLK_UNROLL;

    if (cols == VQDOTP_VLXBLK_UNROLL) {
      _Float16 red0, red1, red2, red3;

      for (unsigned int kb = 0; kb < kblocks; ++kb) {
        const __fp16 *a_block = a + kb * CB_D;
        const __fp16 *cb0 = b_cb0 + ((unsigned int)b_idx0[(n + 0) * kblocks + kb] * CB_D);
        const __fp16 *cb1 = b_cb0 + ((unsigned int)b_idx0[(n + 1) * kblocks + kb] * CB_D);
        const __fp16 *cb2 = b_cb0 + ((unsigned int)b_idx0[(n + 2) * kblocks + kb] * CB_D);
        const __fp16 *cb3 = b_cb0 + ((unsigned int)b_idx0[(n + 3) * kblocks + kb] * CB_D);

        if (kb == 0) {
          asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       "vle16.v v4, (%[a])\n"
                       "vle16.v v6, (%[cb0])\n"
                       "vle16.v v8, (%[cb1])\n"
                       "vle16.v v10, (%[cb2])\n"
                       "vle16.v v12, (%[cb3])\n"
                       "vfmul.vv v14, v4, v6\n"
                       "vfmul.vv v16, v4, v8\n"
                       "vfmul.vv v18, v4, v10\n"
                       "vfmul.vv v20, v4, v12\n"
                       :
                       : [vl] "r"(CB_D), [a] "r"(a_block), [cb0] "r"(cb0),
                         [cb1] "r"(cb1), [cb2] "r"(cb2), [cb3] "r"(cb3)
                       : "v4", "v6", "v8", "v10", "v12", "v14",
                         "v16", "v18", "v20",
                         "memory");
        } else {
          asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       "vle16.v v4, (%[a])\n"
                       "vle16.v v6, (%[cb0])\n"
                       "vle16.v v8, (%[cb1])\n"
                       "vle16.v v10, (%[cb2])\n"
                       "vle16.v v12, (%[cb3])\n"
                       "vfmacc.vv v14, v4, v6\n"
                       "vfmacc.vv v16, v4, v8\n"
                       "vfmacc.vv v18, v4, v10\n"
                       "vfmacc.vv v20, v4, v12\n"
                       :
                       : [vl] "r"(CB_D), [a] "r"(a_block), [cb0] "r"(cb0),
                         [cb1] "r"(cb1), [cb2] "r"(cb2), [cb3] "r"(cb3)
                       : "v4", "v6", "v8", "v10", "v12", "v14",
                         "v16", "v18", "v20",
                         "memory");
        }
      }

      VQDOTP_PROFILE_REDUCE_START();
      asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                   "vmv.s.x v0, zero\n"
                   "vfredusum.vs v0, v14, v0\n"
                   :
                   : [vl] "r"(CB_D)
                   : "v0", "memory");
      asm volatile("vfmv.f.s %0, v0" : "=f"(red0));
      asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                   "vmv.s.x v0, zero\n"
                   "vfredusum.vs v0, v16, v0\n"
                   :
                   : [vl] "r"(CB_D)
                   : "v0", "memory");
      asm volatile("vfmv.f.s %0, v0" : "=f"(red1));
      asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                   "vmv.s.x v0, zero\n"
                   "vfredusum.vs v0, v18, v0\n"
                   :
                   : [vl] "r"(CB_D)
                   : "v0", "memory");
      asm volatile("vfmv.f.s %0, v0" : "=f"(red2));
      asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                   "vmv.s.x v0, zero\n"
                   "vfredusum.vs v0, v20, v0\n"
                   :
                   : [vl] "r"(CB_D)
                   : "v0", "memory");
      asm volatile("vfmv.f.s %0, v0" : "=f"(red3));
      VQDOTP_PROFILE_REDUCE_STOP(4);

      c[n + 0] = (__fp16)((float)red0 * (float)b_scales[n + 0]);
      c[n + 1] = (__fp16)((float)red1 * (float)b_scales[n + 1]);
      c[n + 2] = (__fp16)((float)red2 * (float)b_scales[n + 2]);
      c[n + 3] = (__fp16)((float)red3 * (float)b_scales[n + 3]);

      n += VQDOTP_VLXBLK_UNROLL;
      continue;
    }

    // Reduce into v0 and extract with vfmv.f.s, exactly like fdotp.c.
    //
    // Two things here are load-bearing.  The reduction destination must be v0:
    // at LMUL=8 the only legal group bases are v0/v8/v16/v24, and v8/v16 are
    // rewritten by the loads below, so reducing into either of those and then
    // reading it back with a vector op returns the stale loaded value.  v0 is
    // written only by the vmv.s.x seed.  And the scale is applied in scalar FP
    // rather than with a vector multiply, so no vector op reads the reduction's
    // destination at all.
    _Float16 red;

    asm volatile("vsetvli zero, %[vl], e16, m8, ta, ma\n"
                 :
                 : [vl] "r"(CB_D)
                 : "memory");

    for (unsigned int kb = 0; kb < kblocks; ++kb) {
      const unsigned int idx = n * kblocks + kb;
      const __fp16 *a_block = a + kb * CB_D;
      const __fp16 *cb0 = b_cb0 + ((unsigned int)b_idx0[idx] * CB_D);

      asm volatile("vsetvli zero, %[vl], e16, m8, ta, ma\n"
                   "vle16.v v8, (%[a])\n"
                   "vle16.v v16, (%[cb0])\n"
                   :
                   : [vl] "r"(CB_D), [a] "r"(a_block), [cb0] "r"(cb0)
                   : "v8", "v16", "memory");

      if (kb == 0)
        asm volatile("vfmul.vv v24, v8, v16" ::: "v24", "memory");
      else
        asm volatile("vfmacc.vv v24, v8, v16" ::: "v24", "memory");
    }

    VQDOTP_PROFILE_REDUCE_START();
    asm volatile("vsetvli zero, %[vl], e16, m8, ta, ma\n"
                 "vmv.s.x v0, zero\n"
                 "vfredusum.vs v0, v24, v0\n"
                 :
                 : [vl] "r"(CB_D)
                 : "v0", "memory");
    asm volatile("vfmv.f.s %0, v0" : "=f"(red));
    VQDOTP_PROFILE_REDUCE_STOP(1);

    c[n] = (__fp16)((float)red * (float)b_scales[n]);
    ++n;
  }
}

void vqdotp_vlxblk(__fp16 *c, const __fp16 *a, const __fp16 *b_cb0,
                   const __fp16 *b_cb1, const uint8_t *b_idx0,
                   const uint8_t *b_idx1, const __fp16 *b_scales,
                   const unsigned int K, const unsigned int N,
                   const unsigned int n_start, const unsigned int n_end) {
  (void)N;
  (void)b_cb1;
  (void)b_idx1;
  const unsigned int kblocks = K / CB_D;

  if ((n_start >= n_end) || (kblocks == 0))
    return;

  register const __fp16 *cb0_reg asm("t2") = b_cb0;
  asm volatile("" ::"r"(cb0_reg));
  asm volatile("vsetblklen %0" ::"r"(CB_D));

  for (unsigned int n = n_start; n < n_end;) {
    const unsigned int cols =
        ((n_end - n) < VQDOTP_VLXBLK_UNROLL) ? (n_end - n)
                                             : VQDOTP_VLXBLK_UNROLL;

    if (cols == VQDOTP_VLXBLK_UNROLL) {
      _Float16 red0, red1, red2, red3;
      unsigned int avl = K;
      unsigned int kb = 0;
      unsigned int reduce_vl = 0;
      int first = 1;

      do {
        unsigned int chunk_vl;
        asm volatile("vsetvli %[chunk_vl], %[avl], e16, m2, ta, ma"
                     : [chunk_vl] "=r"(chunk_vl)
                     : [avl] "r"(avl));

        const unsigned int chunk_blocks = chunk_vl / CB_D;
        const __fp16 *a_block = a + kb * CB_D;
        const uint8_t *idx0 = b_idx0 + (n + 0) * kblocks + kb;
        const uint8_t *idx1 = b_idx0 + (n + 1) * kblocks + kb;
        const uint8_t *idx2 = b_idx0 + (n + 2) * kblocks + kb;
        const uint8_t *idx3 = b_idx0 + (n + 3) * kblocks + kb;

        if (first)
          reduce_vl = chunk_vl;

        asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                     "vle16.v v8, (%[a])\n"
                     :
                     : [vl] "r"(chunk_vl), [a] "r"(a_block)
                     : "v8", "memory");

        if (first) {
          asm volatile("vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                       "vle8.v v0, (%[idx0])\n"
                       "vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       VLXBLK_EI8 " v10, (%[cb0]), v0\n"
                       "vfmul.vv v16, v8, v10\n"
                       "vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                       "vle8.v v0, (%[idx1])\n"
                       "vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       VLXBLK_EI8 " v10, (%[cb0]), v0\n"
                       "vfmul.vv v18, v8, v10\n"
                       "vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                       "vle8.v v0, (%[idx2])\n"
                       "vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       VLXBLK_EI8 " v10, (%[cb0]), v0\n"
                       "vfmul.vv v20, v8, v10\n"
                       "vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                       "vle8.v v0, (%[idx3])\n"
                       "vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       VLXBLK_EI8 " v10, (%[cb0]), v0\n"
                       "vfmul.vv v22, v8, v10\n"
                       :
                       : [idx_vl] "r"(chunk_blocks), [vl] "r"(chunk_vl),
                         [idx0] "r"(idx0), [idx1] "r"(idx1),
                         [idx2] "r"(idx2), [idx3] "r"(idx3),
                         [cb0] "r"(cb0_reg)
                       : "v0", "v10", "v16", "v18", "v20", "v22",
                         "memory");
          first = 0;
        } else {
          asm volatile("vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                       "vle8.v v0, (%[idx0])\n"
                       "vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       VLXBLK_EI8 " v10, (%[cb0]), v0\n"
                       "vfmacc.vv v16, v8, v10\n"
                       "vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                       "vle8.v v0, (%[idx1])\n"
                       "vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       VLXBLK_EI8 " v10, (%[cb0]), v0\n"
                       "vfmacc.vv v18, v8, v10\n"
                       "vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                       "vle8.v v0, (%[idx2])\n"
                       "vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       VLXBLK_EI8 " v10, (%[cb0]), v0\n"
                       "vfmacc.vv v20, v8, v10\n"
                       "vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                       "vle8.v v0, (%[idx3])\n"
                       "vsetvli zero, %[vl], e16, m2, ta, ma\n"
                       VLXBLK_EI8 " v10, (%[cb0]), v0\n"
                       "vfmacc.vv v22, v8, v10\n"
                       :
                       : [idx_vl] "r"(chunk_blocks), [vl] "r"(chunk_vl),
                         [idx0] "r"(idx0), [idx1] "r"(idx1),
                         [idx2] "r"(idx2), [idx3] "r"(idx3),
                         [cb0] "r"(cb0_reg)
                       : "v0", "v10", "v16", "v18", "v20", "v22",
                         "memory");
        }

        avl -= chunk_vl;
        kb += chunk_blocks;
      } while (avl > 0);

      VQDOTP_PROFILE_REDUCE_START();
      asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                   "vmv.s.x v0, zero\n"
                   "vfredusum.vs v0, v16, v0\n"
                   :
                   : [vl] "r"(reduce_vl)
                   : "v0", "memory");
      asm volatile("vfmv.f.s %0, v0" : "=f"(red0));
      asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                   "vmv.s.x v0, zero\n"
                   "vfredusum.vs v0, v18, v0\n"
                   :
                   : [vl] "r"(reduce_vl)
                   : "v0", "memory");
      asm volatile("vfmv.f.s %0, v0" : "=f"(red1));
      asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                   "vmv.s.x v0, zero\n"
                   "vfredusum.vs v0, v20, v0\n"
                   :
                   : [vl] "r"(reduce_vl)
                   : "v0", "memory");
      asm volatile("vfmv.f.s %0, v0" : "=f"(red2));
      asm volatile("vsetvli zero, %[vl], e16, m2, ta, ma\n"
                   "vmv.s.x v0, zero\n"
                   "vfredusum.vs v0, v22, v0\n"
                   :
                   : [vl] "r"(reduce_vl)
                   : "v0", "memory");
      asm volatile("vfmv.f.s %0, v0" : "=f"(red3));
      VQDOTP_PROFILE_REDUCE_STOP(4);

      c[n + 0] = (__fp16)((float)red0 * (float)b_scales[n + 0]);
      c[n + 1] = (__fp16)((float)red1 * (float)b_scales[n + 1]);
      c[n + 2] = (__fp16)((float)red2 * (float)b_scales[n + 2]);
      c[n + 3] = (__fp16)((float)red3 * (float)b_scales[n + 3]);

      n += VQDOTP_VLXBLK_UNROLL;
      continue;
    }

    _Float16 red;

    asm volatile("vsetvli zero, %[vl], e16, m8, ta, ma\n"
                 :
                 : [vl] "r"(CB_D)
                 : "memory");

    for (unsigned int kb = 0; kb < kblocks; ++kb) {
      const unsigned int idx = n * kblocks + kb;
      const __fp16 *a_block = a + kb * CB_D;

      asm volatile("vsetvli zero, %[idx_vl], e8, m1, ta, ma\n"
                   "vle8.v v0, (%[idx])\n"
                   "vsetvli zero, %[vl], e16, m8, ta, ma\n"
                   "vle16.v v8, (%[a])\n"
                   VLXBLK_EI8 " v16, (%[cb0]), v0\n"
                   :
                   : [idx_vl] "r"(1), [vl] "r"(CB_D), [a] "r"(a_block),
                     [idx] "r"(b_idx0 + idx), [cb0] "r"(cb0_reg)
                   : "v0", "v8", "v16", "memory");

      if (kb == 0)
        asm volatile("vfmul.vv v24, v8, v16" ::: "v24", "memory");
      else
        asm volatile("vfmacc.vv v24, v8, v16" ::: "v24", "memory");
    }

    VQDOTP_PROFILE_REDUCE_START();
    asm volatile("vsetvli zero, %[vl], e16, m8, ta, ma\n"
                 "vmv.s.x v0, zero\n"
                 "vfredusum.vs v0, v24, v0\n"
                 :
                 : [vl] "r"(CB_D)
                 : "v0", "memory");
    asm volatile("vfmv.f.s %0, v0" : "=f"(red));
    VQDOTP_PROFILE_REDUCE_STOP(1);

    c[n] = (__fp16)((float)red * (float)b_scales[n]);
    ++n;
  }
}
