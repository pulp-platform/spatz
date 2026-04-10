// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <snrt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/rs_spmm.h"

#if (PREC != 64)
#error "rs_spmm currently supports double precision only"
#endif

#define T double

#ifndef RS_SPMM_NUM_CORES
#define RS_SPMM_NUM_CORES 0
#endif

#if defined(__clang__)
#define RS_SPMM_PRAGMA(X) _Pragma(#X)
#define RS_SPMM_NO_UNROLL RS_SPMM_PRAGMA(clang loop unroll(disable))
#else
#define RS_SPMM_NO_UNROLL
#endif

#define RS_SPMM_MIN(a, b) ((a) < (b) ? (a) : (b))

// Shared SPM pointers on separate cache lines.
static T *b_mat __attribute__((section(".data"), aligned(64)));
static T *result_spm __attribute__((section(".data"), aligned(64)));
static uint32_t *nz_idx __attribute__((section(".data"), aligned(64)));

static T result_cache[sizeof(rs_spmm_result) / sizeof(rs_spmm_result[0])]
    __attribute__((section(".data"), aligned(8)));

static T *a_buf[2];
static volatile int rs_spmm_setup_error
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
      (RS_SPMM_NUM_CORES > 0 && RS_SPMM_NUM_CORES < num_cores_hw)
          ? RS_SPMM_NUM_CORES
          : num_cores_hw;
#if USE_CACHE == 0
  const uint32_t num_cores_spm =
      (RS_SPMM_NUM_CORES > 0 && RS_SPMM_NUM_CORES < num_cores_hw)
          ? RS_SPMM_NUM_CORES
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
  if (cid == 0) rs_spmm_setup_error = 0;

#if USE_CACHE == 1
  // Cache mode: distribute nz_rows across cores.
  const uint32_t nz_start =
      (cid < num_cores_cache)
          ? (rs_spmm_l.nz_rows * cid) / num_cores_cache
          : 0;
  const uint32_t nz_end =
      (cid < num_cores_cache)
          ? (rs_spmm_l.nz_rows * (cid + 1)) / num_cores_cache
          : 0;
#else
  // SPM mode: compute tile sizing for double-buffered A rows.
  const size_t resident_bytes =
      (size_t)rs_spmm_l.nz_rows * sizeof(uint32_t) +
      (size_t)rs_spmm_l.K * rs_spmm_l.N * sizeof(T) +
      (size_t)rs_spmm_l.M * rs_spmm_l.N * sizeof(T) + 6 * 8;
  const size_t per_row_buffer_bytes = (size_t)rs_spmm_l.K * sizeof(T);
  const size_t total_l1_bytes = (size_t)spm_size * 1024;
  uint32_t rows_per_tile = 0;

  (void)num_cores_cache;

  if (resident_bytes + 2 * per_row_buffer_bytes > total_l1_bytes) {
    if (cid == 0) {
      printf("Error: rs_spmm SPM tile does not fit in L1 (K=%lu, N=%lu).\n",
             (unsigned long)rs_spmm_l.K, (unsigned long)rs_spmm_l.N);
      rs_spmm_setup_error = -1;
    }
  } else {
    rows_per_tile = (uint32_t)((total_l1_bytes - resident_bytes) /
                               (2 * per_row_buffer_bytes));
    if (rows_per_tile > rs_spmm_l.M) rows_per_tile = rs_spmm_l.M;
    if (rows_per_tile < 1) rows_per_tile = 1;
  }

  if (cid == 0) {
    // Allocate resident arrays: nz_row_idx, B, result.
    nz_idx = (uint32_t *)l1alloc_aligned(
        rs_spmm_l.nz_rows * sizeof(uint32_t), 8);
    b_mat = (T *)l1alloc_aligned(
        (size_t)rs_spmm_l.K * rs_spmm_l.N * sizeof(T), 8);
    result_spm = (T *)l1alloc_aligned(
        (size_t)rs_spmm_l.M * rs_spmm_l.N * sizeof(T), 8);

    if (rs_spmm_setup_error == 0) {
      rows_per_tile = (uint32_t)((total_l1_bytes - resident_bytes) /
                                 (2 * per_row_buffer_bytes));
      if (rows_per_tile > rs_spmm_l.M) rows_per_tile = rs_spmm_l.M;
      if (rows_per_tile < 1) rows_per_tile = 1;

      a_buf[0] = (T *)l1alloc_aligned(
          (size_t)rows_per_tile * rs_spmm_l.K * sizeof(T), 8);
      a_buf[1] = (T *)l1alloc_aligned(
          (size_t)rows_per_tile * rs_spmm_l.K * sizeof(T), 8);

      // DMA resident data once: nz_row_idx and B.
      timer_dma = benchmark_get_cycle();
      snrt_dma_start_1d(nz_idx, rs_spmm_nz_row_idx_dram,
                        rs_spmm_l.nz_rows * sizeof(uint32_t));
      snrt_dma_start_1d(
          b_mat, rs_spmm_b_dram,
          (size_t)rs_spmm_l.K * rs_spmm_l.N * sizeof(T));
      snrt_dma_wait_all();
      timer_dma = benchmark_get_cycle() - timer_dma;
    }
  }

  snrt_cluster_hw_barrier();

  if (rs_spmm_setup_error != 0) {
    ret = rs_spmm_setup_error;
    set_eoc();
    return ret;
  }
