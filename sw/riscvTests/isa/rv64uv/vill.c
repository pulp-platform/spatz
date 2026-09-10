#include "vector_macros.h"

int main(void) {

  // ensure illegal vtype
  unsigned int vlmul = 4;
  unsigned int illegal_vsew  = 4;
  unsigned int vtype = illegal_vsew << 3 | vlmul;

  __asm__ volatile (
      "vsetvl zero, zero, %[vtype] \n":: [vtype]"r"(vtype):
  );

  asm volatile("vadd.vv v24, v8, v16");

  return 0;
}
