// Author: CMY

#include "vector_macros.h"

// static volatile uint8_t ALIGNED_I8[16] __attribute__((aligned(AXI_DWIDTH))) = {
//     0xe0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88,
//     0x88, 0xae, 0x08, 0x91, 0x02, 0x59, 0x11, 0x89};

// void TEST_CASE1(void) {
//   VSET(15, e8, m1);
//   asm volatile("vle8.v v0, (%0)" ::"r"(&ALIGNED_I8[1]));
//   VCMP_U8(1, v0, 0xd3, 0x40, 0xd1, 0x84, 0x48, 0x89, 0x88, 0x88, 0xae, 0x08,
//           0x91, 0x02, 0x59, 0x11, 0x89);
// }

// Positive-stride tests
void TEST_CASE1(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  uint64_t stride = 3;
  asm volatile("vlse8.v v1, (%0), %1" ::"r"(INP1), "r"(stride));
  VCMP_U8(1, v1, 0x9f, 0x20, 0x05, 0xaa);
}

void TEST_CASE2(void) {
  VSET(4, e8, m1);
  VLOAD_8(v2, 0, 1, 2, 3);
  volatile uint8_t INP[] = {0xff, 0x00, 0x0f, 0xf0};
  asm volatile("vluxei8.v v1, (%0), v2" ::"r"(INP));
  VCMP_U8(1, v1, 0xff, 0x00, 0x0f, 0xf0);
}

void TEST_CASE3(void) {
  VSET(4, e8, m1);
  VLOAD_8(v2, 0, 1, 2, 3);
  volatile uint8_t INP[] = {0xff, 0x00, 0x0f, 0xf0};
  asm volatile("vloxei8.v v1, (%0), v2" ::"r"(INP));
  VCMP_U8(1, v1, 0xff, 0x00, 0x0f, 0xf0);
}

void TEST_CASE4(void) {
  VSET(4, e8, m1);
  volatile uint8_t INP1[] = {0x9f, 0xe4, 0x19, 0x20, 0x8f, 0x2e, 0x05, 0xe0,
                             0xf9, 0xaa, 0x71, 0xf0, 0xc3, 0x94, 0xbb, 0xd3};
  uint64_t stride = 3;
  VLOAD_8(v0, 0xAA);
  VCLEAR(v1);
  asm volatile("vlse8.v v1, (%0), %1, v0.t" ::"r"(INP1), "r"(stride));
  VCMP_U8(4, v1, 0x00, 0x20, 0x00, 0xaa);
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();

  EXIT_CHECK();
}