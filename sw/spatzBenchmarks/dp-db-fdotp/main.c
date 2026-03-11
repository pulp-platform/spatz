// Copyright 2022 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Imfeld Jonah,ETH Zurich
//         Diyou Shen,  ETH Zurich

#include <benchmark.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/db-fdotp.c"

// Uncomment for fine grained timing
// #define TIMING 1

#define DBG_LVL_NONE 0
#define DBG_LVL_ERR 1
#define DBG_LVL_WARN 2
#define DBG_LVL_INFO 3
#define DBG_LVL_DBG 4
#define SNRT_NFPU_PER_CORE 4

#define DBG_LVL DBG_LVL_NONE

#define DEBUG(func, lvl) \
  if (lvl <= DBG_LVL) { \
    func; \
  }

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Shared Variables between cores
double *l1_buf;
// I had some problems with accumulating between the two cores, so I use a global result array
double *result_ma;
double *result_simple;

double result[2] = { 0 };


// NaN detection
int is_nan_double(double x) {
    union {
        double d;
        uint64_t u;
    } value;

    value.d = x;

    uint64_t exponent = (value.u >> 52) & 0x7FF;
    uint64_t mantissa = value.u & 0xFFFFFFFFFFFFF; // lower 52 bits

    #ifdef PRINT_RESULT
    if ((exponent == 0x7FF) && (mantissa != 0)) {
      printf("Calculation result is NaN!\n");
    } else {
      printf("Calculation result is not NaN!\n");
    }
    #endif

    return (exponent == 0x7FF) && (mantissa != 0);
}

static inline int fp_check(const double a, const double b) {
  const double threshold = 0.00001;

  // Check nan
  if (is_nan_double(a)){
    return 1;
  }

  // Absolute value
  double comp = a - b;
  if (comp < 0)
    comp = -comp;

  return comp > threshold;
}


/**
 * @brief Used to reduce a vector of doubles to a single value
 *
 * @param a Pointer to the vector of doubles
 * @param avl The number of elements in the vector
 */
double vreduce_sum(double *a, unsigned int avl) {
  const unsigned int orig_avl = avl;
  unsigned int vl;
  double res;

  asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
  asm volatile("vmv.s.x v0, zero");

  do {
    asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v8, (%0)" ::"r"(a));
    asm volatile("vfredusum.vs v0, v8, v0");

    DEBUG(printf("vl = %u, avl = %u\n", vl, avl), DBG_LVL_DBG);

    a += vl;
    avl -= vl;
  } while (avl > 0);

  asm volatile("vsetvli zero, %0, e64, m8, ta, ma" ::"r"(orig_avl));
  asm volatile("vfmv.f.s %0, v0" : "=f"(res));

  return res;
}

/**
 * @brief Performs a dot product of two vectors using double buffering considering the memory layout.
 *
 * @param dotp_A_dram Pointer to the first vector in DRAM
 * @param dotp_B_dram Pointer to the second vector in DRAM
 * @param vec_dim The dimension of the vectors
 * @param l1_buf Pointer to a buffer in L1 memory
 * @param l1_buf_len Length of the L1 buffer in bytes
 * @return The result of the dot product for the current core
 */
