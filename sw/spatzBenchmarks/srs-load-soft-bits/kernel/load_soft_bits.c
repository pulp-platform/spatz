#include "load_soft_bits.h"
#include <stddef.h>
#include <snrt.h>

void load_soft_bits(const int8_t* llrs, uint32_t nof_llr, int8_t* soft, const uint32_t lifting_size, const uint32_t buffer_nodes) {
  uint32_t vl;
  int32_t low = -64;
  int32_t high = 64;
  // First 2 nodes are not transmitted -> setting to 0
  uint32_t remaining = 2 * lifting_size;
  do {
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vmv.v.i v0, 0");
    asm volatile("vse8.v v0, (%0)" :: "r"(soft) : "memory");
    soft      += vl;
    remaining -= vl;
  } while (remaining > 0);
  uint32_t body    = (buffer_nodes - 2) * lifting_size;
  uint32_t to_copy = (nof_llr < body) ? nof_llr : body;
  uint32_t to_zero = body - to_copy;  

  remaining = to_copy;
  while (remaining > 0) {
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vle8.v  v0, (%0)" :: "r"(llrs) : "memory");
    asm volatile("vmax.vx v0, v0, %0" :: "r"(low));
    asm volatile("vmin.vx v0, v0, %0" :: "r"(high));
    asm volatile("vse8.v  v0, (%0)" :: "r"(soft) : "memory");
    llrs      += vl;
    soft      += vl;
    remaining -= vl;
  }
  remaining = to_zero;
  while (remaining > 0) {
    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vmv.v.i v0, 0");
    asm volatile("vse8.v  v0, (%0)" :: "r"(soft) : "memory");
    soft      += vl;
    remaining -= vl;
  }
}