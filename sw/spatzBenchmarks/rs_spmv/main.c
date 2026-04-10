// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/rs_spmv.h"

#if (PREC != 64)
#error "rs_spmv currently supports double precision only"
#endif

#define T double

#ifndef RS_SPMV_NUM_CORES
#define RS_SPMV_NUM_CORES 0
#endif

#if defined(__clang__)
#define RS_SPMV_PRAGMA(X) _Pragma(#X)
#define RS_SPMV_NO_UNROLL RS_SPMV_PRAGMA(clang loop unroll(disable))
#else
#define RS_SPMV_NO_UNROLL
#endif

#define RS_SPMV_MIN(a, b) ((a) < (b) ? (a) : (b))

#define RS_SPMV_SPM_CAPACITY (120 * 1024)

// Shared pointers for SPM mode.
static T *x_vec;
static T *result_spm;
static uint32_t *nz_idx_spm;

// Double-buffer pointers for dense_a tiles (SPM mode).
static T *dense_buf[2];

// Compile-time constants derived from generated data arrays.
enum {
  RS_SPMV_COMPILE_M =
      (uint32_t)(sizeof(rs_spmv_result) / sizeof(rs_spmv_result[0])),
};

// Cache-mode result buffer in .data (L2-backed, cacheable).
static T result_cache[RS_SPMV_COMPILE_M]
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
  const uint32_t num_cores =
      (RS_SPMV_NUM_CORES > 0 && RS_SPMV_NUM_CORES < num_cores_hw)
          ? RS_SPMV_NUM_CORES
          : num_cores_hw;

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

  const uint32_t M = rs_spmv_l.M;
  const uint32_t K = rs_spmv_l.K;
  const uint32_t nz_rows = rs_spmv_l.nz_rows;

#if USE_CACHE == 1
  // ---- Cache mode: read all from DRAM directly ----
  // Distribute nz_rows across cores.
  const uint32_t nz_start =
      (cid < num_cores) ? (nz_rows * cid) / num_cores : 0;
  const uint32_t nz_end =
      (cid < num_cores) ? (nz_rows * (cid + 1)) / num_cores : 0;

  const uint32_t *nz_row_idx = rs_spmv_nz_row_idx_dram;
  const T *dense_a = rs_spmv_dense_a_dram;
  const T *x = rs_spmv_x_dram;
  T *result = result_cache;

#else
  // ---- SPM mode: DMA-based tiling ----
  // Resident in L1: x (K*8), nz_row_idx (nz_rows*4), result (M*8)
  // Double-buffered: dense_a tiles (rows_per_tile * K * 8) x2
  const size_t resident_bytes =
      (size_t)K * sizeof(T) + (size_t)nz_rows * sizeof(uint32_t) +
      (size_t)M * sizeof(T) + 6 * 8;
  const size_t per_tile_row_bytes = (size_t)K * sizeof(T);
  const size_t total_l1_bytes = (size_t)spm_size * 1024;
  uint32_t rows_per_tile =
      (uint32_t)((total_l1_bytes - resident_bytes) / (2 * per_tile_row_bytes));
  if (rows_per_tile > M) rows_per_tile = M;
  if (rows_per_tile < 1) rows_per_tile = 1;

  if (cid == 0) {
    // Allocate resident arrays in L1.
    x_vec = (T *)l1alloc_aligned(K * sizeof(T), 8);
    nz_idx_spm = (uint32_t *)l1alloc_aligned(nz_rows * sizeof(uint32_t), 8);
    result_spm = (T *)l1alloc_aligned(M * sizeof(T), 8);

    // Allocate double buffers for dense_a tiles.
    dense_buf[0] =
        (T *)l1alloc_aligned((size_t)rows_per_tile * K * sizeof(T), 8);
    dense_buf[1] =
        (T *)l1alloc_aligned((size_t)rows_per_tile * K * sizeof(T), 8);

    // DMA resident data once.
    timer_dma = benchmark_get_cycle();
    snrt_dma_start_1d(x_vec, rs_spmv_x_dram, K * sizeof(T));
    snrt_dma_start_1d(nz_idx_spm, rs_spmv_nz_row_idx_dram,
                      nz_rows * sizeof(uint32_t));
    snrt_dma_wait_all();
    timer_dma = benchmark_get_cycle() - timer_dma;

    // Zero the result buffer.
    for (uint32_t i = 0; i < M; ++i) result_spm[i] = 0.0;
  }
#endif

  snrt_cluster_hw_barrier();

#if USE_CACHE == 1
  // Warm-up touch in cache mode: read first non-zero row's input lines once
  // before entering the vector kernel.
  if (cid == 0 && nz_rows > 0) {
    const uint32_t first_row = nz_row_idx[0];
    volatile double warm =
        dense_a[(size_t)first_row * K] + x[0];
    (void)warm;
  }
  snrt_cluster_hw_barrier();
