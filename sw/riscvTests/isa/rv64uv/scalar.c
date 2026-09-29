#include "vector_macros.h"

int main(void) {

  // ensure illegal vtype
  unsigned int vlmul = 4;
  unsigned int illegal_vsew  = 4;
  unsigned int vtype = illegal_vsew << 3 | vlmul;

  __asm__ volatile (
      "vsetvl zero, zero, %[vtype] \n":: [vtype]"r"(vtype):
  );

  // Scalar instructions should not be affected by the illegal vtype
  asm volatile("mul a0, a1, a2");
  asm volatile("add a0, a1, a2");
  asm volatile("div a0, a1, a2");
  asm volatile("fmul.b f0, f1, f2");
  asm volatile("fmul.h f0, f1, f2");
  asm volatile("fmul.s f0, f1, f2");
#if ELEN == 64
  asm volatile("fmul.d f0, f1, f2");
#endif

  return 0;
}
