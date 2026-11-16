#include "analyze_var_to_check_msgs.h"
#include <stddef.h>
#include <stdint.h>
#include <snrt.h>
#define ELENB 8

void analyze_var_to_check_msgs(int8_t *min_var_to_check, int8_t *second_min_var_to_check, uint8_t *min_var_to_check_index, uint8_t *sign_prod_var_to_check, const int8_t *rotated_node, const uint32_t var_node, const uint32_t lifting_size) {
  const uint32_t r = (uint32_t)((uintptr_t)rotated_node & (ELENB - 1));
  const int8_t *src = rotated_node - r;

  uint32_t vlmax, vl;
  asm volatile("vsetvli %0, zero, e8, m8, ta, mu" : "=r"(vlmax));
  const uint32_t step = vlmax - ELENB;

  uint32_t remaining = lifting_size;
  uint32_t n  = remaining < step ? remaining : step;
  uint32_t nl = n + r;

  asm volatile("vsetvli %0, %1, e8, m8, ta, mu" : "=r"(vl) : "r"(nl));
  asm volatile("vle8.v v16, (%0)" :: "r"(src) : "memory");
  asm volatile("vsetvli %0, %1, e8, m8, ta, mu" : "=r"(vl) : "r"(n));
  asm volatile("vslidedown.vx v24, v16, %0" :: "r"(r));

  asm volatile("vmsle.vi v1, v24, -1");
  asm volatile("vrsub.vi v16, v24, 0");
  asm volatile("vmax.vv v24, v24, v16");
  asm volatile("vle8.v v8, (%0)" :: "r"(min_var_to_check) : "memory");
  asm volatile("vmslt.vv v0, v24, v8");

  asm volatile("vmerge.vvm v16, v24, v8, v0");
  asm volatile("vmerge.vvm v8, v8, v24, v0");
  asm volatile("vse8.v v8, (%0)" :: "r"(min_var_to_check) : "memory");
  asm volatile("vle8.v v8, (%0)" :: "r"(min_var_to_check_index) : "memory");
  asm volatile("vmerge.vxm v8, v8, %0, v0" :: "r"(var_node));
  asm volatile("vse8.v v8, (%0)" :: "r"(min_var_to_check_index) : "memory");
  asm volatile("vle8.v v8, (%0)" :: "r"(second_min_var_to_check) : "memory");
  asm volatile("vmslt.vv v0, v24, v8");

  asm volatile("vmerge.vvm v8, v8, v16, v0");
  asm volatile("vse8.v v8, (%0)" :: "r"(second_min_var_to_check) : "memory");
  asm volatile("vmand.mm v0, v1, v1");
  asm volatile("vle8.v v16, (%0)" :: "r"(sign_prod_var_to_check) : "memory");
  asm volatile("vxor.vi v16, v16, 1, v0.t");
  asm volatile("vse8.v v16, (%0)" :: "r"(sign_prod_var_to_check) : "memory");
}