double dp_dotp_db_ma(
  double *dotp_A_dram,
  double *dotp_B_dram,
  const unsigned int vec_dim,

  double *l1_buf,
  const unsigned int l1_buf_len
) {
  const unsigned int cid = snrt_cluster_core_idx();
  const unsigned int num_cores = snrt_cluster_core_num();

  // HW and algo configuration
  const unsigned int CHUNK_SIZE = 8; // Half of L1 banks
  const unsigned int T_S = sizeof(double);

  if (l1_buf_len % 32 != 0) {
    printf("L1 buffer length must be a multiple of the number of banks times the number of cores (32).\n");
    return 0.0;
  }

  // Number of element per iteration
  const unsigned MAX_NUM_ELEM_BUF = l1_buf_len / 4;
  const unsigned int NUM_ELEM_PER_ITER = MIN(MAX_NUM_ELEM_BUF, vec_dim / 2);

  // Pointers to the left and right side of L1 memory
  double *a0 = l1_buf;
  double *b0 = l1_buf + 2 * NUM_ELEM_PER_ITER;
  double *a1 = a0 + CHUNK_SIZE;
  double *b1 = b0 + CHUNK_SIZE;
  double *temp = NULL;
  double res = 0.0;
  result[cid] = 0.0;

  // Pointer increment
  const unsigned int NUM_ELEM_PER_CORE = NUM_ELEM_PER_ITER / num_cores;
  const unsigned int CORE_OFFSET = cid * NUM_ELEM_PER_ITER;
  const unsigned int DMA2D_REPEAT = NUM_ELEM_PER_ITER / CHUNK_SIZE;

  // if (cid == 0) {
  //   printf("a0: %p, b0: %p, a1: %p, b1: %p\n", a0, b0, a1, b1);
  //   printf("MAX_NUM_ELEM_BUF: %u\n", MAX_NUM_ELEM_BUF);
  //   printf("NUM_ELEM_PER_ITER: %u\n", NUM_ELEM_PER_ITER);
  //   printf("2D DMA REPEAT: %u\n", DMA2D_REPEAT);
  // }

  // Timing
  unsigned int load_timer = (unsigned int)-1;
  unsigned int calc_timer = (unsigned int)-1;
  unsigned int calc_timer_avg = 0;
  unsigned dma_wait_tot = 0;
  unsigned int performance_timer = 0;

  // Current element to load from DRAM
  unsigned int load_idx = 0;

  // Start the initial DMA transfer
  if (cid == 0) {
    DEBUG(printf("Load index: %u, vec_dim: %u\n", load_idx, vec_dim), DBG_LVL_DBG);
    snrt_dma_start_2d(
      a0,
      dotp_A_dram + load_idx,
      CHUNK_SIZE * T_S,
      2 * CHUNK_SIZE * T_S,
      CHUNK_SIZE * T_S,
      DMA2D_REPEAT
    );
    snrt_dma_start_2d(
      b0,
      dotp_B_dram + load_idx,
      CHUNK_SIZE * T_S,
      2 * CHUNK_SIZE * T_S,
      CHUNK_SIZE * T_S,
      DMA2D_REPEAT
    );
    performance_timer = benchmark_get_cycle();
  }

  snrt_cluster_hw_barrier();

  do {

    // Wait for the data load of the current calc data
    if (cid == 0) {
#ifdef TIMING
      unsigned dma_wait = benchmark_get_cycle();
#endif
      snrt_dma_wait_all();
#ifdef TIMING
      dma_wait = benchmark_get_cycle() - dma_wait;
      dma_wait_tot += dma_wait;
#endif
    }

    load_idx += NUM_ELEM_PER_ITER;

    snrt_cluster_hw_barrier();

    // Start the DMA transfer on chunk i + 1
    if (cid == 0) {
      if (load_idx < vec_dim) {
        DEBUG(printf("Load index: %u, vec_dim: %u\n", load_idx, vec_dim), DBG_LVL_DBG);
        snrt_dma_start_2d(
          a1,
          dotp_A_dram + load_idx,
          CHUNK_SIZE * T_S,
          2 * CHUNK_SIZE * T_S,
          CHUNK_SIZE * T_S,
          DMA2D_REPEAT
        );
        snrt_dma_start_2d(
          b1,
          dotp_B_dram + load_idx,
          CHUNK_SIZE * T_S,
          2 * CHUNK_SIZE * T_S,
          CHUNK_SIZE * T_S,
          DMA2D_REPEAT
        );
      }
    }

#ifdef TIMING
    if (cid == 0) {
      calc_timer = benchmark_get_cycle();
    }
#endif

    DEBUG(printf("Core %u - Starting fdotp_v64b_ma with a0: %p, b0: %p, CORE_OFFSET: %u, NUM_ELEM_PER_CORE: %u\n",
           cid, a0 + CORE_OFFSET, b0 + CORE_OFFSET,
           CORE_OFFSET, NUM_ELEM_PER_CORE), DBG_LVL_DBG);
    result[cid] = fdotp_v64b_ma(
      a0 + CORE_OFFSET,
      b0 + CORE_OFFSET,
      NUM_ELEM_PER_CORE,
      result[cid]
    );

#ifdef TIMING
    if (cid == 0) {
      calc_timer = benchmark_get_cycle() - calc_timer;
      calc_timer_avg += calc_timer;
    }
#endif

  // Swap the pointers
    temp = a0;
    a0 = a1;
    a1 = temp;

    temp = b0;
    b0 = b1;
    b1 = temp;

  } while (load_idx < vec_dim);

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    performance_timer = benchmark_get_cycle() - performance_timer;
  }

