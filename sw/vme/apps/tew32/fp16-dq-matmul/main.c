#include <printf.h>
#include <snrt.h>
#include "ope_runtime.h"
#include DATAHEADER
#include "kernel/dq_matmul_mt8x2.h"

int main(void) {
  if (snrt_cluster_core_idx() != 0) return 0;
  enum {
    A_BYTES = sizeof(dq_mt4_a), CB_BYTES = sizeof(dq_mt4_cb),
    IDX_BYTES = sizeof(dq_mt4_idx), C_BYTES = DQ_M * DQ_N * 4,
    W_BYTES = DQ_RED * DQ_N * 2,
    TOTAL = A_BYTES + CB_BYTES + IDX_BYTES + C_BYTES + W_BYTES
  };
  uint8_t *mem = snrt_l1alloc(TOTAL);
  uint32_t *a = (uint32_t *)mem; mem += A_BYTES;
  uint32_t *cb = (uint32_t *)mem; mem += CB_BYTES;
  uint32_t *idx = (uint32_t *)mem; mem += IDX_BYTES;
  float *out = (float *)mem; mem += C_BYTES;
  uint32_t *workspace = (uint32_t *)mem;
  snrt_dma_start_1d(a, dq_mt4_a, A_BYTES);
  snrt_dma_start_1d(cb, dq_mt4_cb, CB_BYTES);
  snrt_dma_start_1d(idx, dq_mt4_idx, IDX_BYTES);
  snrt_dma_wait_all();
  for (uint32_t i = 0; i < DQ_M * DQ_N; ++i) out[i] = 0.0f;

  const uint32_t start = get_cycle();
  dq_matmul_MT8x2(a, idx, cb, cb + DQ_CBN * DQ_CB_W, out,
                  DQ_M, DQ_RED, workspace);
  const uint32_t elapsed = get_cycle() - start;

  uint32_t errors = 0;
  float max_abs = 0.0f;
  for (uint32_t i = 0; i < DQ_M * DQ_N; ++i) {
    union { uint32_t u; float f; } ref = {.u = dq_mt4_golden[i]};
    float got = out[i];
    float d = got - ref.f;
    if (d < 0) d = -d;
    float q = ref.f;
    if (q < 0) q = -q;
    if (d > max_abs) max_abs = d;
    if (!(d <= 0.001f + 0.001f * q)) {
      if (errors < 8) printf("mismatch %u got=%x ref=%x\n", i,
                             *(uint32_t *)&got, ref.u);
      ++errors;
    }
  }
  union { float f; uint32_t u; } max_bits = {.f = max_abs};
  printf("DQ MT8x2 M128 N64 K128 cycles=%u max=%08x errors=%u %s\n",
         elapsed, max_bits.u, errors, errors ? "FAIL" : "PASS");
  return errors != 0;
}
