// SPDX-License-Identifier: Apache-2.0

#include <printf.h>
#include <snrt.h>

#include "ope_runtime.h"
#include DATAHEADER
#include "kernel/flashattn.h"

static void print_stage_counters(void) {
#if BENCHMARK
  flashattn_tq64_counters_t c;
  flashattn_tq64_get_counters(&c);
  PRINTF("stage qk=%u softmax=%u pv=%u normalize=%u sum=%u\n", c.qk,
         c.softmax, c.pv, c.normalize,
         c.qk + c.softmax + c.pv + c.normalize);
  PRINTF("softmax premax=%u exp_pack=%u rescale=%u\n", c.premax,
         c.exp_pack, c.rescale);
  for (unsigned block = 0; block < NT; ++block)
    PRINTF("block %u: qk=%u softmax=%u (premax=%u exp_pack=%u "
           "rescale=%u) pv=%u\n",
           block, c.qk_block[block], c.softmax_block[block],
           c.premax_block[block], c.exp_pack_block[block],
           c.rescale_block[block], c.pv_block[block]);
  PRINTF("PV tiles: tile0=%u tile1=%u tile2=%u tile3=%u\n", c.pv_tile[0],
         c.pv_tile[1], c.pv_tile[2], c.pv_tile[3]);
  for (unsigned tile = 0; tile < 4; ++tile)
    PRINTF("PV tile %u blocks: b0=%u b1=%u b2=%u b3=%u\n", tile,
           c.pv_block_tile[tile][0], c.pv_block_tile[tile][1],
           c.pv_block_tile[tile][2], c.pv_block_tile[tile][3]);
#endif
}

int main(void) {
  if (snrt_cluster_core_idx() != 0) return 0;

  flashattn_tq64_buffers_t b = {
      .q = snrt_l1alloc((D_SIZE / 2) * TQ * sizeof(uint32_t)),
      .k = snrt_l1alloc(NT * (D_SIZE / 2) * BC * sizeof(uint32_t)),
      .v = snrt_l1alloc(NT * (BC / 2) * DV_SIZE * sizeof(uint32_t)),
      .scores = snrt_l1alloc(NT * BC * TQ * sizeof(float)),
      .probabilities =
          snrt_l1alloc(NT * (BC / 2) * TQ * sizeof(uint32_t)),
      .output = snrt_l1alloc(DV_SIZE * TQ * sizeof(float)),
      .row_max = snrt_l1alloc(TQ * sizeof(float)),
      .row_sum = snrt_l1alloc(TQ * sizeof(float)),
  };
  if (!b.q || !b.k || !b.v || !b.scores || !b.probabilities || !b.output ||
      !b.row_max || !b.row_sum) {
    PRINTF("L1 allocation failed\n");
    return 1;
  }

  snrt_dma_start_1d(b.q, fa_qw_vme, sizeof(fa_qw_vme));
  snrt_dma_start_1d(b.k, fa_kw_vme, sizeof(fa_kw_vme));
  snrt_dma_start_1d(b.v, fa_vw_dram, sizeof(fa_vw_dram));
  snrt_dma_start_1d(b.row_max, fa_m_dram, sizeof(fa_m_dram));
  snrt_dma_wait_all();
  ope_vzero(b.row_sum, TQ);
  ope_vzero(b.output, DV_SIZE * TQ);

  flashattn_tq64_configure(&b);
  const unsigned begin = get_cycle();
  start_kernel();
  flashattn_tq64_run();
  stop_kernel();
  const unsigned cycles = get_cycle() - begin;

  PRINTF("\n----- fp16 flashattn fused Tq%d T%d d%d -----\n", TQ, T_SIZE,
         D_SIZE);
  PRINTF("The execution took %u cycles.\n", cycles);
  print_stage_counters();
  const int errors = ope_check_fp32(b.output, (const float *)fa_golden,
                                    DV_SIZE * TQ, 1e-3f, 1e-3f);
  PRINTF(errors ? "FAIL: %d mismatches\n" : "PASS\n", errors);
  return errors ? 1 : 0;
}
