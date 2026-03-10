// Copyright 2024 ETH Zurich and University of Bologna.
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

// Author: Tim Frey,    ETH Zurich
//         Diyou Shen,  ETH Zurich


#include "dp-db-fmatmul.h"
#include <stddef.h>


#include <benchmark.h>
#include <snrt.h>
#include <stddef.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define DEBUG 1
#if DEBUG == 0
#define debug_print(fmt, ...)
#else
#define debug_print(fmt, ...)                  \
    if (snrt_cluster_core_idx() == 0) \
      printf(fmt __VA_OPT__(, ) __VA_ARGS__);
#endif
#define TS sizeof(double)  // TYPE SIZE

void print_matrix(const char* name, const double* matrix, const unsigned int M,
                  const unsigned int N) {
  if (DEBUG == 0) return;
  for (unsigned int i = 0; i < M; ++i) {
    debug_print("%s[%d] = [", name, i);
    for (unsigned int j = 0; j < N; ++j) {
      debug_print("%.2f, ", matrix[i * N + j]);
    }
    debug_print("]\n");
  }
}

const unsigned int KERNEL_SIZE = 4;
const unsigned int VL = 32;  // VECTOR LENGTH

// matrix multiplication c = a * b
unsigned int dp_fmatmul_double_buffering_kernel(
    double* c,                    // result in DRAM
    const double* a,              // a in DRAM
    const double* b,              // b in DRAM
    const double* buf,            // L1 buffer
    const unsigned int BUF_SIZE,  // size of buffer in words
    const unsigned int M,         // a matrix rows
    const unsigned int K,         // a matrix columns / b matrix rows
    const unsigned int N          // b matrix columns
) {
  // Only using BUF_SIZE words in buffer - which has to point to the L1

  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // STEP SIZE
  const unsigned int M_ = num_cores * KERNEL_SIZE;  // rows of a to be processed
  const unsigned int N_ = VL;  // columns of b to be processed

  // Chunk size in K dimension
  //  = columns of a
  //  = rows of b
  // to be processed at a time
  const unsigned int K_ = 128; // change to 64 if needed

  // Devide L1 buffer
  const unsigned int A_SIZE = M_ * K_;
  const unsigned int B_SIZE = K_ * N_;
  const unsigned int C_SIZE = M_ * N_;

  // double buffering of b in inner loop
  const unsigned BUF_SIZE_REQUIRED = 2 * (A_SIZE + B_SIZE + C_SIZE);
  if (BUF_SIZE < BUF_SIZE_REQUIRED) {
    printf("Buffer size is too small: %d < %d\n", BUF_SIZE, BUF_SIZE_REQUIRED);
    snrt_cluster_hw_barrier();
    return -1;
  }

  const unsigned int N_CHUNKS = N / N_;  // number of vertical chunks of b
  const unsigned int M_CHUNKS = M / M_;  // number of horizontal chunks of a
  const unsigned int K_CHUNKS = K / K_;  // number of horizontal chunks of b

  if (N % N_ != 0) {
    printf("N is not a multiple of N_: %d %% %d != 0\n", N, N_);
    snrt_cluster_hw_barrier();
    return -1;
  }
  if (M % M_ != 0) {
    printf("M is not a multiple of M_: %d %% %d != 0\n", M, M_);
    snrt_cluster_hw_barrier();
    return -1;
  }
  if (K % K_ != 0) {
    printf("K is not a multiple of K_: %d %% %d != 0\n", K, K_);
    snrt_cluster_hw_barrier();
    return -1;
  }

  double* c_ = c;    // moving DRAM pointer, moving vertically
  double* a_ = a;    // moving DRAM pointer, moving vertically
  double* a__ = a_;  // moving DRAM pointer, moving horizontally
  double* b_ = b;    // moving DRAM pointer, moving vertically

  // Partition L1 buffer, double buffering
  // the pointers for a, c are core id specific to
  // directly access the right part of the buffer per core
  double* ptr = buf;

  double* a_buf1 = ptr + cid * KERNEL_SIZE * K_;
  ptr += A_SIZE;
  double* a_buf2 = ptr + cid * KERNEL_SIZE * K_;
  ptr += A_SIZE;

  double* b_buf1 = ptr;
  ptr += B_SIZE;
  double* b_buf2 = ptr;
  ptr += B_SIZE;

  double* c_buf1 = ptr + cid * KERNEL_SIZE * N_;
  ptr += C_SIZE;
  double* c_buf2 = ptr + cid * KERNEL_SIZE * N_;
  // ptr += C_SIZE;

  snrt_cluster_hw_barrier();

  // snrt_dma_txid_t a_transfer;
  unsigned int timer = 0;

  size_t gvl;

  // fetch first chunk of all
  if (cid == 0) {
    snrt_dma_start_2d(a_buf1,   // destination
                      a__,      // source
                      K_ * TS,  // elements per row
                      K_ * TS,  // destination stride = aligned
                      K * TS,   // source stride = go to next row
                      M_        // rows
    );
    a__ += K_;

    snrt_dma_start_2d(b_buf1,   // destination
                      b_,       // source
                      N_ * TS,  // N_ elements per row
                      N_ * TS,  // destination stride = aligned
                      N * TS,   // source stride = go to next row
                      K_        // K_ rows
    );
    b_ += K_ * N;  // move down to next chunk of b
  }

  asm volatile("vsetvli %[gvl], %[vl], e64, m4, ta, ma"
               : [gvl] "=r"(gvl)
               : [vl] "r"(N_));

  // clean vector registers
  asm volatile("vmv.v.i v0, 0");
  asm volatile("vmv.v.i v4, 0");
  asm volatile("vmv.v.i v8, 0");
  asm volatile("vmv.v.i v12, 0");

  if (cid == 1) {
    printf(" vl = %zu, M_ = %d, N_ = %d, K_ = %d\n", gvl, M_, N_, K_);
  }
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    snrt_dma_wait_all();
    start_kernel();
    timer = benchmark_get_cycle();
  }
  snrt_cluster_hw_barrier();

  // asm volatile("vle64.v v16, (%0);" ::"r"(b_buf1));

  for (int n_chunk_id = N_CHUNKS - 1; n_chunk_id >= 0; n_chunk_id--) {
    // outer column loop

    const double* c_ = c;  // c_ is moving downwards, c is moving horizontally
    c += N_;               // next column chunk of c for next iteration

    for (int m_chunk_id = M_CHUNKS - 1; m_chunk_id >= 0; m_chunk_id--) {
      // outer row loop

      a_ += K * M_;  // move vertically to next row of a
      if (m_chunk_id == 0) {
        b += N_;  // move horizontally to next column of b
        a_ = a;   // reset vertical moving pointer to the beginning of a
      }

      for (int k_chunk_id = K_CHUNKS - 1; k_chunk_id >= 0; k_chunk_id--) {
        // inner chunk loop

        // if (cid == 0) {
        //   snrt_dma_wait(a_transfer);
        // }
        snrt_cluster_hw_barrier();  // keep one barrier in loop to sync cores

        // preload
        asm volatile("vle64.v v16, (%0);" ::"r"(b_buf1));

        if (cid == 0) {
          // start fetching next a and b
          if (1) {  // do not check if not last element, much faster this way

            if (k_chunk_id == 0) {
              // will be down with K_CHUNKS
              b_ = b;    // jump to start of next column of b
              a__ = a_;  // jump to start of next row of a
            }

            // a_transfer =
            snrt_dma_start_2d(a_buf2,   // dest
                              a__,      // src
                              K_ * TS,  // K_ elements per row
                              K_ * TS,  // destination stride = aligned
                              K * TS,   // source stride = go to next row
                              M_        // M_ rows
            );
            a__ += K_;  // move to the right to next chunk of a

            snrt_dma_start_2d(b_buf2,   // dest
                              b_,       // src
                              N_ * TS,  // N_ elements per row
                              N_ * TS,  // destination stride = aligned
                              N * TS,   // source stride = go to next row
                              K_        // K_ rows
            );
            b_ += K_ * N;  // move down to next chunk of b
          }
        }

        // actual calculation of (M_ * K_) x (K_ * N_) sub matrix
        double* a_buf_ = a_buf1;       // this pointer is moving horizontally
        double* a_buf__ = a_buf_;      // this pointer is moving vertically
        double* b_buf_ = b_buf1 + N_;  // this pointer is moving line by line

        // preload first elements of a
        double t0, t1, t2, t3;
        t0 = *a_buf__;
        a_buf__ += K_;
        t1 = *a_buf__;
        a_buf__ += K_;
        t2 = *a_buf__;
        a_buf__ += K_;
        t3 = *a_buf__;

        // already preloaded first row vector of b

        // unrolled loop for prefetching vectors
        for (int k = K_ - 2; k >= 0; k -= 2) {
          asm volatile("vle64.v v24, (%0);" ::"r"(b_buf_));  // preload
          b_buf_ += N_;

          a_buf_ += 1;       // move one col to the right
          a_buf__ = a_buf_;  // reset vertical moving pointer

          asm volatile("vfmacc.vf v0, %0, v16" ::"f"(t0));
          t0 = *a_buf__;
          asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t1));
          a_buf__ += K_;
          t1 = *a_buf__;
          asm volatile("vfmacc.vf v8, %0, v16" ::"f"(t2));
          a_buf__ += K_;
          t2 = *a_buf__;
          asm volatile("vfmacc.vf v12, %0, v16" ::"f"(t3));
          a_buf__ += K_;
          t3 = *a_buf__;

          a_buf_ += 1;
          a_buf__ = a_buf_;

          asm volatile("vle64.v v16, (%0);" ::"r"(b_buf_));  // preload
          b_buf_ += N_;

          asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t0));
          t0 = *a_buf__;
          asm volatile("vfmacc.vf v4, %0, v24" ::"f"(t1));
          a_buf__ += K_;
          t1 = *a_buf__;
          asm volatile("vfmacc.vf v8, %0, v24" ::"f"(t2));
          a_buf__ += K_;
          t2 = *a_buf__;
          asm volatile("vfmacc.vf v12, %0, v24" ::"f"(t3));
          a_buf__ += K_;
          t3 = *a_buf__;
        }  // k loop end

        // swap buffers for b and a
        double* tempb = b_buf1;
        b_buf1 = b_buf2;
        b_buf2 = tempb;

        double* tempa = a_buf1;
        a_buf1 = a_buf2;
        a_buf2 = tempa;

      }  // k chunk loop end

      const double* c_buf__ = c_buf1;  // this pointer is moving line by line

      // store vector result into L1
      asm volatile("vse64.v v0, (%0);" ::"r"(c_buf__));
      c_buf__ += N_;
      asm volatile("vse64.v v4, (%0);" ::"r"(c_buf__));
      c_buf__ += N_;
      asm volatile("vse64.v v8, (%0);" ::"r"(c_buf__));
      c_buf__ += N_;
      asm volatile("vse64.v v12, (%0);" ::"r"(c_buf__));

      // clean vector registers
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v4, 0");
      asm volatile("vmv.v.i v8, 0");
      asm volatile("vmv.v.i v12, 0");

      // snrt_cluster_hw_barrier(); // do we need to wait here?

      // post result to DRAM
      if (cid == 0) {
        snrt_dma_start_2d(c_,       // dst
                          c_buf1,   // src
                          N_ * TS,  // N_ elements per row
                          N * TS,   // destination stride = next row
                          N_ * TS,  // source stride = aligned
                          M_        // M_ rows
        );
        c_ += M_ * N;  // move one chunk down
      }

      // swap buffers for c
      double* tempc = c_buf1;
      c_buf1 = c_buf2;
      c_buf2 = tempc;

    }  // m chunk loop end

  }  // n chunk loop end

  if (cid == 0) {
    timer = benchmark_get_cycle() - timer;
    stop_kernel();
    snrt_dma_wait_all();
  }
  snrt_cluster_hw_barrier();
  return timer;
}
