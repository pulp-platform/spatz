// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include "runtime.h"
#include <printf.h>
#include <snrt.h>
#include DATAHEADER
#include "kernel/conv.h"

#ifndef APP_FUSED
#define APP_FUSED 1
#endif

static __attribute__((noinline)) void relu_all(float *z, unsigned count) {
  for (unsigned i = 0; i < count;) {
    unsigned vl;
    asm volatile(
        "vsetvli %[vl], %[left], e32, m8, ta, ma\n"
        "vle32.v v0, (%[ptr])\n"
        RELU_ASM("v0")
        "vse32.v v0, (%[ptr])"
        : [vl] "=&r"(vl)
        : [left] "r"(count - i), [ptr] "r"(z + i), RELU_INPUTS
        : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
    i += vl;
  }
  asm volatile("lw zero, 0(%0)" : : "r"(z) : "memory");
}

int main(void) {
  if (snrt_cluster_core_idx() != 0) return 0;
  _Static_assert(BUFFER_BYTES == 119040, "latest conv footprint changed");
  _Static_assert(BUFFER_BYTES <= SNRT_TCDM_SIZE, "conv exceeds TCDM");

  uint8_t *mem = snrt_l1alloc(BUFFER_BYTES);
  if (!mem) return 2;
  uint32_t *input = (uint32_t *)mem;
  uint32_t *weights = (uint32_t *)(mem + sizeof(cv_in_dram));
  float *z = (float *)((uint8_t *)weights + sizeof(cv_w_dram));
  snrt_dma_start_1d(input, cv_in_dram, sizeof(cv_in_dram));
  snrt_dma_start_1d(weights, cv_w_dram, sizeof(cv_w_dram));
  snrt_dma_start_1d(z, cv_zinit_dram, sizeof(cv_zinit_dram));
  snrt_dma_wait_all();

  unsigned start = get_cycle();
  conv_accumulate_taps(input, weights, z, H_SIZE, W_SIZE, CIN, COUT,
                       KH_SIZE, KW_SIZE, APP_FUSED);
#if !APP_FUSED
  relu_all(z, M_SIZE * COUT);
#endif
  unsigned cycles = get_cycle() - start;

  unsigned errors = 0;
  for (unsigned oy = 0; oy < HO_SIZE; ++oy)
    for (unsigned ox = 0; ox < WO_SIZE; ++ox)
      for (unsigned c = 0; c < COUT; ++c) {
        union { uint32_t u; float f; } ref = {
          .u = cv_golden[(oy * WO_SIZE + ox) * COUT + c]
        };
        float got = z[(oy * W_SIZE + ox) * COUT + c];
        float d = got - ref.f; if (d < 0) d = -d;
        float mag = ref.f; if (mag < 0) mag = -mag;
        if (!(d <= 1e-3f + 1e-3f * mag)) ++errors;
      }
  printf("fp16_conv_relu %s Cin%u H%u W%u Cout%u K3x3\n",
         APP_FUSED ? "fused" : "non-fused", CIN, H_SIZE, W_SIZE, COUT);
  printf("9 accumulating GEMMs + ReLU: %u cycles, TCDM=%u\n",
         cycles, BUFFER_BYTES);
  printf("Errors=%u Status=%s\n", errors, errors ? "FAIL" : "PASS");
  return errors ? 1 : 0;
}
