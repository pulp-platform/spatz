
#include <stdint.h>

static inline uint32_t gemm_cycle(void) {
    uint32_t value;
    asm volatile("csrr %0, mcycle" : "=r"(value));
    return value;
}

// Tuned two-core bias GEMM for one packed M64xN64xK64 panel. C must contain
// the packed initial accumulator values before entry.
void gemm_fp32(float *C, const float *Apack, const float *Bpack,
               uint32_t ti_lo, uint32_t ti_hi);

// Runtime edge kernel. Each call handles at most one M32xN64xK64 per-core
// panel; A/B/C retain the physical M64xN64xK64 packed-panel strides.
void gemm_block_fp32(void *addrA, void *addrB, void *addrC,
                     int K, int N, int M, int alt_fmt);
