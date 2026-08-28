#include "vector_macros.h"

int main(void) {

  __asm__ volatile (
      "vsetvli zero, zero, e64, mf8, ta, ma\n"
  );

  asm volatile("vadd.vv v24, v8, v16");

  return 0;
}
