#include "vector_macros.h"

int main(void) {

  unsigned int vtype = 1 << 31;

  __asm__ volatile (
      "li      t0, -1 \n"
      "vsetvl zero, zero, t0 \n"
  );

  asm volatile("vadd.vv v24, v8, v16");

  return 0;
}
