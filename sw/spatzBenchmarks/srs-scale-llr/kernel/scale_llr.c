#include "scale_llr.h"
#include <stddef.h>
#include <snrt.h>
//scaling factor in LDPC = 0.8 in this implementation

void scale_llr(uint8_t *out, const uint8_t *in, uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;
  uint32_t llr_max = 120; 
  uint8_t scaling_mul = 205;
  asm volatile("vsetvli %0, %1, e8, m8, ta, mu" : "=r"(vl) : "r"(remaining));
  asm volatile("vle8.v v8, (%0)" :: "r"(in) : "memory");
  asm volatile("vmsleu.vx v0, v8, %0" :: "r"(llr_max));
  asm volatile("vmul.vx v28, v8, %0" :: "r"(scaling_mul));
  asm volatile("vmulhu.vx v8, v8, %0, v0.t" :: "r"(scaling_mul));
  asm volatile("vmsgtu.vx v1, v28, %0" :: "r"(127));
  asm volatile("vmand.mm v0, v0, v1");
  asm volatile("vadd.vi v24, v8, 1, v0.t");
  asm volatile("vse8.v v24, (%0)" :: "r"(out) : "memory");
  asm volatile("vse8.v v24, (%0)" :: "r"(out + lifting_size) : "memory");
    // asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    // asm volatile("vle8.v v8, (%0)" :: "r"(in) : "memory");
    // //Compare input with llr_max
    // asm volatile("vmsleu.vx v0, v8, %0" :: "r"(llr_max));
    // //Do scaling multiplication (do not scale maxs)
    // asm volatile("vmulhu.vx v24, v8, %0, v0.t" :: "r"(scaling_mul));
    // asm volatile("vse8.v v24, (%0)" :: "r"(out): "memory");
    // asm volatile("vse8.v v24, (%0)" :: "r"(out + lifting_size): "memory");
}