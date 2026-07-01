// Copyright 2025 ETH Zurich and University of Bologna.
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

// Author: Diyou Shen,              ETH Zurich <dishen@iis.ee.ethz.ch>
// Author: Navaneeth Kunhi Purayil, ETH Zurich <nkunhi@iis.ee.ethz.ch>

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/sa-gemv.c"

#if (PREC == 64)
#define T double
#elif (PREC == 32)
#define T float
#elif (PREC == 16)
#define T __fp16
#else
#define T double
#endif

// Debugging defines
// #define DEBUG_NZ
// #define DEBUG_NZ_IDX
// #define DEBUG_GEMV_PreLD
// #define DEBUG_GEMV_DB

T *vec_buf0;
T *vec_buf1;
uint16_t *dense_idx;
T *dense_vec;
T *mat_buf0;
T *mat_buf1;
T *result;

static inline int fp_check(const T *a, const T *b) {
#if (PREC == 64)
  const float threshold = 0.001f;
#elif (PREC == 32)
  const float threshold = 0.01f;
#else
  const float threshold = 0.5f;
#endif

  float comp = (float)*a - (float)*b;

  // Compare via the raw bit pattern with the sign bit cleared instead of a
  // floating-point subtraction/compare against `threshold`. This is called
  // from inside the result-checking loop right before a conditional PRINTF,
  // and keeping `threshold` as a live float across that call pushes the
  // compiler to spill it through a callee-saved fs* register (fsd/fld),
  // which traps as an illegal instruction on hardware built without the D
  // extension. IEEE-754 positive floats compare the same way as unsigned
  // integers of their bit pattern, so this avoids needing a persistent FP
  // register altogether.
  union {
    float f;
    uint32_t u;
  } conv, thr;
  conv.f = comp;
  thr.f = threshold;

  return (conv.u & 0x7FFFFFFFu) > thr.u;
}

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;
  unsigned int timer_best = (unsigned int)-1;
  unsigned int timer_nz = (unsigned int)-1;
  const unsigned int m_core = gemv_l.M / num_cores;
  // Size (in KiB) of L1 SPM, used to calculate tiling window
  const unsigned int spm_size = 128;

  // For Sparse Attention GEMV, we need several steps
  // 1. Find all non-zeros
  // 2. Calculate the GEMV
  // What to be double buffered?
  // 1. 2 chunks of sparse vector + densed vector (output) + densed idx
  // 2. 2 chunks of matrix + densed vector (output) + densed idx

  // Sizes of each part we need
  const uint32_t mat_size = sizeof(T) * gemv_l.M * tot_nz_dram;
  const uint32_t row_size = sizeof(T) * gemv_l.M;
  const uint32_t vec_size = sizeof(T) * gemv_l.N;
  const uint32_t dense_vec_size = sizeof(T) * tot_nz_dram;
  const uint32_t dense_idx_size = sizeof(uint16_t) * tot_nz_dram;
  const uint32_t result_size = sizeof(T) * gemv_l.M;

  // leave 8 KiB for Stack
  const uint32_t l1_size = (spm_size - 8) * 1024;
  const uint32_t fixed_alloc_size =
      dense_vec_size + dense_idx_size + result_size;

  // --- BOUNDS CHECK 1: Do the fixed arrays fit in L1? ---
  if (fixed_alloc_size >= l1_size) {
    if (cid == 0) {
      PRINTF("FATAL: L1 Memory Overflow! Fixed arrays require %u bytes, but "
             "only %u bytes available.\n",
             fixed_alloc_size, l1_size);
    }
    snrt_cluster_hw_barrier();
    return -1; // Exit gracefully
  }

  const uint32_t l1_for_chunk = l1_size - fixed_alloc_size;

  // --- BOUNDS CHECK 2: Does the full sparse vector fit in L1 at once? ---
  // Non-zero finding no longer double buffers the vector, so it must be
  // loaded in a single chunk.
  if (vec_size > l1_for_chunk) {
    if (cid == 0) {
      PRINTF("FATAL: L1 Memory Overflow! Vector requires %u bytes, but only "
             "%u bytes available for non-zero finding.\n",
             vec_size, l1_for_chunk);
    }
    snrt_cluster_hw_barrier();
    return -1; // Exit gracefully
  }

  // How many whole rows (or columns) can fit in half the L1 chunk space?
  const uint32_t num_row_mat = (l1_for_chunk / 2) / row_size;

  // --- BOUNDS CHECK 3: Can we double buffer at least 1 row? ---
  if (num_row_mat < 1) {
    if (cid == 0) {
      PRINTF("FATAL: L1 Memory Overflow! Cannot fit at least 2 rows for double "
             "buffering. "
             "Chunk space left: %u bytes, Row size: %u bytes.\n",
             l1_for_chunk, row_size);
    }
    snrt_cluster_hw_barrier();
    return -1; // Exit gracefully
  }

  // Split the available memory in half for matrix double-buffering
  const uint32_t vec_chunk_size = l1_for_chunk / 2;

  // Recalculate exact chunk size based on whole rows
  const uint32_t mat_chunk_size = num_row_mat * row_size;

  // Number of chunks based on the total non-zeros we need to process
  const uint32_t num_mat_chunk = (tot_nz_dram + num_row_mat - 1) / num_row_mat;

  // Number of elements in each chunk
  const uint32_t vec_chunk_len = vec_chunk_size / sizeof(T);

  // Memory Allocation
  if (cid == 0) {
    result = (T *)snrt_l1alloc(result_size);
    dense_vec = (T *)snrt_l1alloc(dense_vec_size);
    vec_buf0 = (T *)snrt_l1alloc(l1_for_chunk);
    dense_idx = (uint16_t *)snrt_l1alloc(dense_idx_size);

    // Offset by half of the size if needed by double buffering
    vec_buf1 = vec_buf0 + vec_chunk_len;

    mat_buf0 = vec_buf0;
    mat_buf1 = vec_buf1;
  }

  // MUST zero out the memory accumulator!
  if (cid == 0) {
    for (unsigned int i = 0; i < gemv_l.M; i++) {
      result[i] = 0.0;
    }
  }
  snrt_cluster_hw_barrier();

  if (cid == 0)
    start_kernel();

  timer = benchmark_get_cycle();

  // Task 1: Find out the non-zeros
  uint32_t nz_count = 0;

  if (cid == 0) {
#ifdef DEBUG_NZ
    PRINTF("NZ-Calc Load\n");
    PRINTF("DMA SRC:%p, TGT:%p, SIZE:%u\n", gemv_vec_dram, vec_buf0, vec_size);
#endif
    snrt_dma_start_1d(vec_buf0, gemv_vec_dram, vec_size);
    snrt_dma_wait_all();

    // Non-zero entries are guaranteed by data generation to have magnitude
    // >= NZ_THRESHOLD, while masked-out entries are exactly 0.0.
    const float nz_threshold = 0.1f;
    for (unsigned int j = 0; j < gemv_l.N; ++j) {
      float val = (float)vec_buf0[j];
      if (val > nz_threshold || val < -nz_threshold) {
        dense_vec[nz_count] = vec_buf0[j];
        dense_idx[nz_count] = j;
        nz_count++;
      }

      if (nz_count == tot_nz_dram)
        break;
    }
  }

  snrt_cluster_hw_barrier();

