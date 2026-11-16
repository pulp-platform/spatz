#include "compute_var_to_check_msgs.h"
#include <stddef.h>
#include <snrt.h>
#include <stdio.h>

void compute_var_to_check_msgs(int8_t *this_var_to_check, const int8_t *this_soft_bits, const uint16_t *vidx,
                               const int8_t *this_check_to_var, const uint32_t seg_len)
{
  uint32_t remaining = seg_len;
  uint32_t vl;
  int8_t llr_max     =  120;
  int8_t llr_min     = -120;
  int8_t llr_inf     =  127;
  int8_t llr_inf_neg = -127;
  while (remaining > 0) {
    asm volatile("vsetvli %0, %1, e8, m4, ta, mu" : "=r"(vl) : "r"(remaining));
    if (vl > 16) { vl = 16; asm volatile("vsetvli %0, %0, e8, m4, ta, mu" : "+r"(vl)); }
    asm volatile("vle16.v v0, (%0)" :: "r"(vidx) : "memory");
    asm volatile("vle8.v v16, (%0)" :: "r"(this_check_to_var) : "memory");
    asm volatile("vloxei16.v v8, (%0), v0" :: "r"(this_soft_bits) : "memory");

    asm volatile("vssub.vv v24, v8, v16");
    asm volatile("vmin.vx  v24, v24, %0" :: "r"(llr_max));
    asm volatile("vmax.vx  v24, v24, %0" :: "r"(llr_min));

    asm volatile("vmseq.vx v1, v8,  %0" :: "r"(llr_inf));
    asm volatile("vmseq.vx v3, v16, %0" :: "r"(llr_inf_neg));
    asm volatile("vmor.mm  v1, v1, v3");

    asm volatile("vmseq.vx v2, v8,  %0" :: "r"(llr_inf_neg));
    asm volatile("vmseq.vx v3, v16, %0" :: "r"(llr_inf));
    asm volatile("vmor.mm  v2, v2, v3");

    asm volatile("vmandn.mm v0, v1, v2");
    asm volatile("vmerge.vxm v24, v24, %0, v0" :: "r"(llr_inf));
    asm volatile("vmandn.mm v0, v2, v1");
    asm volatile("vmerge.vxm v24, v24, %0, v0" :: "r"(llr_inf_neg));

    asm volatile("vse8.v v24, (%0)" :: "r"(this_var_to_check) : "memory");
    asm volatile("vse8.v v24, (%0)" :: "r"(this_var_to_check + seg_len) : "memory");
    
    vidx              += vl;
    this_check_to_var += vl;
    this_var_to_check += vl;
    remaining         -= vl;
  }
}