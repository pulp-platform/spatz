// SPDX-License-Identifier: Apache-2.0

#include <printf.h>
#include "runtime.h"
#include <snrt.h>

#include "gemm.h"
#include "data/data_64_64_64.h"
#include "kernel/gemm.c"

static inline uint32_t float_bits(float value) {
  union { float f; uint32_t u; } bits = {.f = value};
  return bits.u;
}

int main(void) {
  if (snrt_cluster_core_idx() != 0)
    return 0;

  enum {
    A_BYTES = sizeof(Atpack_dram),
    B_BYTES = sizeof(Bpack_dram),
    C_BYTES = sizeof(Caddend_dram),
    BUFFER_BYTES = A_BYTES + B_BYTES + C_BYTES,
  };
  _Static_assert(BUFFER_BYTES <= SNRT_TCDM_SIZE,
                 "M64_N64_K64 buffers exceed TCDM");
  uint8_t *buffer = (uint8_t *)snrt_l1alloc(BUFFER_BYTES);
  if (buffer == 0)
    return 2;
  __fp16 *Atpack = (__fp16 *)buffer;
  __fp16 *Bpack = (__fp16 *)(buffer + A_BYTES);
  float *C = (float *)(buffer + A_BYTES + B_BYTES);

  snrt_dma_start_1d(Atpack, Atpack_dram, A_BYTES);
  snrt_dma_start_1d(Bpack, Bpack_dram, B_BYTES);
  snrt_dma_start_1d(C, Caddend_dram, C_BYTES);
  snrt_dma_wait_all();

  const uint32_t start = get_cycle();
  gemm_relu_fused_m64_k64(Atpack, Bpack, C);
  const uint32_t cycles = get_cycle() - start;

  uint32_t errors = 0;
  float max_abs = 0.0f;
  for (uint32_t i = 0; i < 64 * 64; ++i) {
    const float expected = Cref_dram[i];
    const float delta = C[i] > expected ? C[i] - expected : expected - C[i];
    const float tolerance = 0.001f + 0.001f *
        (expected < 0.0f ? -expected : expected);
    if (delta > max_abs)
      max_abs = delta;
    if (!(delta <= tolerance)) {
      if (errors < 8)
        printf("mismatch row %u col %u: got 0x%08x ref 0x%08x\n",
               i / 64, i % 64, float_bits(C[i]), float_bits(expected));
      ++errors;
    }
  }

  printf("\n=== VME GEMM+ReLU fused M64_N64_K64 ===\n");
  printf("Cycles: %u\n", cycles);
#if BENCHMARK
  gemm_relu_counters_t counters;
  gemm_relu_get_counters(&counters);
  printf("Breakdown: prologue=%u steady=%u epilogue=%u accounted=%u\n",
         counters.setup, counters.compute, counters.drain,
         counters.setup + counters.compute + counters.drain);
#endif
  printf("MaxAbsErr: 0x%08x\nErrors: %u\nStatus: %s\n",
         float_bits(max_abs), errors, errors ? "FAIL" : "PASS");
  return errors ? 1 : 0;
}