#ifdef DEBUG_NZ
  if (cid == 0)
    PRINTF("Non-Zero Calc Complete\n");
#endif

#ifdef DEBUG_NZ_IDX
  if (cid == 0) {
    for (uint32_t i = 0; i < tot_nz_dram; i++) {
      PRINTF("IDX[%u]=%u\n", i, dense_idx[i]);
    }
  }
#endif

  timer_nz = benchmark_get_cycle() - timer_nz;
  timer = benchmark_get_cycle();

  // Task 2: GEMV calculation
  // Calculate internal pointers
  T *mat_ptr = mat_buf0;
  T *mat_db_ptr = mat_buf1;
  T *result_core = result + m_core * cid;
  uint16_t *idx_ptr = dense_idx; // Corrected pointer type

  if (cid == 0) {
    // Determine how many rows are actually active for this very first chunk
    uint32_t active_rows =
        (tot_nz_dram < num_row_mat) ? tot_nz_dram : num_row_mat;

#ifdef DEBUG_GEMV_PreLD
    PRINTF("GEMV PreLD\n");
    PRINTF("Active Rows:%u\n", active_rows);
#endif

    for (unsigned int i = 0; i < active_rows; i++) {
#ifdef DEBUG
      PRINTF("Row:%u, SRC:%p, TGT:%p, SIZE:%u\n", i,
             gemv_mat_dram + (size_t)(*idx_ptr) * gemv_l.M,
             mat_ptr + i * gemv_l.M, row_size);
#endif
      snrt_dma_start_1d(mat_ptr + i * gemv_l.M, // Pack linearly into L1
                        gemv_mat_dram +
                            (size_t)(*idx_ptr) * gemv_l.M, // Source from DRAM
                        row_size);
      idx_ptr++;
    }
  }
  // error accumulator for checking
  int error = 0;

  snrt_cluster_hw_barrier();

#ifdef DEBUG_GEMV_PreLD
  if (cid == 0)
    PRINTF("GEMV PreLD Complete\n");
#endif

