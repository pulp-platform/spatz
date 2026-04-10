// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <snrt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/spmm_sparse.h"

#if (PREC != 64)
#error "spmm_sparse currently supports double precision only"
#endif

#define T double

#ifndef SPMM_SPARSE_NUM_CORES
#define SPMM_SPARSE_NUM_CORES 0
#endif

#if defined(__clang__)
#define SPMM_SPARSE_PRAGMA(X) _Pragma(#X)
#define SPMM_SPARSE_NO_UNROLL SPMM_SPARSE_PRAGMA(clang loop unroll(disable))
#else
#define SPMM_SPARSE_NO_UNROLL
#endif

#define SPMM_SPARSE_MIN(a, b) ((a) < (b) ? (a) : (b))

// Shared SPM pointers are kept on separate cache lines so runtime lock traffic
// cannot clobber adjacent words in cache mode.
static T *b_mat __attribute__((section(".data"), aligned(64)));
static T *result __attribute__((section(".data"), aligned(64)));
static uint32_t *row_ptr __attribute__((section(".data"), aligned(64)));
static uint32_t *col_idx __attribute__((section(".data"), aligned(64)));

static T result_cache[sizeof(spmm_sparse_result) / sizeof(spmm_sparse_result[0])]
    __attribute__((section(".data"), aligned(8)));

static T *dense_buf[2];
static uint32_t *col_buf[2];
static volatile int spmm_sparse_setup_error
    __attribute__((section(".data"), aligned(8)));

static inline void *l1alloc_aligned(size_t size, size_t alignment) {
  uintptr_t raw = (uintptr_t)snrt_l1alloc(size + alignment - 1);
  uintptr_t aligned = (raw + alignment - 1) & ~(uintptr_t)(alignment - 1);
  return (void *)aligned;
}

static inline double abs_diff(double a, double b) {
  double d = a - b;
  return d < 0.0 ? -d : d;
}

static inline int fp_check(const T *a, const T *b) {
  return abs_diff(*a, *b) > 0.001;
}

