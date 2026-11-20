#include "demodulate_soft_BPSK.h"
#include <stddef.h>
#include <snrt.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h> 
#define M_SQRT2f32 1.41421356237309504880f

void demodulate_soft_BPSK(int8_t *llrs, const float *symbols, const float *noise_vars, uint32_t nof_symbols) {
  uint32_t remaining = nof_symbols;
  uint32_t vl;
  uint32_t stride = 4;
  uint32_t stride2 = 8;
  float m2_sqrt = 2.0f * M_SQRT2f32;
  const float RANGE_LIMIT_FLOAT = 24.0f;
  const float RANGE_LIMIT_FLOAT_NEG = -24.0f;
  const float quant_const = 120.0f / RANGE_LIMIT_FLOAT;
  //LLR_MAX = 120
  do {
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vlse32.v v8,  (%0), %1" :: "r"(symbols),     "r"(stride2) : "memory"); // Re
    asm volatile("vlse32.v v16, (%0), %1" :: "r"(symbols + 1), "r"(stride2) : "memory"); // Im

    asm volatile("vfadd.vv v8, v8, v16");
    asm volatile("vle32.v  v24, (%0)" :: "r"(noise_vars) : "memory");
    asm volatile("vmfle.vf v0, v24, %0" :: "f"(0.0f));
    asm volatile("vfmul.vf v8, v8, %0" :: "f"(m2_sqrt));
    asm volatile("vfdiv.vv v8, v8, v24");

    asm volatile("vfmin.vf v8, v8, %0" :: "f"(RANGE_LIMIT_FLOAT));
    asm volatile("vfmax.vf v8, v8, %0" :: "f"(RANGE_LIMIT_FLOAT_NEG));
    asm volatile("vfmul.vf v8, v8, %0" :: "f"(quant_const));

    asm volatile("vsetvli zero, zero, e16, m4, ta, ma");
    asm volatile("fsrmi 1");
    asm volatile("vfncvt.f.f.w v16, v8");
    asm volatile("vsetvli zero, zero, e8, m2, ta, ma");
    asm volatile("fsrmi 4");
    asm volatile("vfncvt.x.f.w v24, v16");
    asm volatile("fsrmi 0");
    asm volatile("vmerge.vim v24, v24, 0, v0");
    asm volatile("vse8.v v24, (%0)" :: "r"(llrs) : "memory");

    remaining  -= vl;
    symbols    += 2 * vl;
    llrs       += vl;
    noise_vars += vl;
} while (remaining > 0);
}
