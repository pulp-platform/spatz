// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include "gemm.h"

// Pair-major operands: [M/TE][K/2][TE] and [N/TE][K/2][TE].
// Two tiles compute a 16x32 block while the other two tiles drain the
// preceding block. Caddend is loaded into the active tiles before the MACs.
__attribute__((noinline)) void gemm_fp16_fp32_unit_staggered(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C) {
  const uint32_t *a_words = (const uint32_t *)Atpack;
  const uint32_t *b_words = (const uint32_t *)Bpack;
  const uintptr_t mac_mtype = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t mac_vtype = 0xc8u;
  const uintptr_t tile_vl = TE;
  const uintptr_t drain_step = K_CHUNK / 128;
  uintptr_t prev_sel0 = 0, prev_sel1 = 0;
  float *prev_c0 = 0, *prev_c1 = 0;
  unsigned have_prev = 0;
  unsigned group = 0;

  for (unsigned mb = 0; mb < 64; mb += TE) {
    for (unsigned nb = 0; nb < 64; nb += 2 * TE, ++group) {
      const unsigned active_hi = group & 1u;
      const uintptr_t active_sel0 = active_hi ? 0x40000000u : 0u;
      const uintptr_t active_sel1 = active_sel0 + 0x20000000u;
      float *c0 = C + mb * 64 + nb;
      float *c1 = c0 + TE;
      const uint32_t *a = a_words + (mb / TE) * (K_CHUNK / 2) * TE;
      const uint32_t *b0 = b_words + (nb / TE) * (K_CHUNK / 2) * TE;
      const uint32_t *b1 = b0 + (K_CHUNK / 2) * TE;

      asm volatile("msetmtype %0, %1\nmsettn zero, %2\nmsettm zero, %2"
                   :: "r"(mac_mtype), "r"(mac_vtype), "r"(tile_vl)
                   : "memory");
      for (unsigned row = 0; row < TE; ++row) {
        asm volatile("vtle32 %0, (%1)"
                     :: "r"(active_sel0 + row), "r"(c0 + row * 64)
                     : "memory");
        asm volatile("vtle32 %0, (%1)"
                     :: "r"(active_sel1 + row), "r"(c1 + row * 64)
                     : "memory");
      }

      for (unsigned kg = 0; kg < K_CHUNK / 8; ++kg) {
        const unsigned offset = kg * 4 * TE;
        asm volatile("vsetvli zero, %0, e32, m4, ta, ma"
                     :: "r"((uintptr_t)(4 * TE)) : "memory");
        asm volatile("vle32.v v0, (%0)" :: "r"(a + offset)
                     : "v0", "v1", "v2", "v3", "memory");
        asm volatile("vle32.v v16, (%0)" :: "r"(b0 + offset)
                     : "v16", "v17", "v18", "v19", "memory");
        asm volatile("vle32.v v24, (%0)" :: "r"(b1 + offset)
                     : "v24", "v25", "v26", "v27", "memory");
        asm volatile("msetmtype %0, %1\nmsettn zero, %2\nmsettm zero, %2"
                     :: "r"(mac_mtype), "r"(mac_vtype), "r"(tile_vl)
                     : "memory");

        if (active_hi) {
          asm volatile("vtfmm.tvv mt8, v0, v16\n"
                       "vtfmm.tvv mt12, v0, v24\n"
                       "vtfmm.tvv mt8, v1, v17\n"
                       "vtfmm.tvv mt12, v1, v25" ::: "memory");
        } else {
          asm volatile("vtfmm.tvv mt0, v0, v16\n"
                       "vtfmm.tvv mt4, v0, v24\n"
                       "vtfmm.tvv mt0, v1, v17\n"
                       "vtfmm.tvv mt4, v1, v25" ::: "memory");
        }
        if (have_prev && kg % drain_step == 0) {
          asm volatile("vtse32 %0, (%1)"
                       :: "r"(prev_sel0++), "r"(prev_c0) : "memory");
          prev_c0 += 64;
        }
        if (active_hi) {
          asm volatile("vtfmm.tvv mt8, v2, v18\n"
                       "vtfmm.tvv mt12, v2, v26\n"
                       "vtfmm.tvv mt8, v3, v19\n"
                       "vtfmm.tvv mt12, v3, v27" ::: "memory");
        } else {
          asm volatile("vtfmm.tvv mt0, v2, v18\n"
                       "vtfmm.tvv mt4, v2, v26\n"
                       "vtfmm.tvv mt0, v3, v19\n"
                       "vtfmm.tvv mt4, v3, v27" ::: "memory");
        }
        if (have_prev && kg % drain_step == 0) {
          asm volatile("vtse32 %0, (%1)"
                       :: "r"(prev_sel1++), "r"(prev_c1) : "memory");
          prev_c1 += 64;
        }
      }

      prev_sel0 = active_sel0;
      prev_sel1 = active_sel1;
      prev_c0 = c0;
      prev_c1 = c1;
      have_prev = 1;
    }
  }

  for (unsigned row = 0; row < TE; ++row) {
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(prev_sel0++), "r"(prev_c0) : "memory");
    asm volatile("vtse32 %0, (%1)"
                 :: "r"(prev_sel1++), "r"(prev_c1) : "memory");
    prev_c0 += 64;
    prev_c1 += 64;
  }
  asm volatile("lw zero, 0(%0)" :: "r"(C) : "memory");
}
