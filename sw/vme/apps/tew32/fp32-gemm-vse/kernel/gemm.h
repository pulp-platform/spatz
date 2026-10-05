
#include <stdint.h>

static inline uint32_t gemm_cycle(void) {
    uint32_t value;
    asm volatile("csrr %0, mcycle" : "=r"(value));
    return value;
}

// Runtime kernel. Each call handles at most one physical M32xN64xK64 panel;
// A/B/C retain the physical 64x64x64 packed-panel strides.
void gemm_fp32(void *addrA, void *addrB, void *addrC,
               int K, int N, int M, int alt_fmt);
