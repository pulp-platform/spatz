#include "vector_macros.h"

int main(void) {
  unsigned int vlmul = 5;
  unsigned int vsew  = 3;
  unsigned int vtype = vsew << 3 | vlmul;

  __asm__ volatile (
      "li      t0, -1 \n"
      "vsetvl zero, zero, %[vtype] \n":: [vtype]"r"(vtype):
  );

  asm volatile("vadd.vv v24, v8, v16");

  return 0;
}
