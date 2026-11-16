#include "compute_soft_bits.h"
#include <stddef.h>
#include <snrt.h>

void compute_soft_bits(int8_t *this_soft_bits, const int8_t *this_var_to_check, const int8_t *this_check_to_var, const uint32_t lifting_size) {
  uint32_t remaining = lifting_size;
  uint32_t vl;
  int8_t llr_max = 120;
  int8_t llr_min = -120;
  int8_t llr_inf = 127;
  int8_t llr_inf_neg = -127;
  asm volatile("vsetvli %0, %1, e8, m8, ta, mu" : "=r"(vl) : "r"(remaining));
  // Load data
  asm volatile("vle8.v v8, (%0)" :: "r"(this_var_to_check) : "memory"); 
  asm volatile("vmseq.vx v6, v8,  %0" :: "r"(llr_inf));
  asm volatile("vle8.v v16, (%0)" :: "r"(this_check_to_var) : "memory");

  // Do saturated summ
  asm volatile("vsadd.vv v24, v8, v16");

  //Compare with max value
  asm volatile("vmsgt.vx v2, v24, %0" :: "r"(llr_max));
  asm volatile("vmor.mm  v2, v2, v6");
  asm volatile("vmseq.vx v6, v16, %0" :: "r"(llr_inf));
  asm volatile("vmor.mm  v2, v2, v6");

  asm volatile("vmslt.vx v4, v24, %0" :: "r"(llr_min));
  asm volatile("vmseq.vx v6, v8,  %0" :: "r"(llr_inf_neg));
  asm volatile("vmor.mm  v4, v4, v6");
  asm volatile("vmseq.vx v6, v16, %0" :: "r"(llr_inf_neg));
  asm volatile("vmor.mm  v4, v4, v6");

  asm volatile("vmandn.mm v0, v2, v4");
  asm volatile("vmerge.vxm v24, v24, %0, v0" :: "r"(llr_inf));
  asm volatile("vmandn.mm v0, v4, v2");
  asm volatile("vmerge.vxm v24, v24, %0, v0" :: "r"(llr_inf_neg));
  asm volatile("vse8.v v24, (%0)" :: "r"(this_soft_bits) : "memory");
}