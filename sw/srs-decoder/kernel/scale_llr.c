#include "scale_llr.h"
#include <stddef.h>
#include <snrt.h>
//scaling factor in LDPC = 0.8 in this implementation

void scale_llr(uint8_t *out, const uint8_t *in, uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;
  uint32_t llr_max = 120; 
  uint8_t scaling_mul = 205;
  do {
    asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v v8, (%0)" :: "r"(in) : "memory");
    //Do scaling multiplicatiom
    asm volatile("vmulhu.vx v24, v8, %0" :: "r"(scaling_mul));
    asm volatile("vmul.vx   v28, v8, %0" :: "r"(scaling_mul));
    asm volatile("vmsgtu.vx v0, v28, %0" :: "r"(127));
    asm volatile("vadd.vx   v24, v24, %0, v0.t" :: "r"(1));  
    //Compare input with llr_max
    asm volatile("vmsgtu.vx v0, v8, %0" :: "r"(llr_max));
    //Overwrite results of comparison with llr_inf (do not scale maxs)
    asm volatile("vmerge.vvm v24, v24, v8, v0"); 
    asm volatile("vse8.v v24, (%0)" :: "r"(out): "memory");
    asm volatile("vse8.v v24, (%0)" :: "r"(out + lifting_size): "memory");
    in += vl;
    out += vl;
    remaining    -= vl;
  } while (remaining > 0);
}