#ifdef DEBUG_GEMV_DB
  if (cid == 0)
    PRINTF("Tot Chunks %u\n", num_mat_chunk);
#endif

  for (unsigned int chunk_idx = 0; chunk_idx < num_mat_chunk; chunk_idx++) {
    // Wait for the CURRENT chunk to finish loading
    if (cid == 0) {
      snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();

    // Determine bounds for the NEXT chunk (for background DMA)
    uint32_t next_chunk_start = (chunk_idx + 1) * num_row_mat;
    uint32_t next_active_rows = 0;

    if (next_chunk_start < tot_nz_dram) {
      next_active_rows = (tot_nz_dram - next_chunk_start < num_row_mat)
                             ? (tot_nz_dram - next_chunk_start)
                             : num_row_mat;
    }

#ifdef DEBUG_GEMV_DB
    if (cid == 0)
      PRINTF("Chunk%u, DB Rows%u\n", chunk_idx, next_active_rows);
#endif

    // Load NEXT chunk in the background
    if (cid == 0 && next_active_rows > 0) {
      for (unsigned int i = 0; i < next_active_rows; i++) {
#ifdef DEBUG_GEMV_DB
        PRINTF("Ptr:%p, Row:%u, SRC:%p, TGT:%p, SIZE:%u\n", idx_ptr, i,
               gemv_mat_dram + (size_t)(*idx_ptr) * gemv_l.M,
               mat_db_ptr + i * gemv_l.M, row_size);
#endif
        snrt_dma_start_1d(mat_db_ptr + i * gemv_l.M,
                          gemv_mat_dram + (size_t)(*idx_ptr) * gemv_l.M,
                          row_size);
        idx_ptr++;
      }
    }

    // Calculate active rows for the CURRENT compute phase
    uint32_t curr_active_rows =
        (tot_nz_dram - chunk_idx * num_row_mat < num_row_mat)
            ? (tot_nz_dram - chunk_idx * num_row_mat)
            : num_row_mat;

    // Calculate GEMV on the current chunk
    T *current_dense_vec = dense_vec + chunk_idx * num_row_mat;

    // Offset the matrix pointer by m_core * cid so each core reads its correct
    // rows
    T *mat_core_ptr = mat_ptr + m_core * cid;

#if (PREC == 64)
    gemv_v64b_m4(mat_core_ptr, current_dense_vec, result_core, gemv_l.M, m_core,
                 curr_active_rows);
#elif (PREC == 32)
    gemv_v32b_m4(mat_core_ptr, current_dense_vec, result_core, gemv_l.M, m_core,
                 curr_active_rows);
#else
    gemv_v16b_m4(mat_core_ptr, current_dense_vec, result_core, gemv_l.M, m_core,
                 curr_active_rows);
#endif

    // Swap pointers for the next iteration
    T *temp = mat_ptr;
    mat_ptr = mat_db_ptr;
    mat_db_ptr = temp;
  }

  snrt_cluster_hw_barrier();

  timer = benchmark_get_cycle() - timer;

  if (cid == 0)
    stop_kernel();

  // Result Checking
  if (cid == 0) {
    // Checking
    for (unsigned int i = 0; i < gemv_l.M; i++) {
      if (fp_check(&result[i], &gemv_result[i])) {
#if (PREC == 64)
        PRINTF("Error: ID: %i Result = %f, Golden = %f\n", i, result[i],
               gemv_result[i]);
#elif (PREC == 32)
        PRINTF("Error: ID: %i Result = %x, Golden = %x\n", i,
               *(int *)&result[i], *(int *)&gemv_result[i]);
#else
        PRINTF("Error: ID: %i Result = %x, Golden = %x\n", i,
               *(uint16_t *)&result[i], *(uint16_t *)&gemv_result[i]);
#endif
        error++;
      }
    }
  }

  snrt_cluster_hw_barrier();

  // Check and display results
  // Assume 2 core 4 fpu configuration
  if (cid == 0) {
    // Flops per cycle
    long unsigned int performance = 1000 * 2 * gemv_l.M * tot_nz_dram / timer;
    // Ideal perf = MACC * NCore * Nfpu * Prec adjustment
    long unsigned int utilization =
        performance / (2 * num_cores * 4 * 8 / sizeof(T));

    PRINTF("\n----- (%d x %d) x (%d x 1) sa-gemv -----\n", gemv_l.M, gemv_l.N,
           tot_nz_dram);
    PRINTF("The NZ finding takes %u cycles.\n", timer_nz);
    PRINTF("The GEMV execution took %u cycles.\n", timer);
    PRINTF("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
           performance, utilization);
  }

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();
  if (error > 0)
    return -1;
  else
    return 0;
}
