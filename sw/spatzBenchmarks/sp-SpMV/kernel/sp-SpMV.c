// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Bowen Wang <bowwang@iis.ee.ethz.ch>

#include "sp-SpMV.h"

// ===========================================================================
// Ventaglio (vfx) implementation — spmv_ventaglio
// ===========================================================================
//
// CSR addresses for Ventaglio sparse-format configuration.
//   0x7c3: VTL vreg bitmap (which vregs are mapped to Ventaglio's bank)
//   0x7c4: index width (encoded: IDXW_1=0, IDXW_2=1, IDXW_4=2, IDXW_8=3)
//   0x7c5: block size  (encoded: BLK_1=0, BLK_2=1, BLK_4=2, BLK_8=3)
//   0x7c6: sparsity ratio (encoded: SP_050=1 for 2:4, SP_025=2 for 1:4)

static inline uint32_t bit_if_valid(uint32_t vr) {
  return (vr < 32) ? (1u << vr) : 0u;
}

static inline void vtl_cfg(uint32_t vr0, uint32_t vr1, uint32_t vr2,
                           uint32_t vr3) {
  uint32_t bitmap = bit_if_valid(vr0) | bit_if_valid(vr1) | bit_if_valid(vr2) |
                    bit_if_valid(vr3);
  asm volatile("csrrw x0, 0x7c3, %0" ::"r"(bitmap) : "memory");
}

static inline void sparse_fmt_cfg(uint32_t idx_width, uint32_t m_sparse,
                                  uint32_t n_sparse) {
  uint32_t idx_enc = (idx_width == 1)   ? 0u
                     : (idx_width == 2) ? 1u
                     : (idx_width == 4) ? 2u
                                        : 3u;
  uint32_t blk_enc = (m_sparse == 1)   ? 0u
                     : (m_sparse == 2) ? 1u
                     : (m_sparse == 4) ? 2u
                                       : 3u;
  uint32_t ratio_enc = ((m_sparse / n_sparse) == 2u) ? 1u : 2u;

  asm volatile("csrrw x0, 0x7c4, %0" ::"r"(idx_enc) : "memory");
  asm volatile("csrrw x0, 0x7c5, %0" ::"r"(blk_enc) : "memory");
  asm volatile("csrrw x0, 0x7c6, %0" ::"r"(ratio_enc) : "memory");
}

void spmv_ventaglio(float *res, const float *a, const float *w,
                    const uint32_t *nm_index, uint32_t N, uint32_t P_W,
                    uint32_t NM_INDEX_ROW_WORDS, uint32_t idx_width,
                    uint32_t m_sparse, uint32_t n_sparse) {

  // Map only the accumulator (v16) into the Ventaglio bank.
  vtl_cfg(16, 32, 32, 32);
  sparse_fmt_cfg(idx_width, m_sparse, n_sparse);

  uint32_t avl = P_W;
  const float *_w = w;
  const uint32_t *_nm_index = nm_index;
  float *_res = res;

  do {
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

    // Inner-loop running pointers
    const float *_a = a;
    const float *__w = _w;
    const uint32_t *__nm_index = _nm_index;

    // Clear the Ventaglio bank so vfxmul lands on a clean slate.
    asm volatile("vventclr");

    // Prologue: n = 0  →  vfxmul writes v16[idx[i]] = a[0] * w[i]
    asm volatile("flw         ft0,  (%0)" ::"r"(_a));
    asm volatile("vlx32.v     v20,  (%0)" ::"r"(__nm_index));
    asm volatile("vle32.v     v8,   (%0)" ::"r"(__w));
    asm volatile("vfxmul.vrf  v16, ft0, v8, v20");
    _a += 1;
    __w += P_W;
    __nm_index += NM_INDEX_ROW_WORDS;

    // Body: n = 1..N-1  →  vfxmacc accumulates a[n] * w[i] at idx[i]
    for (uint32_t n = 1; n < N; n++) {
      asm volatile("flw         ft0,  (%0)" ::"r"(_a));
      asm volatile("vlx32.v     v20,  (%0)" ::"r"(__nm_index));
      asm volatile("vle32.v     v8,   (%0)" ::"r"(__w));
      asm volatile("vfxmacc.vrf v16, ft0, v8, v20");
      _a += 1;
      __w += P_W;
      __nm_index += NM_INDEX_ROW_WORDS;
    }

    // Store this P chunk; v16 will be overwritten by the next iter's vfxmul.
    uint32_t savl = vl * (m_sparse / n_sparse);
    asm volatile("vse32.v v16, (%0)" ::"r"(_res) : "memory");
    _res += savl;

    // Bump outer-loop pointers
    avl -= vl;
    _w += vl;
    _nm_index += vl / (sizeof(uint32_t) * 8u / idx_width);

    // Scalar load barrier: force vse32.v v16's bank gather + TCDM drain to
    // retire before the next iter's vventclr starts clearing the bank.
    // Without this, vventclr races with the in-flight gather and the gather
    // reads back already-cleared (zero) cells (manifests as zeros in the
    // tail of the previous chunk).
    if (avl > 0) {
      volatile uint32_t _vse_drain = *((volatile uint32_t *)(_res - 1));
      (void)_vse_drain;
    }
  } while (avl > 0);
}

