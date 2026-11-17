// SPDX-License-Identifier: Apache-2.0
// Ct[COUT][SPATIAL] = Wt[COUT][144] * Xt[144][SPATIAL] + bias.
#include <stdint.h>
#include "conv.h"
#define CFG16() asm volatile("msetmtype %0, %1\nmsettn zero, %2\nmsettm zero, %2" :: "r"(16450u), "r"(0xc8u), "r"(16u) : "memory")
#define CFG32() asm volatile("vsetivli zero, 16, e32, m1, ta, ma\n"            \
                             "msetmtypei 1, 2, 0\n"                           \
                             "msettn zero, %0\nmsettm zero, %0"              \
                             :: "r"(16u) : "memory")
#define LOAD(reg, ptr) asm volatile("vle32.v " #reg ", (%0)" :: "r"(ptr) : #reg, "memory")
#define MAC(tile, a, b) asm volatile("vtfmm.tvv " #tile ", " #a ", " #b ::: "memory")
#if APP_FUSED
#define CONV_RELU(reg) asm volatile(RELU_ASM(#reg)                         \
    : : RELU_INPUTS : #reg, "memory")
#else
#define CONV_RELU(reg) do { } while (0)
#endif

// TILES is a compile-time constant at every expansion. The compiler removes
// inactive loads/MACs and the inner K loop contains no tile-count branch.
#define RUN_BLOCK(TILES) do {                                                   \
  CFG32();                                                                      \
  for (unsigned t = 0; t < (TILES); ++t) {                                     \
    uintptr_t seed = t * 0x20000000u;                                          \
    const float *row_ptr = ct + co*SPATIAL + s+t*16;                           \
    for (unsigned row = 0; row < 16; ++row) {                                  \
      asm volatile("vtle32 %0, (%1)" : : "r"(seed), "r"(row_ptr)           \
                   : "memory");                                               \
      ++seed;                                                                   \
      row_ptr += SPATIAL;                                                       \
    }                                                                           \
  }                                                                             \
  const uint32_t *a = wt + (co/16)*72*16;                                      \
  const uint32_t *b = xt + s;                                                   \
  CFG16();                                                                      \
  LOAD(v0,a);                                                                   \
  LOAD(v16,b+0*16);                                                             \
  if ((TILES) > 1) LOAD(v18,b+1*16);                                            \
  if ((TILES) > 2) LOAD(v20,b+2*16);                                            \
  if ((TILES) > 3) LOAD(v22,b+3*16);                                            \
  for (unsigned p = 0; p < 70; p += 2) {                                       \
    const uint32_t *b1 = b + (p+1)*SPATIAL;                                     \
    const uint32_t *b2 = b + (p+2)*SPATIAL;                                     \
    LOAD(v2,a+(p+1)*16);                                                        \
    MAC(mt0,v0,v16); LOAD(v24,b1+0*16);                                        \
    if ((TILES) > 1) { MAC(mt4,v0,v18); LOAD(v26,b1+1*16); }                   \
    if ((TILES) > 2) { MAC(mt8,v0,v20); LOAD(v28,b1+2*16); }                   \
    if ((TILES) > 3) { MAC(mt12,v0,v22); LOAD(v30,b1+3*16); }                  \
    LOAD(v0,a+(p+2)*16);                                                        \
    MAC(mt0,v2,v24); LOAD(v16,b2+0*16);                                        \
    if ((TILES) > 1) { MAC(mt4,v2,v26); LOAD(v18,b2+1*16); }                   \
    if ((TILES) > 2) { MAC(mt8,v2,v28); LOAD(v20,b2+2*16); }                   \
    if ((TILES) > 3) { MAC(mt12,v2,v30); LOAD(v22,b2+3*16); }                  \
  }                                                                             \
  {                                                                             \
    const uint32_t *b1 = b + 71*SPATIAL;                                        \
    LOAD(v2,a+71*16);                                                           \
    MAC(mt0,v0,v16); LOAD(v24,b1+0*16);                                        \
    if ((TILES) > 1) { MAC(mt4,v0,v18); LOAD(v26,b1+1*16); }                   \
    if ((TILES) > 2) { MAC(mt8,v0,v20); LOAD(v28,b1+2*16); }                   \
    if ((TILES) > 3) { MAC(mt12,v0,v22); LOAD(v30,b1+3*16); }                  \
  }                                                                             \
  MAC(mt0,v2,v24);                                                              \
  if ((TILES) > 1) MAC(mt4,v2,v26);                                            \
  if ((TILES) > 2) MAC(mt8,v2,v28);                                            \
  if ((TILES) > 3) MAC(mt12,v2,v30);                                           \
  CFG32();                                                                      \
  for (unsigned t = 0; t < (TILES); ++t) {                                    \
    uintptr_t sel = t * 0x20000000u;                                           \
    float *dst = ct + co*SPATIAL + s+t*16;                                     \
    for (unsigned row = 0; row < 16; row += 2) {                               \
      asm volatile("vtmv.v.t v8, %0\naddi %0, %0, 1\n"                       \
                   "vtmv.v.t v12, %0\naddi %0, %0, 1\n"                      \
                   : "+&r"(sel) : : "v8", "v12", "memory");                 \
      CONV_RELU(v8);                                                            \
      asm volatile("lw zero, 0(%0)\nvse32.v v8, (%0)\nadd %0, %0, %1"     \
                   : "+&r"(dst) : "r"(SPATIAL*sizeof(float)) : "memory");     \
      CONV_RELU(v12);                                                           \
      asm volatile("lw zero, 0(%0)\nvse32.v v12, (%0)\nadd %0, %0, %1"    \
                   : "+&r"(dst) : "r"(SPATIAL*sizeof(float)) : "memory");     \
    }                                                                           \
  }                                                                             \
} while (0)

static __attribute__((noinline)) void conv_gemm(const uint32_t *wt,
    const uint32_t *xt, float *ct) {
  for (unsigned co = 0; co < COUT; co += 16) {
    for (unsigned s = 0; s < SPATIAL; s += 64) {
      unsigned tiles = (SPATIAL-s < 64 ? SPATIAL-s : 64)/16;
      switch (tiles) {
        case 4: RUN_BLOCK(4); break;
        case 3: RUN_BLOCK(3); break;
        case 2: RUN_BLOCK(2); break;
        default: RUN_BLOCK(1); break;
      }
    }
  }
  // A scalar TCDM load waits for outstanding vector stores.
  asm volatile("lw zero, 0(%0)" :: "r"(ct) : "memory");
}

static __attribute__((noinline)) void conv_relu_non_fused(float *ct,
                                                           unsigned elements) {
  for (unsigned i = 0; i < elements; i += 128) {
    unsigned vl = elements - i;
    if (vl > 128) vl = 128;
    asm volatile(
        "vsetvli zero, %[vl], e32, m8, ta, ma\n"
        "vle32.v v0, (%[ptr])\n"
        RELU_ASM("v0")
        "vse32.v v0, (%[ptr])\n"
        : : [vl] "r"((uintptr_t)vl), [ptr] "r"(ct + i), RELU_INPUTS
        : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
  }
  asm volatile("lw zero, 0(%0)" : : "r"(ct) : "memory");
}

#undef RUN_BLOCK
#undef CONV_RELU
