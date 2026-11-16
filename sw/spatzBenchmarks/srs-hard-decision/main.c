#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>
#include <math.h>

#include DATAHEADER
#include "kernel/hard_decision.h"
#include "kernel/hard_decision.c"

int hard_decision_check(const uint8_t *packed, const int8_t *soft_bits,
                        uint32_t nbits, bool returned_result) {
  int errors = 0;
  bool expected_no_zero = true;

  for (uint32_t i = 0; i < nbits; ++i) {
    int expected = (soft_bits[i] <= 0) ? 1 : 0;
    int actual   = (packed[i >> 3] >> (7 - (i & 7))) & 1;
    if (expected != actual) {
      if (errors < 20)
        printf("Mismatch bit %u: got=%d exp=%d (soft=%d)\n",
               i, actual, expected, (int)soft_bits[i]);
      errors++;
    }
    if (soft_bits[i] == 0)
      expected_no_zero = false;
  }

  if (returned_result != expected_no_zero) {
    printf("Incorrect return value = %d, expected = %d\n",
           returned_result, expected_no_zero);
    errors++;
  }

  if (errors) printf("hard_decision_packed FAIL errors=%d\n", errors);
  else        printf("hard_decision_packed OK\n");
  return errors;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
  const uint32_t nbits = num_cores * lifting_size;
  const uint32_t nbytes = (nbits + 7) / 8;
  int8_t *soft_bits;
  uint8_t *packed;

  unsigned int timer = (unsigned int)-1;
  bool result = false;

  if (cid == 0) {
    soft_bits = (int8_t *)snrt_l1alloc(nbits * sizeof(int8_t));
    packed    = (uint8_t *)snrt_l1alloc(nbytes);
  }

  if (cid == 0) {
    snrt_dma_start_1d(soft_bits, hard_decision_soft_bits_dram, nbits * sizeof(int8_t));
    snrt_dma_wait_all();
  }

  snrt_cluster_hw_barrier();

  if (cid == 0)
    start_kernel();

  if (cid == 0)
    timer = benchmark_get_cycle();

  if (cid == 0) {
    result = hard_decision_packed(packed, soft_bits, nbits);
  }

  //snrt_cluster_hw_barrier();
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;

  if (cid == 0)
    stop_kernel();

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    long unsigned int bits_per_1000cycle = 1000UL * (nbits/2) / timer;

    printf("\n----- (%u bits) hard_decision_packed -----\n", (nbits)/2);
    printf("The execution took %u cycles.\n", timer);
    printf("The throughput is %lu bits/1000cycle.\n", bits_per_1000cycle);
  }

  snrt_cluster_hw_barrier();

  if (cid == 0)
    hard_decision_check(packed, soft_bits, nbits, result);

  snrt_cluster_hw_barrier();

  return 0;
}