#endif

  snrt_cluster_hw_barrier();

#if USE_CACHE == 1
  // Warm-up: touch first non-zero row's A and B data.
  if (cid == 0 && rs_spmm_l.nz_rows > 0) {
    const uint32_t first_row = rs_spmm_nz_row_idx_dram[0];
    volatile double warm =
        rs_spmm_dense_a_dram[(size_t)first_row * rs_spmm_l.K] +
        rs_spmm_b_dram[0];
    (void)warm;
  }
  snrt_cluster_hw_barrier();
#endif

  // Zero-initialize C (only nz rows will be written by kernel).
  if (cid == 0) {
    const size_t total_c = (size_t)rs_spmm_l.M * rs_spmm_l.N;
#if USE_CACHE == 1
    for (size_t i = 0; i < total_c; ++i) result_cache[i] = 0.0;
#else
    for (size_t i = 0; i < total_c; ++i) result_spm[i] = 0.0;
#endif
  }
  snrt_cluster_hw_barrier();

  RS_SPMM_NO_UNROLL
  for (int iter = 0; iter < measure_iter; ++iter) {
    if (cid == 0) {
      start_kernel();
      timer = benchmark_get_cycle();
    }

#if USE_CACHE == 1
    rs_spmm_v64b(rs_spmm_nz_row_idx_dram, rs_spmm_dense_a_dram,
                  rs_spmm_l.K, rs_spmm_b_dram, rs_spmm_l.N, result_cache,
                  rs_spmm_l.N, nz_start, nz_end);
#else
    {
      uint32_t cur = 0;
      uint32_t tile_row = 0;

      // Prefetch first A tile into buf[0].
      if (cid == 0) {
        const uint32_t first_rows =
            RS_SPMM_MIN(rows_per_tile, rs_spmm_l.M);
        snrt_dma_start_1d(a_buf[0], rs_spmm_dense_a_dram,
                          (size_t)first_rows * rs_spmm_l.K * sizeof(T));
      }

      do {
        const uint32_t tile_rs = tile_row;
        const uint32_t cur_rows =
            RS_SPMM_MIN(rows_per_tile, rs_spmm_l.M - tile_rs);
        const uint32_t tile_re = tile_rs + cur_rows;

        if (cid == 0) {
          snrt_dma_wait_all();
        }

        tile_row = tile_re;
        snrt_cluster_hw_barrier();

        // Prefetch next tile.
        if (cid == 0 && tile_row < rs_spmm_l.M) {
          const uint32_t nxt = 1 - cur;
          const uint32_t next_rows =
              RS_SPMM_MIN(rows_per_tile, rs_spmm_l.M - tile_row);
          snrt_dma_start_1d(
              a_buf[nxt],
              rs_spmm_dense_a_dram + (size_t)tile_row * rs_spmm_l.K,
              (size_t)next_rows * rs_spmm_l.K * sizeof(T));
        }

        // Find which nz_row_idx entries fall in [tile_rs, tile_re).
        // nz_row_idx is sorted, so use linear scan from where we left off.
        if (is_spm_worker) {
          // Find range of nz indices whose row is in [tile_rs, tile_re).
          uint32_t nz_lo = 0;
          while (nz_lo < rs_spmm_l.nz_rows && nz_idx[nz_lo] < tile_rs)
            ++nz_lo;
          uint32_t nz_hi = nz_lo;
          while (nz_hi < rs_spmm_l.nz_rows && nz_idx[nz_hi] < tile_re)
            ++nz_hi;

          // Distribute tile's nz_rows across cores.
          const uint32_t tile_nz = nz_hi - nz_lo;
          const uint32_t core_nz_start =
              nz_lo + (tile_nz * cid) / num_cores_spm;
          const uint32_t core_nz_end =
              nz_lo + (tile_nz * (cid + 1)) / num_cores_spm;

          // The A buffer holds rows [tile_rs, tile_re).
          // For a nz_row_idx[i] = row, the row in the buffer is at offset
          // (row - tile_rs) * K.
          rs_spmm_v64b(nz_idx, a_buf[cur] - (size_t)tile_rs * rs_spmm_l.K,
                        rs_spmm_l.K, b_mat, rs_spmm_l.N, result_spm,
                        rs_spmm_l.N, core_nz_start, core_nz_end);
        }

        snrt_cluster_hw_barrier();
        cur = 1 - cur;
      } while (tile_row < rs_spmm_l.M);
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
    const size_t total = (size_t)rs_spmm_l.M * rs_spmm_l.N;

    for (size_t idx = 0; idx < total; ++idx) {
      const double got =
#if USE_CACHE == 1
          result_cache[idx];
#else
          result_spm[idx];
#endif
      checksum += got;
      if (fp_check(&got, &rs_spmm_result[idx])) {
        ++errors;
        if (errors <= 16) {
          const uint32_t row = (uint32_t)(idx / rs_spmm_l.N);
          const uint32_t col = (uint32_t)(idx % rs_spmm_l.N);
          printf("Error: C[%lu,%lu] mismatch\n", (unsigned long)row,
                 (unsigned long)col, got, rs_spmm_result[idx]);
        }
      }
    }

    if (abs_diff(checksum, rs_spmm_checksum) > 0.001) {
      ++errors;
      printf("Error: checksum mismatch\n");
    }

    write_cyc(timer_best);

    {
      const unsigned long performance =
          (timer_best > 0)
              ? (1000UL * 2UL * rs_spmm_l.nz_rows * rs_spmm_l.K *
                 rs_spmm_l.N / timer_best)
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

      printf("\n----- row-sparse SpMM (%lu x %lu) * (%lu x %lu), "
             "nz_rows=%lu -----\n",
             (unsigned long)rs_spmm_l.M, (unsigned long)rs_spmm_l.K,
             (unsigned long)rs_spmm_l.K, (unsigned long)rs_spmm_l.N,
             (unsigned long)rs_spmm_l.nz_rows);
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
      printf("The best execution took %lu cycles.\n",
             (unsigned long)timer_best);
      if (timer_dma > 0) {
        printf("Total (resident DMA + best kernel) took %lu cycles.\n",
               (unsigned long)(timer_dma + timer_best));
      }
      printf("Checksum: %ld.%06ld\n", (long)checksum,
             (long)((checksum - (long)checksum) * 1000000));
      printf("The performance is %lu OP/1000cycle (%lu%%o utilization).\n",
             performance, utilization);
    }

    if (errors) ret = -1;
  }

  snrt_cluster_hw_barrier();
  set_eoc();
  return ret;
}