#ifdef TIMING
  if (cid == 0) {
    printf("The DMA wait took %u cycles on average.\n", dma_wait_tot / iter);
    printf("The calculation took %u cycles on average.\n", calc_timer_avg / iter);
  }
#endif

  // End dump, Record the time
  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * vec_dim / performance_timer;
    long unsigned int utilization =
        performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    printf("\n----- (%d) dp fdotp MA -----\n", vec_dim);
    printf("The calculation took %u cycles.\n", performance_timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  snrt_cluster_hw_barrier();

  return res;
}


/**
 * @brief Performs a dot product of two vectors using double buffering.
 *
 * @param dotp_A_dram Pointer to the first vector in DRAM
 * @param dotp_B_dram Pointer to the second vector in DRAM
 * @param vec_dim The dimension of the vectors
 * @param l1_buf Pointer to a buffer in L1 memory
 * @param l1_buf_len Length of the L1 buffer in bytes
 * @return The result of the dot product for the current core
 */
double dp_dotp_db_simple(
  double *dotp_A_dram,
  double *dotp_B_dram,
  const unsigned int vec_dim,

  double *l1_buf,
  const unsigned int l1_buf_len // In sizeof doubles
) {
  const unsigned int cid = snrt_cluster_core_idx();
  const unsigned int num_cores = snrt_cluster_core_num();

  const unsigned int T_S = sizeof(double);

  if (l1_buf_len % 32 != 0) {
    printf("L1 buffer length must be a multiple of the number of banks times the number of cores (32).\n");
    return 0.0;
  }

  // Split the buffer into left and right side.
  double *a0 = l1_buf;
  double *b0 = l1_buf + (l1_buf_len / 4);
  double *a1 = l1_buf + (l1_buf_len / 2);
  double *b1 = l1_buf + (3 * l1_buf_len / 4);
  double *temp = NULL;
  double res = 0.0;
  result[cid] = 0.0;

  // Pointer increment
  const unsigned int NUM_ELEM_PER_ITER = b0 - a0 > vec_dim / 2 ? vec_dim / 2 : b0 - a0;
  const unsigned int NUM_ELEM_PER_CORE = NUM_ELEM_PER_ITER / num_cores;
  const unsigned int CORE_OFFSET = cid * NUM_ELEM_PER_CORE;
  // printf("NUM_ELEM_PER_ITER: %u\n", NUM_ELEM_PER_ITER);

  // Timing variables
  unsigned int calc_timer = 0;
  unsigned int calc_timer_avg = 0;
  unsigned dma_wait_tot = 0;
  unsigned int performance_timer = 0;

  // Which elements to load next in doubles
  unsigned int load_idx = 0;

  if (cid == 0) {
    snrt_dma_start_1d(
      a0,
      dotp_A_dram + load_idx, // load index is in size of type
      NUM_ELEM_PER_ITER * T_S // in bytes
    );
    snrt_dma_start_1d(
      b0,
      dotp_B_dram + load_idx,
      NUM_ELEM_PER_ITER * T_S
    );
    start_kernel();
    performance_timer = benchmark_get_cycle();
  }

  do {
    // Wait for the data load of the current calc data
    if (cid == 0) {
#ifdef TIMING
      unsigned dma_wait = benchmark_get_cycle();
#endif
      snrt_dma_wait_all();
#ifdef TIMING
      dma_wait = benchmark_get_cycle() - dma_wait;
      dma_wait_tot += dma_wait;
#endif
    }

    load_idx += NUM_ELEM_PER_ITER; // increment the index of the next load

    snrt_cluster_hw_barrier();

    // Load the i + 1 subvector
    if (cid == 0) {
      if (load_idx < vec_dim) {
        snrt_dma_start_1d(
          a1,
          dotp_A_dram + load_idx,
          NUM_ELEM_PER_ITER * T_S
        );
        snrt_dma_start_1d(
          b1,
          dotp_B_dram + load_idx,
          NUM_ELEM_PER_ITER * T_S
        );
      }
    }

    #ifdef TIMING
    if (cid == 0) {
      calc_timer = benchmark_get_cycle();
    }
    #endif

    // Calculate the i sub-vector dot product
    result[cid] = fdotp_v64b(
      a0 + CORE_OFFSET,
      b0 + CORE_OFFSET,
      NUM_ELEM_PER_CORE,
      result[cid]
    );

#ifdef TIMING
    if (cid == 0) {
      calc_timer = benchmark_get_cycle() - calc_timer;
      calc_timer_avg += calc_timer;
    }
#endif

  // Swap the pointers
    temp = a0;
    a0 = a1;
    a1 = temp;

    temp = b0;
    b0 = b1;
    b1 = temp;
  } while (load_idx < vec_dim);

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    performance_timer = benchmark_get_cycle() - performance_timer;
    stop_kernel();
  }


  #ifdef TIMING
  if (cid == 0) {
    printf("The DMA wait took %u cycles on average.\n", dma_wait_tot / iter);
    printf("The calculation took %u cycles on average.\n", calc_timer_avg / iter);
  }
  #endif

  if (cid == 0) {
    long unsigned int performance = 1000 * 2 * vec_dim / performance_timer;
    long unsigned int utilization =
    performance / (2 * num_cores * SNRT_NFPU_PER_CORE);

    printf("\n----- (%d) dp fdotp Simple -----\n", vec_dim);
    printf("The calculation took %u cycles.\n", performance_timer);
    printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
      performance, utilization);
    }

  snrt_cluster_hw_barrier();

  return res;
}