#endif

  // Zero-initialize y (only nz rows will be written by kernel).
  if (cid == 0) {
#if USE_CACHE == 1
    for (uint32_t i = 0; i < rs_spmv_l.M; ++i) result_cache[i] = 0.0;
#else
    for (uint32_t i = 0; i < rs_spmv_l.M; ++i) result_spm[i] = 0.0;
#endif
  }
  snrt_cluster_hw_barrier();

  RS_SPMV_NO_UNROLL
  for (int iter = 0; iter < measure_iter; ++iter) {
    if (cid == 0) {
      start_kernel();
      timer = benchmark_get_cycle();
    }

#if USE_CACHE == 1
    rs_spmv_v64b(nz_row_idx, dense_a, K, x, result, nz_start, nz_end);
#else
    // ---- Double-buffered tiled row-sparse SpMV ----
    // Tile over rows of dense_a. For each tile [tile_rs, tile_re), DMA the
    // full dense block A[tile_rs*K : tile_re*K] (including zero rows), then
    // find which nz_row_idx entries fall in [tile_rs, tile_re) and compute
    // only those.
    {
      uint32_t cur = 0;
      uint32_t tile_row = 0;

      // Prefetch first tile into buf[0].
      if (cid == 0) {
        uint32_t first_rows = RS_SPMV_MIN(rows_per_tile, M);
        snrt_dma_start_1d(dense_buf[0], rs_spmv_dense_a_dram,
                          (size_t)first_rows * K * sizeof(T));
      }

      do {
        // Current tile bounds.
        const uint32_t tile_rs = tile_row;
        const uint32_t cur_rows = RS_SPMV_MIN(rows_per_tile, M - tile_rs);
        const uint32_t tile_re = tile_rs + cur_rows;

        // Wait for current tile DMA.
        if (cid == 0) {
          snrt_dma_wait_all();
        }

        tile_row = tile_re;

        snrt_cluster_hw_barrier();

        // Start DMA for next tile (overlaps with compute below).
        if (cid == 0 && tile_row < M) {
          const uint32_t nxt = 1 - cur;
          const uint32_t next_rows = RS_SPMV_MIN(rows_per_tile, M - tile_row);
          snrt_dma_start_1d(
              dense_buf[nxt],
              rs_spmv_dense_a_dram + (size_t)tile_row * K,
              (size_t)next_rows * K * sizeof(T));
        }

        // Find which nz_row_idx entries fall in [tile_rs, tile_re).
        // Linear scan (nz_row_idx is sorted).
        uint32_t nz_lo = 0;
        while (nz_lo < nz_rows && nz_idx_spm[nz_lo] < tile_rs) ++nz_lo;
        uint32_t nz_hi = nz_lo;
        while (nz_hi < nz_rows && nz_idx_spm[nz_hi] < tile_re) ++nz_hi;

        // Distribute nz entries in this tile across cores.
        const uint32_t tile_nz = nz_hi - nz_lo;
        uint32_t core_start, core_end;
        if (cid < num_cores && tile_nz > 0) {
          core_start = nz_lo + (tile_nz * cid) / num_cores;
          core_end = nz_lo + (tile_nz * (cid + 1)) / num_cores;
        } else {
          core_start = core_end = nz_lo;
        }

        // Compute: dense_buf[cur] holds rows [tile_rs, tile_re).
        // Shift buffer pointer back by tile_rs*K so that absolute row
        // indices in nz_row_idx map into the tile-local buffer.
        rs_spmv_v64b(nz_idx_spm, dense_buf[cur] - (size_t)tile_rs * K, K,
                     x_vec, result_spm, core_start, core_end);

        snrt_cluster_hw_barrier();
        cur = 1 - cur;
      } while (tile_row < M);
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

#if USE_CACHE == 1
    const T *res = result;
#else
    const T *res = result_spm;
#endif

    for (uint32_t i = 0; i < M; ++i) {
      checksum += res[i];
      if (fp_check(&res[i], &rs_spmv_result[i])) {
        ++errors;
        printf("Error: row %lu result=%f golden=%f\n", (unsigned long)i,
               res[i], rs_spmv_result[i]);
      }
    }

    if (abs_diff(checksum, rs_spmv_checksum) > 0.001) {
      ++errors;
      printf("Error: checksum=%f golden=%f\n", checksum, rs_spmv_checksum);
    }

    write_cyc(timer_best);

    {
      const unsigned long performance =
          (timer_best > 0)
              ? (1000UL * 2UL * (unsigned long)nz_rows * K / timer_best)
              : 0;
      const unsigned long utilization =
          (timer_best > 0)
              ? performance /
                    (2 * num_cores * num_fpu * 8 / sizeof(T))
              : 0;

      printf("\n----- (%lu x %lu, nz_rows=%lu) row-sparse SpMV -----\n",
             (unsigned long)M, (unsigned long)K, (unsigned long)nz_rows);
      printf("Active cores: %lu / %lu\n", (unsigned long)num_cores,
             (unsigned long)num_cores_hw);
#if USE_CACHE == 0
      printf("Tile: %lu rows/tile, %lu tiles\n",
             (unsigned long)rows_per_tile,
             (unsigned long)((M + rows_per_tile - 1) / rows_per_tile));
#endif
      printf("DMA (resident) took %lu cycles.\n", (unsigned long)timer_dma);
      printf("The first iter takes %lu cycles.\n",
             (unsigned long)timer_1iter);
      printf("The best execution took %lu cycles.\n",
             (unsigned long)timer_best);
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
