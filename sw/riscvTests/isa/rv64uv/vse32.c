#include "vector_macros.h"


//**********Checking functionality of vse32********//
void TEST_CASE1(void) {
  volatile uint32_t ALIGNED_I32[1024];
  VSET(16, e32, m1);
  VLOAD_32(v0, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  asm volatile("vse32.v v0, (%0)" ::"r"(ALIGNED_I32));
  VVCMP_U32(1, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}

//*******Checking functionality of vse32 with different values of masking
// register******//
void TEST_CASE2(void) {
  volatile uint32_t ALIGNED_I32[1024]={0};
  VSET(16, e32, m1);
  VLOAD_32(v3, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  VLOAD_8(v0, 0xFF, 0xFF);
  asm volatile("vse32.v v3, (%0), v0.t" ::"r"(ALIGNED_I32));
  VCLEAR(v3);
  VVCMP_U32(2, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}

void TEST_CASE3(void) {
  volatile uint32_t ALIGNED_I32[1024];
  VSET(16, e32, m1);
  VLOAD_32(v3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse32.v v3, (%0)" ::"r"(ALIGNED_I32));
  VCLEAR(v3);
  VLOAD_32(v3, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  VLOAD_8(v0, 0x00, 0x00);
  asm volatile("vse32.v v3, (%0), v0.t" ::"r"(ALIGNED_I32));
  VCLEAR(v3);
  VVCMP_U32(3, ALIGNED_I32, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16);
}

void TEST_CASE4(void) {
  volatile uint32_t ALIGNED_I32[1024];
  VSET(16, e32, m1);
  VLOAD_32(v3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  asm volatile("vse32.v v3, (%0)" ::"r"(ALIGNED_I32));
  VCLEAR(v3);
  VLOAD_32(v3,  0x11111111, 0x22222222, 0x33333333, 0x44444444,
           0x55555555, 0x66666666, 0x77777777, 0x88888888, 0x99999999,
           0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd, 0xeeeeeeee,
           0xffffffff,0x00000000);
  VLOAD_8(v0, 0xAA, 0xAA);
  asm volatile("vse32.v v3, (%0), v0.t" ::"r"(ALIGNED_I32));
  VCLEAR(v3);
  VVCMP_U32(4, ALIGNED_I32, 1, 0x22222222, 3, 0x44444444, 5, 0x66666666, 7,
            0x88888888, 9, 0xaaaaaaaa, 11, 0xcccccccc, 13, 0xeeeeeeee, 15,
            0x00000000);
}

// change LMUL and EW
void TEST_CASE5(void) {
  volatile uint32_t ALIGNED_I32[1024] = {0};
  VSET(16, e32, m1);
  VLOAD_32(v8, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348, 0x9fa831c7,
           0x38197598, 0x18931795, 0x81937598, 0x18747547, 0x3eeeeeee,
           0x90139301, 0xab8b9148, 0x90318509, 0x31897598, 0x83195999,
           0x89139848);
  VSET(16, e8, m2); // ? uncertain
  asm volatile("vse32.v v8, (%0)" ::"r"(ALIGNED_I32));
  VCLEAR(v8);
  VVCMP_U32(5, ALIGNED_I32, 0x9fe41920, 0xf9aa71f0, 0xa11a9384, 0x99991348,
            0x9fa831c7, 0x38197598, 0x18931795, 0x81937598, 0x18747547,
            0x3eeeeeee, 0x90139301, 0xab8b9148, 0x90318509, 0x31897598,
            0x83195999, 0x89139848);
}


int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();
  TEST_CASE2();
  TEST_CASE3();
  TEST_CASE4();
  TEST_CASE5();

  EXIT_CHECK();
}