int main() {

  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  uint32_t spm_size = 126;

  if (cid == 0) {
    // Init the cache
    l1d_spm_config(spm_size);
  }

  unsigned int dim = dotp_l.M;
  const unsigned int T_S = sizeof(double);

  const unsigned int NUM_BANKS = 16;

  // The minimal buffer size is 32 doubles (16 banks * 2 cores)
  // const unsigned int SCALAR = dim / 32;
  // const unsigned int BUF_SIZE = num_cores * NUM_BANKS * SCALAR; // in doubles
  unsigned int BUF_SIZE = 12288; // in doubles, 64kB

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  unsigned max_dim = dim;

  unsigned int current_dim = dotp_l.M;

  while (current_dim <= 32768) {
    unsigned int test_buf_size = BUF_SIZE; // Start small (64kB)
    int out_of_mem = 0;

    while (test_buf_size <= 16384 && !out_of_mem) {
      if (cid == 0) {
        // Attempt allocation for this specific iteration
        l1_buf = (double *)snrt_l1alloc(test_buf_size * sizeof(double));

        if (l1_buf == NULL) {
          printf("Reached L1 limit at BUF_SIZE: %u. Stopping size growth.\n", test_buf_size);
            out_of_mem = 1;
        } else {
          printf("Running: Dim %u, Buf %u (Allocated at %p)\n", current_dim, test_buf_size, l1_buf);
        }
      }

      // Synchronize: Don't let other cores start until CID 0 checks the pointer
      snrt_cluster_hw_barrier();

      // Broadcast the out_of_mem status or check l1_buf
      if (l1_buf == NULL) break;

      dp_dotp_db_simple(
        (double *)dotp_A_dram,
        (double *)dotp_B_dram,
        current_dim,
        l1_buf,
        test_buf_size
      );

      if (cid == 0) {
        // IMPORTANT: Reset L1 space for the next loop if your runtime allows.
        l1d_spm_config(spm_size);
      }

      test_buf_size *= 2;
      snrt_cluster_hw_barrier();
    }
    current_dim *= 2;
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();
  set_eoc();

  return 0;
}
