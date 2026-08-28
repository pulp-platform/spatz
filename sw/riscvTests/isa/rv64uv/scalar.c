#include "vector_macros.h"

int main(void) {

  // ensure illegal vtype
  unsigned int vlmul = 5;
  unsigned int vsew  = 3;
  unsigned int vtype = vsew << 3 | vlmul;

  __asm__ volatile (
      "li      t0, -1 \n"
      "vsetvl zero, zero, %[vtype] \n":: [vtype]"r"(vtype):
  );

  asm volatile("mul a0, a1, a2");
  asm volatile("add a0, a1, a2");
  asm volatile("div a0, a1, a2");
  asm volatile("fmul.b f0, f1, f2");
  asm volatile("fmul.h f0, f1, f2");
  asm volatile("fmul.s f0, f1, f2");
  asm volatile("fmul.d f0, f1, f2");

//  asm volatile("fdiv.s f0, %0, %1"::"f"(1.0), "f"(1.0):"f0");
//  asm volatile("fdiv.d f0, %0, %1"::"f"(1.0), "f"(1.0):"f0");

  return 0;
}
