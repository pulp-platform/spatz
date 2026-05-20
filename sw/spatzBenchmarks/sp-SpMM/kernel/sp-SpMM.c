// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#include <stddef.h>
#include <stdint.h>

#include "sp-SpMM.h"

// ===========================================================================
// Ventaglio (vfx) implementation — spmm_ventaglio
// ===========================================================================
//
// CSR addresses for Ventaglio sparse-format configuration.
//   0x7c3: VTL vreg bitmap (which vregs are mapped to Ventaglio's bank)
//   0x7c4: index width (encoded: IDXW_1=0, IDXW_2=1, IDXW_4=2, IDXW_8=3)
//   0x7c5: block size  (encoded: BLK_1=0,  BLK_2=1,  BLK_4=2,  BLK_8=3)
//   0x7c6: sparsity ratio (encoded: SP_050=1 for 2:4, SP_025=2 for 1:4)

static inline uint32_t bit_if_valid(uint32_t vr) {
  return (vr < 32) ? (1u << vr) : 0u;
}

static inline void vtl_cfg(uint32_t vr0, uint32_t vr1, uint32_t vr2, uint32_t vr3) {
  uint32_t bitmap = bit_if_valid(vr0) | bit_if_valid(vr1)
                  | bit_if_valid(vr2) | bit_if_valid(vr3);
  asm volatile("csrrw x0, 0x7c3, %0" :: "r"(bitmap) : "memory");
}

static inline void sparse_fmt_cfg(uint32_t idx_width, uint32_t m_sparse, uint32_t n_sparse) {
  uint32_t idx_enc   = (idx_width == 1) ? 0u : (idx_width == 2) ? 1u
                      : (idx_width == 4) ? 2u : 3u;
  uint32_t blk_enc   = (m_sparse  == 1) ? 0u : (m_sparse  == 2) ? 1u
                      : (m_sparse  == 4) ? 2u : 3u;
  uint32_t ratio_enc = ((m_sparse / n_sparse) == 2u) ? 1u : 2u;

  asm volatile("csrrw x0, 0x7c4, %0" :: "r"(idx_enc)   : "memory");
  asm volatile("csrrw x0, 0x7c5, %0" :: "r"(blk_enc)   : "memory");
  asm volatile("csrrw x0, 0x7c6, %0" :: "r"(ratio_enc) : "memory");
}

