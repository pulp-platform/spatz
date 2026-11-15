#include <printf.h>
#include <snrt.h>
#include <stdint.h>

#include "ope_runtime.h"
#include DATAHEADER
#include "kernel/matmul.h"

enum { M96 = 96, N128 = 128, K128 = 128, CBW = 16, CBN = 64, NCB = 2 };

static uint32_t check(const float *actual, const uint32_t *golden,
                      uint32_t count, float *max_abs) {
  uint32_t errors = 0;
  *max_abs = 0.0f;
  for (uint32_t i = 0; i < count; ++i) {
    union { uint32_t u; float f; } ref = {.u = golden[i]};
    float delta = actual[i] - ref.f;
    if (delta < 0.0f) delta = -delta;
    const float magnitude = ref.f < 0.0f ? -ref.f : ref.f;
    if (delta > *max_abs) *max_abs = delta;
    if (!(delta <= 1e-3f + 1e-3f * magnitude)) ++errors;
  }
  return errors;
}

int main(void) {
  if (snrt_cluster_core_idx() != 0) return 0;
  enum { A_BYTES = M96 * K128 * 2,
         C_BYTES = M96 * N128 * 4,
         CB_BYTES = NCB * CBN * CBW * 4,
         INDEX_BYTES = (K128 / 2) * (N128 / CBW / 2) * 4,
         WORKSPACE_BYTES = K128 * N128 * 2,
         BUFFER_BYTES = A_BYTES + C_BYTES + CB_BYTES + INDEX_BYTES +
                        WORKSPACE_BYTES };
  uint8_t *buffer = (uint8_t *)snrt_l1alloc(BUFFER_BYTES);
  uint32_t *apack = (uint32_t *)buffer;
  float *output = (float *)(buffer + A_BYTES);
  uint32_t *cb0 = (uint32_t *)((uint8_t *)output + C_BYTES);
  uint32_t *cb1 = cb0 + CBN * CBW;
  uint32_t *idx = (uint32_t *)((uint8_t *)cb0 + CB_BYTES);
  uint32_t *workspace = (uint32_t *)((uint8_t *)idx + INDEX_BYTES);
  snrt_dma_start_1d(apack, vq_apack_dram, A_BYTES);
  snrt_dma_start_1d(cb0, vq_cb_dram, CB_BYTES);
  snrt_dma_start_1d(idx, vq_idx_dram, INDEX_BYTES);
  snrt_dma_wait_all();
  for (uint32_t i = 0; i < M96 * N128; ++i) output[i] = 0.0f;

  const uint32_t start = get_cycle();
  vq_gemm_MT6x2(apack, workspace, idx, cb0, cb1, output,
                 M96, N128, K128);
  const uint32_t cycles = get_cycle() - start;
  float max_abs;
  const uint32_t errors = check(output, vq_golden_dram, M96 * N128, &max_abs);
  union { float f; uint32_t u; } bits = {.f = max_abs};
  printf("\n=== VQ-MatMul MT6x2 serial M96 N128 K128 ===\n");
  printf("cycles: %u\nMaxAbsErr: 0x%08x\nErrors: %u\nStatus: %s\n",
         cycles, bits.u, errors, errors ? "FAIL" : "PASS");
  return errors ? 1 : 0;
}
