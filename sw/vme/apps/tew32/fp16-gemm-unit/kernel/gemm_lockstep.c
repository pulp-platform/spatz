// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include "gemm.h"

// Four 16x16 tiles compute a 16x64 output block, then drain together.
// Uses the same Caddend seeding and aligned operand group loads as staggered.
__attribute__((noinline)) void gemm_fp16_fp32_unit_lockstep(
    const __fp16 *Atpack, const __fp16 *Bpack, float *C) {
  const uint32_t *a_words = (const uint32_t *)Atpack;
  const uint32_t *b_words = (const uint32_t *)Bpack;
  const uintptr_t mac_mtype = (TE << 10) | (2u << 5) | 2u;
  const uintptr_t mac_vtype = 0xc8u;
  const uintptr_t tile_vl = TE;

  for (unsigned mb = 0; mb < 64; mb += TE) {
    asm volatile("msetmtype %0, %1\nmsettn zero, %2\nmsettm zero, %2"
                 :: "r"(mac_mtype), "r"(mac_vtype), "r"(tile_vl)
                 : "memory");
    for (unsigned tile = 0; tile < 4; ++tile) {
      float *dst = C + mb * 64 + tile * TE;
      for (unsigned row = 0; row < TE; ++row) {
        asm volatile("vtle32 %0, (%1)"
                     :: "r"((uintptr_t)tile * 0x20000000u + row),
                        "r"(dst + row * 64) : "memory");
      }
    }

    const uint32_t *a = a_words + (mb / TE) * (K_CHUNK / 2) * TE;
    for (unsigned kg = 0; kg < K_CHUNK / 8; ++kg) {
      const unsigned offset = kg * 4 * TE;
      asm volatile("vsetvli zero, %0, e32, m4, ta, ma"
                   :: "r"((uintptr_t)(4 * TE)) : "memory");
      asm volatile("vle32.v v0, (%0)" :: "r"(a + offset)
                   : "v0", "v1", "v2", "v3", "memory");
      asm volatile("vle32.v v16, (%0)" :: "r"(b_words + offset)
                   : "v16", "v17", "v18", "v19", "memory");
      asm volatile("vle32.v v20, (%0)"
                   :: "r"(b_words + (K_CHUNK / 2) * TE + offset)
                   : "v20", "v21", "v22", "v23", "memory");
      asm volatile("vle32.v v24, (%0)"
                   :: "r"(b_words + 2 * (K_CHUNK / 2) * TE + offset)
                   : "v24", "v25", "v26", "v27", "memory");
      asm volatile("vle32.v v28, (%0)"
                   :: "r"(b_words + 3 * (K_CHUNK / 2) * TE + offset)
                   : "v28", "v29", "v30", "v31", "memory");
      asm volatile("msetmtype %0, %1\nmsettn zero, %2\nmsettm zero, %2"
                   :: "r"(mac_mtype), "r"(mac_vtype), "r"(tile_vl)
                   : "memory");
      asm volatile(
          "vtfmm.tvv mt0, v0, v16\nvtfmm.tvv mt4, v0, v20\n"
          "vtfmm.tvv mt8, v0, v24\nvtfmm.tvv mt12, v0, v28\n"
          "vtfmm.tvv mt0, v1, v17\nvtfmm.tvv mt4, v1, v21\n"
          "vtfmm.tvv mt8, v1, v25\nvtfmm.tvv mt12, v1, v29\n"
          "vtfmm.tvv mt0, v2, v18\nvtfmm.tvv mt4, v2, v22\n"
          "vtfmm.tvv mt8, v2, v26\nvtfmm.tvv mt12, v2, v30\n"
          "vtfmm.tvv mt0, v3, v19\nvtfmm.tvv mt4, v3, v23\n"
          "vtfmm.tvv mt8, v3, v27\nvtfmm.tvv mt12, v3, v31"
          ::: "memory");
    }

    for (unsigned tile = 0; tile < 4; ++tile) {
      float *dst = C + mb * 64 + tile * TE;
      for (unsigned row = 0; row < TE; ++row) {
        asm volatile("vtse32 %0, (%1)"
                     :: "r"((uintptr_t)tile * 0x20000000u + row),
                        "r"(dst + row * 64) : "memory");
      }
    }
  }
  asm volatile("lw zero, 0(%0)" :: "r"(C) : "memory");
}
