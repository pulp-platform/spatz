// SPDX-License-Identifier: Apache-2.0

#include <printf.h>
#include <snrt.h>

#include "ope_runtime.h"
#include DATAHEADER
#include "kernel/flashattn.h"

int main(void) {
  if (snrt_cluster_core_idx() != 0) return 0;

  uint32_t *q = (uint32_t *)snrt_l1alloc(sizeof(flash_q));
  uint32_t *k = (uint32_t *)snrt_l1alloc(sizeof(flash_k));
  uint32_t *v = (uint32_t *)snrt_l1alloc(sizeof(flash_v));
  uint32_t *pair_scratch =
      (uint32_t *)snrt_l1alloc(FLASH_TQ * sizeof(uint32_t));
  float *output = (float *)snrt_l1alloc(FLASH_TQ * FLASH_DV * sizeof(float));
  if (!q || !k || !v || !pair_scratch || !output) return 1;

  DMA(q, flash_q);
  DMA(k, flash_k);
  DMA(v, flash_v);
  snrt_dma_wait_all();

  const uint32_t begin = get_cycle();
  flashattn_tq16(q, k, v, pair_scratch, output);
  const uint32_t cycles = get_cycle() - begin;
  const int errors = ope_check_fp32(output, (const float *)flash_golden,
                                    FLASH_TQ * FLASH_DV, 2e-3f, 2e-3f);
  PRINTF("\n=== VME direct-VRF FlashAttention Tq16 T64 d64 dv16 ===\n");
  PRINTF("cycles: %u\n", cycles);
  PRINTF(errors ? "FAIL: %d mismatches\n" : "PASS\n", errors);
  return errors ? 1 : 0;
}