int main() {
  const uint32_t num_cores_hw = snrt_cluster_core_num();
  const uint32_t cid = snrt_cluster_core_idx();
  const uint32_t num_cores_cache =
      (SPMM_SPARSE_NUM_CORES > 0 && SPMM_SPARSE_NUM_CORES < num_cores_hw)
          ? SPMM_SPARSE_NUM_CORES
          : num_cores_hw;
#if USE_CACHE == 0
  const uint32_t num_cores_spm =
      (SPMM_SPARSE_NUM_CORES > 0 && SPMM_SPARSE_NUM_CORES < num_cores_hw)
          ? SPMM_SPARSE_NUM_CORES
          : num_cores_hw;
  const int is_spm_worker = (cid < num_cores_spm);
#endif

#if USE_CACHE == 1
  uint32_t spm_size = 16;
#else
  uint32_t spm_size = 120;
#endif

  const uint32_t num_fpu = 4;

  if (cid == 0) {
    l1d_init(spm_size);
  }
  snrt_cluster_hw_barrier();

#if MEAS_1ITER == 1
  volatile int measure_iter = 1;
#else
  volatile int measure_iter = 2;
#endif

  volatile unsigned int timer = (unsigned int)-1;
  volatile unsigned int timer_best = (unsigned int)-1;
  volatile unsigned int timer_1iter = (unsigned int)-1;
  volatile unsigned int timer_dma = 0;
  int ret = 0;
  const uint32_t *cache_row_ptr = spmm_sparse_row_ptr_dram;
  const uint32_t *cache_col_idx = spmm_sparse_col_idx_dram;
  const T *cache_b_mat = spmm_sparse_b_dram;
  T *cache_result = result_cache;
  if (cid == 0) spmm_sparse_setup_error = 0;

#if USE_CACHE == 1
  const uint32_t row_start =
      (cid < num_cores_cache) ? (spmm_sparse_l.M * cid) / num_cores_cache : 0;
  const uint32_t row_end = (cid < num_cores_cache)
                               ? (spmm_sparse_l.M * (cid + 1)) / num_cores_cache
                               : 0;
#else
  const size_t resident_bytes =
      (size_t)(spmm_sparse_l.M + 1) * sizeof(uint32_t) +
      (size_t)spmm_sparse_l.K * spmm_sparse_l.N * sizeof(T) +
      (size_t)spmm_sparse_l.M * spmm_sparse_l.N * sizeof(T) + 6 * 8;
  const size_t per_row_buffer_bytes =
      (size_t)spmm_sparse_l.K * (sizeof(T) + sizeof(uint32_t));
  const size_t total_l1_bytes = (size_t)spm_size * 1024;
  uint32_t rows_per_tile = 0;
  const uint32_t row_start =
      is_spm_worker ? (spmm_sparse_l.M * cid) / num_cores_spm : 0;
  const uint32_t row_end = is_spm_worker
                               ? (spmm_sparse_l.M * (cid + 1)) / num_cores_spm
                               : 0;
  (void)row_start;
  (void)row_end;
  (void)num_cores_cache;

  if (resident_bytes + 2 * per_row_buffer_bytes > total_l1_bytes) {
    if (cid == 0) {
      printf("Error: spmm_sparse SPM tile does not fit in L1 (K=%lu, N=%lu).\n",
             (unsigned long)spmm_sparse_l.K, (unsigned long)spmm_sparse_l.N);
      spmm_sparse_setup_error = -1;
    }
  } else {
    rows_per_tile = (uint32_t)((total_l1_bytes - resident_bytes) /
                               (2 * per_row_buffer_bytes));
    if (rows_per_tile > spmm_sparse_l.M) rows_per_tile = spmm_sparse_l.M;
    if (rows_per_tile < 1) rows_per_tile = 1;
  }

  if (cid == 0) {
    row_ptr = (uint32_t *)l1alloc_aligned(
        (spmm_sparse_l.M + 1) * sizeof(uint32_t), 8);
    b_mat = (T *)l1alloc_aligned(
        (size_t)spmm_sparse_l.K * spmm_sparse_l.N * sizeof(T), 8);
    result = (T *)l1alloc_aligned(
        (size_t)spmm_sparse_l.M * spmm_sparse_l.N * sizeof(T), 8);

    if (spmm_sparse_setup_error == 0) {
      rows_per_tile = (uint32_t)((total_l1_bytes - resident_bytes) /
                                 (2 * per_row_buffer_bytes));
      if (rows_per_tile > spmm_sparse_l.M) rows_per_tile = spmm_sparse_l.M;
      if (rows_per_tile < 1) rows_per_tile = 1;

      dense_buf[0] = (T *)l1alloc_aligned(
          (size_t)rows_per_tile * spmm_sparse_l.K * sizeof(T), 8);
      col_buf[0] = (uint32_t *)l1alloc_aligned(
          (size_t)rows_per_tile * spmm_sparse_l.K * sizeof(uint32_t), 8);
      dense_buf[1] = (T *)l1alloc_aligned(
          (size_t)rows_per_tile * spmm_sparse_l.K * sizeof(T), 8);
      col_buf[1] = (uint32_t *)l1alloc_aligned(
          (size_t)rows_per_tile * spmm_sparse_l.K * sizeof(uint32_t), 8);

      timer_dma = benchmark_get_cycle();
      snrt_dma_start_1d(row_ptr, spmm_sparse_row_ptr_dram,
                        (spmm_sparse_l.M + 1) * sizeof(uint32_t));
      snrt_dma_start_1d(
          b_mat, spmm_sparse_b_dram,
          (size_t)spmm_sparse_l.K * spmm_sparse_l.N * sizeof(T));
      snrt_dma_wait_all();
      timer_dma = benchmark_get_cycle() - timer_dma;
    }
  }

  snrt_cluster_hw_barrier();

  if (spmm_sparse_setup_error != 0) {
    ret = spmm_sparse_setup_error;
    set_eoc();
    return ret;
  }
#endif

  snrt_cluster_hw_barrier();

#if USE_CACHE == 1
  if (cid == 0 && spmm_sparse_l.nnz > 0) {
    const uint32_t first_col = cache_col_idx[cache_row_ptr[0]];
    volatile double warm = spmm_sparse_dense_a_dram[first_col] +
                           cache_b_mat[(size_t)first_col * spmm_sparse_l.N] +
                           (double)cache_row_ptr[1];
    (void)warm;
  }
  snrt_cluster_hw_barrier();
#endif

  SPMM_SPARSE_NO_UNROLL
  for (int iter = 0; iter < measure_iter; ++iter) {
    if (cid == 0) {
      start_kernel();
      timer = benchmark_get_cycle();
    }

#if USE_CACHE == 1
    spmm_sparse_v64b(cache_row_ptr, cache_col_idx, 0, spmm_sparse_dense_a_dram,
                     spmm_sparse_l.K, 0, cache_b_mat, spmm_sparse_l.N,
                     cache_result, spmm_sparse_l.N, row_start, row_end);
#else
    {
      uint32_t cur = 0;
      uint32_t tile_row = 0;

      if (cid == 0) {
        const uint32_t first_rows =
            SPMM_SPARSE_MIN(rows_per_tile, spmm_sparse_l.M);
        const uint32_t first_re = first_rows;
        const uint32_t first_nnz = row_ptr[first_re] - row_ptr[0];
        snrt_dma_start_1d(dense_buf[0], spmm_sparse_dense_a_dram,
                          (size_t)first_rows * spmm_sparse_l.K * sizeof(T));
        if (first_nnz > 0) {
          snrt_dma_start_1d(col_buf[0], spmm_sparse_col_idx_dram,
                            (size_t)first_nnz * sizeof(uint32_t));
        }
      }

      do {
        const uint32_t tile_rs = tile_row;
        const uint32_t cur_rows =
            SPMM_SPARSE_MIN(rows_per_tile, spmm_sparse_l.M - tile_rs);
        const uint32_t tile_re = tile_rs + cur_rows;
        const uint32_t tile_nnz_start = row_ptr[tile_rs];

        if (cid == 0) {
          snrt_dma_wait_all();
        }

        tile_row = tile_re;
        snrt_cluster_hw_barrier();

        if (cid == 0 && tile_row < spmm_sparse_l.M) {
          const uint32_t nxt = 1 - cur;
          const uint32_t next_rows =
              SPMM_SPARSE_MIN(rows_per_tile, spmm_sparse_l.M - tile_row);
          const uint32_t next_re = tile_row + next_rows;
          const uint32_t next_nnz_start = row_ptr[tile_row];
          const uint32_t next_tile_nnz = row_ptr[next_re] - next_nnz_start;

          snrt_dma_start_1d(
              dense_buf[nxt],
              spmm_sparse_dense_a_dram + (size_t)tile_row * spmm_sparse_l.K,
              (size_t)next_rows * spmm_sparse_l.K * sizeof(T));
          if (next_tile_nnz > 0) {
            snrt_dma_start_1d(
                col_buf[nxt], spmm_sparse_col_idx_dram + next_nnz_start,
                (size_t)next_tile_nnz * sizeof(uint32_t));
          }
        }

        if (is_spm_worker) {
          const uint32_t tile_nrows = tile_re - tile_rs;
          const uint32_t core_rs = tile_rs + (tile_nrows * cid) / num_cores_spm;
          const uint32_t core_re =
              tile_rs + (tile_nrows * (cid + 1)) / num_cores_spm;

          spmm_sparse_v64b(row_ptr, col_buf[cur], tile_nnz_start, dense_buf[cur],
                           spmm_sparse_l.K, tile_rs, b_mat, spmm_sparse_l.N,
                           result, spmm_sparse_l.N, core_rs, core_re);
        }

        snrt_cluster_hw_barrier();
        cur = 1 - cur;
      } while (tile_row < spmm_sparse_l.M);
    }
#endif

    snrt_cluster_hw_barrier();

    if (cid == 0) {
      stop_kernel();
      timer = benchmark_get_cycle() - timer;
      if (iter == 0) {
        timer_1iter = timer;
      }
      timer_best = (timer_best > timer) ? timer : timer_best;
    }

    snrt_cluster_hw_barrier();
  }

  if (cid == 0) {
    double checksum = 0.0;
    int errors = 0;
    const size_t total = (size_t)spmm_sparse_l.M * spmm_sparse_l.N;

    for (size_t idx = 0; idx < total; ++idx) {
      const double got =
#if USE_CACHE == 1
          cache_result[idx];
#else
          result[idx];
#endif
      checksum += got;
      if (fp_check(&got, &spmm_sparse_result[idx])) {
        ++errors;
        if (errors <= 16) {
          const uint32_t row = (uint32_t)(idx / spmm_sparse_l.N);
          const uint32_t col = (uint32_t)(idx % spmm_sparse_l.N);
          printf("Error: C[%lu,%lu]=%f golden=%f\n", (unsigned long)row,
                 (unsigned long)col, got, spmm_sparse_result[idx]);
        }
      }
    }

    if (abs_diff(checksum, spmm_sparse_checksum) > 0.001) {
      ++errors;
      printf("Error: checksum=%f golden=%f\n", checksum,
             spmm_sparse_checksum);
    }

    write_cyc(timer_best);

    {
      const unsigned long performance =
          (timer_best > 0)
              ? (1000UL * 2UL * spmm_sparse_l.M * spmm_sparse_l.N *
                 spmm_sparse_l.nnz / spmm_sparse_l.M / timer_best)
              : 0;
      const unsigned long utilization =
          (timer_best > 0)
              ? performance /
                    (2 *
#if USE_CACHE == 1
                     num_cores_cache
#else
                     num_cores_spm
#endif
                     * num_fpu * 8 / sizeof(T))
              : 0;

      printf("\n----- (%lu x %lu) * (%lu x %lu), nnz=%lu spmm_sparse -----\n",
             (unsigned long)spmm_sparse_l.M, (unsigned long)spmm_sparse_l.K,
             (unsigned long)spmm_sparse_l.K, (unsigned long)spmm_sparse_l.N,
             (unsigned long)spmm_sparse_l.nnz);
      printf("Kernel vector setting: m4 across N columns\n");
      printf("Active cores: %lu / %lu\n",
#if USE_CACHE == 1
             (unsigned long)num_cores_cache,
#else
             (unsigned long)num_cores_spm,
#endif
             (unsigned long)num_cores_hw);
#if USE_CACHE == 0
      printf("Tile: %lu rows/tile\n", (unsigned long)rows_per_tile);
#endif
      printf("DMA (resident) took %lu cycles.\n", (unsigned long)timer_dma);
      printf("The first iter takes %lu cycles.\n", (unsigned long)timer_1iter);
      printf("The best execution took %lu cycles.\n", (unsigned long)timer_best);
      if (timer_dma > 0) {
        printf("Total (resident DMA + best kernel) took %lu cycles.\n",
               (unsigned long)(timer_dma + timer_best));
      }
      printf("Checksum: %f\n", checksum);
      printf("The performance is %lu OP/1000cycle (%lu%%o utilization).\n",
             performance, utilization);
    }

    if (errors) ret = -1;
  }

  snrt_cluster_hw_barrier();
  set_eoc();
  return ret;
}
