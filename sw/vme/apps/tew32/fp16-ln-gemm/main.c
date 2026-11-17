// SPDX-License-Identifier: Apache-2.0
#include <printf.h>
#include "runtime.h"
#include <snrt.h>

#ifndef APP_FUSED
#define APP_FUSED 1
#endif

#include "gemm.h"
#include DATAHEADER
#include "kernel/gemm.c"

enum { M_DIM = COUT_SIZE, N_DIM = T_SIZE, K_DIM = C_SIZE };

static inline uint32_t float_bits(float value) {
  union { float f; uint32_t u; } bits = {.f = value};
  return bits.u;
}

int main(void) {
  if (snrt_cluster_core_idx() != 0)
    return 0;
  if (M_DIM != 176 || N_DIM != 64 || K_DIM != 96) {
    printf("Unsupported P1 shape: expected M176 N64 K96\n");
    return 2;
  }

  enum {
    X_BYTES = sizeof(d_x),
    PACKED_BYTES = (K_DIM / 2) * N_DIM * sizeof(uint32_t),
    W_BYTES = sizeof(d_w),
    C_BYTES = sizeof(d_zinit),
    BUFFER_BYTES = X_BYTES + PACKED_BYTES + W_BYTES + C_BYTES
  };
  _Static_assert(BUFFER_BYTES == 115712, "P1 footprint must match baseline");
  _Static_assert(BUFFER_BYTES <= 120 * 1024, "P1 exceeds TCDM budget");

  uint8_t *const buffer = (uint8_t *)snrt_l1alloc(BUFFER_BYTES);
  if (!buffer)
    return 2;
  float *const x = (float *)buffer;
  uint32_t *const packed = (uint32_t *)(buffer + X_BYTES);
  uint32_t *const w = (uint32_t *)(buffer + X_BYTES + PACKED_BYTES);
  float *const z = (float *)(buffer + X_BYTES + PACKED_BYTES + W_BYTES);

  snrt_dma_start_1d(x, d_x, X_BYTES);
  snrt_dma_start_1d(z, d_zinit, C_BYTES);
  snrt_dma_wait_all();

  // Reorder baseline [K/2][M] weights into VME [M/16][K/2][16]. Keep
  // baseline's low-even/high-odd pair order because LN packing uses the same.
  for (uint32_t block = 0; block < M_DIM / TE; ++block)
    for (uint32_t pair = 0; pair < K_DIM / 2; ++pair)
      for (uint32_t lane = 0; lane < TE; ++lane)
        w[(block * (K_DIM / 2) + pair) * TE + lane] =
            d_w[pair * M_DIM + block * TE + lane];

  // Footprint-neutral temporary offset table; layernorm_pack loads it into
  // v30 before normalized data overwrites this region.
  uint16_t *const offsets = (uint16_t *)packed;
  for (uint32_t q = 0; q < N_DIM; ++q)
    offsets[q] = (q / TE) * (K_DIM / 2) * TE * sizeof(uint32_t) +
                 (q % TE) * sizeof(uint32_t);
  // Relative offsets for the fused N32 ping-pong panels.  Keep the table at
  // the end of the packed workspace: panel 0 cannot overwrite it, and panel
  // 1 loads it into v30 before its final normalized rows reach this region.
  uint16_t *const offsets32 =
      (uint16_t *)(packed + (K_DIM / 2) * N_DIM) - 32;
  for (uint32_t q = 0; q < 32; ++q)
    offsets32[q] = (q / TE) * (K_DIM / 2) * TE * sizeof(uint32_t) +
                   (q % TE) * sizeof(uint32_t);

  const uint32_t start = get_cycle();
#if defined(APP_MT4X1_CACHE)
  ln_gemm_mt4x1_cache(x, w, z, packed, M_DIM, N_DIM, K_DIM);
  const uint32_t ln_cycles = 0;
  const uint32_t gemm_cycles = 0;
#elif APP_FUSED
  ln_gemm_fused(x, w, z, packed, M_DIM, N_DIM, K_DIM);
  const uint32_t ln_cycles = 0;
  const uint32_t gemm_cycles = 0;
#else
  layernorm_pack_non_fused(x, packed, N_DIM, K_DIM);
  const uint32_t after_ln = get_cycle();
  gemm_after_ln_non_fused(x, w, z, packed, M_DIM, N_DIM, K_DIM);
  const uint32_t after_gemm = get_cycle();
  const uint32_t ln_cycles = after_ln - start;
  const uint32_t gemm_cycles = after_gemm - after_ln;
#endif
  const uint32_t cycles = get_cycle() - start;

  uint32_t errors = 0;
  float max_abs = 0.0f;
  for (uint32_t i = 0; i < M_DIM * N_DIM; ++i) {
    union { uint32_t u; float f; } ref = {.u = d_golden[i]};
    float delta = z[i] > ref.f ? z[i] - ref.f : ref.f - z[i];
    float mag = ref.f < 0.0f ? -ref.f : ref.f;
    if (delta > max_abs) max_abs = delta;
    if (!(delta <= 0.001f + 0.001f * mag)) {
      if (errors < 8)
        printf("mismatch row %u col %u: got 0x%08x ref 0x%08x\n",
               i / N_DIM, i % N_DIM, float_bits(z[i]), ref.u);
      ++errors;
    }
  }
  printf("\n=== VME P1 LN+GEMM %s M%u_N%u_K%u "
         "(baseline C96 T64 O176) ===\n",
         APP_FUSED ? "fused" : "non-fused", M_DIM, N_DIM, K_DIM);
  printf("Cycles: %u\nMaxAbsErr: 0x%08x\nErrors: %u\nStatus: %s\n",
         cycles, float_bits(max_abs), errors, errors ? "FAIL" : "PASS");
#if defined(APP_MT4X1_CACHE)
  printf("Cache breakdown: stats %u, M64 producer %u, M112 consumer %u\n",
         ln_cache_stats_cycles, ln_cache_producer_cycles,
         ln_cache_consumer_cycles);
  printf("Consumer detail: M96 body %u, M16 tail %u\n",
         ln_cache_consumer_main_cycles, ln_cache_consumer_tail_cycles);
  printf("Producer detail: tile load %u, normalize+MAC %u, tile store %u\n",
         ln_cache_tile_load_cycles, ln_cache_core_cycles,
         ln_cache_tile_store_cycles);
#endif
#if !APP_FUSED
  printf("Breakdown: LN+pack %u, GEMM %u, timer overhead %u\n",
         ln_cycles, gemm_cycles, cycles - ln_cycles - gemm_cycles);
#endif
  return errors ? 1 : 0;
}