// ===========================================================================
// Baseline (RVV gather-modify-scatter) implementation — spmv_baseline
// ===========================================================================

// Unpack packed N:M indices for ONE sparse row into byte offsets that
// vluxei32/vsuxei32 can consume directly.
static inline void unpack_nm_to_byte_offsets(const uint32_t *packed,
                                             uint32_t *offsets, uint32_t pw,
                                             uint32_t idx_width,
                                             uint32_t m_sparse,
                                             uint32_t n_sparse) {
  const uint32_t idx_mask = (1u << idx_width) - 1u;
  uint32_t bitpos = 0;

  for (uint32_t j = 0; j < pw; j++) {
    uint32_t widx = bitpos >> 5;
    uint32_t boff = bitpos & 0x1fu;
    uint32_t idx = (packed[widx] >> boff) & idx_mask;

    uint32_t block = j / n_sparse;
    uint32_t dense_pos = block * m_sparse + idx;
    offsets[j] = dense_pos * (uint32_t)sizeof(float);

    bitpos += idx_width;
  }
}

void spmv_baseline(float *res, const float *a, const float *w,
                   const uint32_t *nm_index, uint32_t *byte_offsets, uint32_t N,
                   uint32_t P_W, uint32_t NM_INDEX_ROW_WORDS,
                   uint32_t idx_width, uint32_t m_sparse, uint32_t n_sparse) {
  const float *_w = w;
  const uint32_t *_nm = nm_index;

  for (uint32_t n = 0; n < N; n++) {
    // Step 1: software N:M index translation for this sparse row.
    unpack_nm_to_byte_offsets(_nm, byte_offsets, P_W, idx_width, m_sparse,
                              n_sparse);

    // Step 2: load scalar activation a[n].
    float act;
    asm volatile("flw %0, (%1)" : "=f"(act) : "r"(a + n));

    // Step 3: tile across the compact dimension P_W.
    const float *_w_tile = _w;
    const uint32_t *_bo_tile = byte_offsets;
    uint32_t avl = P_W;

    do {
      uint32_t vl;
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

      asm volatile("vle32.v    v4,  (%0)" ::"r"(_bo_tile) : "memory");
      asm volatile("vle32.v    v8,  (%0)" ::"r"(_w_tile) : "memory");

      if (n == 0) {
        // First sparse row: v12 has no initialized contents and Spatz has no
        // `vmv` to clear it, so we use vfmul (write-only) to seed v12 with
        // a[0] * w[0,:]. The vluxei + vfmacc path is skipped to avoid
        // accumulating into uninitialized v12.
        asm volatile("vfmul.vf v12, v8, %0" ::"f"(act));
      } else {
        // Subsequent rows: gather running partial sums, accumulate, scatter.
        asm volatile("vluxei32.v v12, (%0), v4" ::"r"(res) : "memory");
        asm volatile("vfmacc.vf  v12, %0, v8" ::"f"(act));
      }
      asm volatile("vsuxei32.v v12, (%0), v4" ::"r"(res) : "memory");

      _w_tile += vl;
      _bo_tile += vl;
      avl -= vl;
    } while (avl > 0);

    _w += P_W;
    _nm += NM_INDEX_ROW_WORDS;
  }
}
