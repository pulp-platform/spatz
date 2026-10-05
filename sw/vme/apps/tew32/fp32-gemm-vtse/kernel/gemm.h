
#include <stdint.h>

static inline uint32_t gemm_cycle(void) {
    uint32_t value;
    asm volatile("csrr %0, mcycle" : "=r"(value));
    return value;
}

// General tightly packed kernel: At[K][M], B[K][N], C[M][N].
void gemm_fp32(void *addrA, void *addrB, void *addrC,
               int K, int N, int M, int alt_fmt);

// Compatibility entry for the existing packed-panel DMA driver.
void gemm_block_fp32(void *addrA, void *addrB, void *addrC,
                     int K, int N, int M, int alt_fmt);