void spmm_ventaglio(float *res,
                    const float *a,
                    const float *w,
                    const uint32_t *nm_index,
                    uint32_t M,
                    uint32_t N,
                    uint32_t P,
                    uint32_t P_W,
                    uint32_t NM_INDEX_ROW_WORDS,
                    uint32_t idx_width,
                    uint32_t m_sparse,
                    uint32_t n_sparse) {

  // Two accumulators (M=2 unrolling), v8/v4 weights, v20/v24 indices.
  vtl_cfg(16, 18, 32, 32);
  sparse_fmt_cfg(idx_width, m_sparse, n_sparse);

  const intptr_t idx_stride_bytes   = (intptr_t)NM_INDEX_ROW_WORDS * sizeof(uint32_t);
  const intptr_t w_stride_bytes     = (intptr_t)P_W                * sizeof(float);
  const intptr_t a_stride_bytes     = (intptr_t)N                  * sizeof(float);
  const intptr_t a_stride_bytes_1_n = (intptr_t)(1 - (intptr_t)N)  * sizeof(float);

  unsigned int p = 0;
  while (p < P_W) {
    size_t gvl;
    asm volatile("vsetvli %[gvl], %[vl], e32, m2, ta, ma"
                 : [gvl] "=r"(gvl)
                 : [vl] "r"(P_W - p));

    const float    *w_   = w + p;
    const uint32_t *idx_ = nm_index + p / (sizeof(uint32_t) * 8u / idx_width);
    // Dense column stride per compact column: m/n.
    float          *res_ = res + p * (m_sparse / n_sparse);

    for (uint32_t m = 0; m < M; m += 2) {
      const float *a__ = a + m * N;

      // Clear the Ventaglio bank for this (m, m+1) pair.
      asm volatile("vventclr" ::: "memory");

      // Preload n=0 (idx → v20, weight → v8) and n=1 (idx → v24, weight → v4).
      const uint32_t *idx__ = idx_;
      const float    *w__   = w_;
      asm volatile("p.vlx32.v.rrpost v20, (%0), %1"
                   : "+r"(idx__) : "r"(idx_stride_bytes) : "memory");
      asm volatile("p.vle32.v.rrpost v8,  (%0), %1"
                   : "+r"(w__)   : "r"(w_stride_bytes)   : "memory");
      asm volatile("p.vlx32.v.rrpost v24, (%0), %1"
                   : "+r"(idx__) : "r"(idx_stride_bytes) : "memory");
      asm volatile("p.vle32.v.rrpost v4,  (%0), %1"
                   : "+r"(w__)   : "r"(w_stride_bytes)   : "memory");

      float *res__ = res_ + m * P;
      float t0, t1;

      // Scalars for n=0
      asm volatile("p.flw.rrpost %0, (%1), %2"
                   : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("p.flw.rrpost %0, (%1), %2"
                   : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");

      // Prologue: n=0 — vfxmul scatters into the cleared bank.
      asm volatile("vfxmul.vrf v16, %0, v8, v20" :: "f"(t0));
      asm volatile("p.flw.rrpost %0, (%1), %2"
                   : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
      asm volatile("vfxmul.vrf v18, %0, v8, v20" :: "f"(t1));
      asm volatile("p.flw.rrpost %0, (%1), %2"
                   : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");

      unsigned int n = 1;

      // Software-pipelined body: each iteration consumes (n, n+1) using
      // v4/v24 then v8/v20, while pre-loading the next two N steps.
      while (n < N - 1) {
        // Reload v8/v20 with weights/index for n+1 (overwrites n-1 data)
        asm volatile("p.vlx32.v.rrpost v20, (%0), %1"
                     : "+r"(idx__) : "r"(idx_stride_bytes) : "memory");
        asm volatile("p.vle32.v.rrpost v8,  (%0), %1"
                     : "+r"(w__)   : "r"(w_stride_bytes)   : "memory");
        // Consume n (v4 = weight, v24 = index, t0/t1 = a[m..m+1][n])
        asm volatile("vfxmacc.vrf v16, %0, v4, v24" :: "f"(t0));
        asm volatile("p.flw.rrpost %0, (%1), %2"
                     : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
        asm volatile("vfxmacc.vrf v18, %0, v4, v24" :: "f"(t1));
        asm volatile("p.flw.rrpost %0, (%1), %2"
                     : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");

        // Prefetch weights/index for n+2 into v4/v24
        asm volatile("p.vlx32.v.rrpost v24, (%0), %1"
                     : "+r"(idx__) : "r"(idx_stride_bytes) : "memory");
        asm volatile("p.vle32.v.rrpost v4,  (%0), %1"
                     : "+r"(w__)   : "r"(w_stride_bytes)   : "memory");

        // Consume n+1 (v8 = weight, v20 = index, t0/t1 = a[m..m+1][n+1])
        asm volatile("vfxmacc.vrf v16, %0, v8, v20" :: "f"(t0));
        asm volatile("p.flw.rrpost %0, (%1), %2"
                     : "=f"(t0), "+r"(a__) : "r"(a_stride_bytes)     : "memory");
        asm volatile("addi %0, %0, 2"           : "+r"(n));
        asm volatile("vfxmacc.vrf v18, %0, v8, v20" :: "f"(t1));
        asm volatile("p.flw.rrpost %0, (%1), %2"
                     : "=f"(t1), "+r"(a__) : "r"(a_stride_bytes_1_n) : "memory");
      }

      // Final consume for n = N-1 (v4 / v24); t0/t1 already hold a[m..m+1][N-1]
      asm volatile("vfxmacc.vrf v16, %0, v4, v24" :: "f"(t0));
      asm volatile("vfxmacc.vrf v18, %0, v4, v24" :: "f"(t1));

      asm volatile("vse32.v v16, (%0)" :: "r"(res__) : "memory");
      res__ += P;
      asm volatile("vse32.v v18, (%0)" :: "r"(res__) : "memory");

      // Scalar load barrier: force vse32.v v18's bank gather + TCDM drain to
      // retire before the next iter's vventclr starts clearing the bank.
      // Without this, vventclr races with the in-flight gather and the gather
      // reads back already-cleared (zero) cells (manifests as row m+1 all-zero).
      volatile uint32_t _vse_drain = *((volatile uint32_t *)res__);
      (void)_vse_drain;
    }

    p += gvl;
  }
}

// ===========================================================================
// Baseline (RVV gather-modify-scatter) implementation — spmm_baseline
// ===========================================================================

// Unpack packed N:M indices for ONE sparse row into byte offsets that
// vluxei32/vsuxei32 can consume directly.
static inline void unpack_nm_to_byte_offsets(const uint32_t *packed,
                                             uint32_t       *offsets,
                                             uint32_t        pw,
                                             uint32_t        idx_width,
                                             uint32_t        m_sparse,
                                             uint32_t        n_sparse) {
  const uint32_t idx_mask = (1u << idx_width) - 1u;
  uint32_t bitpos = 0;

  for (uint32_t j = 0; j < pw; j++) {
    uint32_t widx = bitpos >> 5;       // bitpos / 32
    uint32_t boff = bitpos & 0x1fu;    // bitpos % 32
    uint32_t idx  = (packed[widx] >> boff) & idx_mask;

    uint32_t block     = j / n_sparse;
    uint32_t dense_pos = block * m_sparse + idx;
    offsets[j] = dense_pos * (uint32_t)sizeof(float);

    bitpos += idx_width;
  }
}

void spmm_baseline(float          *res,
                   const float    *a,
                   const float    *w,
                   const uint32_t *nm_index,
                   uint32_t       *byte_offsets,
                   uint32_t M,
                   uint32_t N,
                   uint32_t P,
                   uint32_t P_W,
                   uint32_t NM_INDEX_ROW_WORDS,
                   uint32_t idx_width,
                   uint32_t m_sparse,
                   uint32_t n_sparse) {
  (void)P;  // referenced via res-row stride below; suppress -Wunused if unused

  const float    *_w  = w;
  const uint32_t *_nm = nm_index;

  for (uint32_t n = 0; n < N; n++) {
    // Step 1: software N:M index translation for this sparse row.
    unpack_nm_to_byte_offsets(_nm, byte_offsets, P_W,
                              idx_width, m_sparse, n_sparse);

    // Step 2: tile across the compact dimension P_W.
    const float    *_w_tile  = _w;
    const uint32_t *_bo_tile = byte_offsets;
    uint32_t        avl      = P_W;

    do {
      uint32_t vl;
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

      // Load byte offsets + compact weights (shared across all M output rows).
      asm volatile("vle32.v v4, (%0)" :: "r"(_bo_tile) : "memory");
      asm volatile("vle32.v v8, (%0)" :: "r"(_w_tile)  : "memory");

      // Step 3: process M output rows, unrolled by 2. The two rows touch
      // independent memory (res + i*P vs res + (i+1)*P), so their gather/
      // fmacc/scatter chains can pipeline.
      // v4=offsets, v8=weights, v12=row_i, v16=row_i+1.
      for (uint32_t i = 0; i < M; i += 2) {
        float *res_row0 = res + i       * P;
        float *res_row1 = res + (i + 1) * P;

        float t0, t1;
        asm volatile("flw %0, (%1)" : "=f"(t0) : "r"(a + i       * N + n));
        asm volatile("flw %0, (%1)" : "=f"(t1) : "r"(a + (i + 1) * N + n));

        if (n == 0) {
          // First sparse row: v12/v16 have no initialized contents and Spatz
          // has no `vmv` to clear them, so seed with vfmul (write-only).
          // Skip the vluxei + vfmacc path to avoid accumulating into
          // uninitialized vregs.
          asm volatile("vfmul.vf v12, v8, %0" :: "f"(t0));
          asm volatile("vfmul.vf v16, v8, %0" :: "f"(t1));
        } else {
          // Subsequent rows: gather running partial sums, accumulate.
          asm volatile("vluxei32.v v12, (%0), v4" :: "r"(res_row0) : "memory");
          asm volatile("vluxei32.v v16, (%0), v4" :: "r"(res_row1) : "memory");
          asm volatile("vfmacc.vf v12, %0, v8" :: "f"(t0));
          asm volatile("vfmacc.vf v16, %0, v8" :: "f"(t1));
        }

        asm volatile("vsuxei32.v v12, (%0), v4" :: "r"(res_row0) : "memory");
        asm volatile("vsuxei32.v v16, (%0), v4" :: "r"(res_row1) : "memory");
      }

      _w_tile  += vl;
      _bo_tile += vl;
      avl      -= vl;
    } while (avl > 0);

    _w  += P_W;
    _nm += NM_INDEX_ROW_WORDS;
  }
}
