// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/spmv_sparse.h"

#if (PREC != 64)
#error "spmv_sparse currently supports double precision only"
#endif

#define T double

#ifndef SPMV_SPARSE_NUM_CORES
#define SPMV_SPARSE_NUM_CORES 0
#endif

#if defined(__clang__)
#define SPMV_SPARSE_PRAGMA(X) _Pragma(#X)
#define SPMV_SPARSE_NO_UNROLL SPMV_SPARSE_PRAGMA(clang loop unroll(disable))
#else
#define SPMV_SPARSE_NO_UNROLL
#endif

#define SPMV_SPARSE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SPMV_SPARSE_SPM_CAPACITY (120 * 1024)

enum {
  SPMV_SPARSE_M =
      (uint32_t)(sizeof(spmv_sparse_result) / sizeof(spmv_sparse_result[0])),
  SPMV_SPARSE_N = (uint32_t)(sizeof(spmv_sparse_x_dram) /
                             sizeof(spmv_sparse_x_dram[0])),
  SPMV_SPARSE_K = (uint32_t)(sizeof(spmv_sparse_col_idx_dram) /
                             sizeof(spmv_sparse_col_idx_dram[0])),
};

// Shared SPM pointers are kept on separate cache lines so runtime lock traffic
// cannot clobber adjacent words in cache mode.
static T *x_vec __attribute__((section(".data"), aligned(64)));
static T *result __attribute__((section(".data"), aligned(64)));
static uint32_t *row_ptr __attribute__((section(".data"), aligned(64)));
static uint32_t *col_idx __attribute__((section(".data"), aligned(64)));

static uint32_t x_off_cache[SPMV_SPARSE_K]
    __attribute__((section(".data"), aligned(8)));
static T result_cache[SPMV_SPARSE_M]
    __attribute__((section(".data"), aligned(8)));

static T *dense_buf[2];
static uint32_t *col_buf[2];
static volatile int spmv_sparse_setup_error __attribute__((section(".data"), aligned(8)));

#if defined(__clang__)
#define SPMV_SPARSE_SPM_NOINLINE __attribute__((noinline, optnone))
#else
#define SPMV_SPARSE_SPM_NOINLINE __attribute__((noinline))
#endif

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

static inline void build_offsets(uint32_t *dst, const uint32_t *src,
                                 uint32_t nnz) {
  for (uint32_t i = 0; i < nnz; ++i) dst[i] = src[i] * sizeof(T);
}

static SPMV_SPARSE_SPM_NOINLINE double spmv_sparse_row_spm_scalar(
    const double *row_dense, const uint32_t *tile_col_idx, const double *x,
    uint32_t start, uint32_t end) {
  double sum = 0.0;
  for (uint32_t k = start; k < end; ++k) {
    const uint32_t col = tile_col_idx[k];
    const double a = row_dense[col];
    const double xv = x[col];
    double prod;
    // Keep multiply and add separate here. The plain C loop contracts to
    // scalar fmadd.d on this path and produces wrong SPM-mode results.
    asm volatile("fmul.d %0, %1, %2" : "=f"(prod) : "f"(a), "f"(xv));
    asm volatile("fadd.d %0, %1, %2" : "=f"(sum) : "f"(sum), "f"(prod));
  }
  return sum;
}

int main() {
  const uint32_t num_cores_hw = snrt_cluster_core_num();
  const uint32_t cid = snrt_cluster_core_idx();
  const uint32_t num_cores_cache =
      (SPMV_SPARSE_NUM_CORES > 0 &&
       SPMV_SPARSE_NUM_CORES < num_cores_hw)
          ? SPMV_SPARSE_NUM_CORES
          : num_cores_hw;
#if USE_CACHE == 0
  const uint32_t num_cores_spm =
      (SPMV_SPARSE_NUM_CORES > 0 &&
       SPMV_SPARSE_NUM_CORES < num_cores_hw)
          ? SPMV_SPARSE_NUM_CORES
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
  const uint32_t *cache_row_ptr = spmv_sparse_row_ptr_dram;
  const uint32_t *cache_col_idx = spmv_sparse_col_idx_dram;
  const T *cache_x_vec = spmv_sparse_x_dram;
  T *cache_result = result_cache;
  if (cid == 0) spmv_sparse_setup_error = 0;

#if USE_CACHE == 1
  const uint32_t row_start =
      (cid < num_cores_cache) ? (spmv_sparse_l.M * cid) / num_cores_cache : 0;
  const uint32_t row_end = (cid < num_cores_cache)
                               ? (spmv_sparse_l.M * (cid + 1)) / num_cores_cache
                               : 0;

  if (cid == 0) {
    build_offsets(x_off_cache, spmv_sparse_col_idx_dram, spmv_sparse_l.K);
  }
#else
  const size_t resident_bytes =
      (size_t)(spmv_sparse_l.M + 1) * sizeof(uint32_t) +
      (size_t)spmv_sparse_l.N * sizeof(T) +
      (size_t)spmv_sparse_l.M * sizeof(T) + 6 * 8;
  const size_t per_row_buffer_bytes =
      (size_t)spmv_sparse_l.N * (sizeof(T) + sizeof(uint32_t));
  const size_t total_l1_bytes = (size_t)spm_size * 1024;
  uint32_t rows_per_tile = 0;
  const uint32_t row_start =
      is_spm_worker ? (spmv_sparse_l.M * cid) / num_cores_spm : 0;
  const uint32_t row_end = is_spm_worker
                               ? (spmv_sparse_l.M * (cid + 1)) /
                                     num_cores_spm
                               : 0;
  (void)row_start;
  (void)row_end;
  (void)num_cores_cache;

  if (resident_bytes + 2 * per_row_buffer_bytes > total_l1_bytes) {
    if (cid == 0) {
      printf("Error: spmv_sparse SPM tile does not fit in L1 (N=%lu).\n",
             (unsigned long)spmv_sparse_l.N);
      spmv_sparse_setup_error = -1;
    }
  } else {
    rows_per_tile =
        (uint32_t)((total_l1_bytes - resident_bytes) / (2 * per_row_buffer_bytes));
    if (rows_per_tile > spmv_sparse_l.M) rows_per_tile = spmv_sparse_l.M;
    if (rows_per_tile < 1) rows_per_tile = 1;
  }

  if (cid == 0) {
    row_ptr = (uint32_t *)l1alloc_aligned(
        (spmv_sparse_l.M + 1) * sizeof(uint32_t), 8);
    x_vec = (T *)l1alloc_aligned(spmv_sparse_l.N * sizeof(T), 8);
    result = (T *)l1alloc_aligned(spmv_sparse_l.M * sizeof(T), 8);

    if (spmv_sparse_setup_error == 0) {
      rows_per_tile =
          (uint32_t)((total_l1_bytes - resident_bytes) / (2 * per_row_buffer_bytes));
      if (rows_per_tile > spmv_sparse_l.M) rows_per_tile = spmv_sparse_l.M;
      if (rows_per_tile < 1) rows_per_tile = 1;

      dense_buf[0] = (T *)l1alloc_aligned((size_t)rows_per_tile * spmv_sparse_l.N *
                                              sizeof(T),
                                          8);
      col_buf[0] = (uint32_t *)l1alloc_aligned(
          (size_t)rows_per_tile * spmv_sparse_l.N * sizeof(uint32_t), 8);
      dense_buf[1] = (T *)l1alloc_aligned((size_t)rows_per_tile * spmv_sparse_l.N *
                                              sizeof(T),
                                          8);
      col_buf[1] = (uint32_t *)l1alloc_aligned(
          (size_t)rows_per_tile * spmv_sparse_l.N * sizeof(uint32_t), 8);

      timer_dma = benchmark_get_cycle();
      snrt_dma_start_1d(row_ptr, spmv_sparse_row_ptr_dram,
                        (spmv_sparse_l.M + 1) * sizeof(uint32_t));
      snrt_dma_start_1d(x_vec, spmv_sparse_x_dram,
                        spmv_sparse_l.N * sizeof(T));
      snrt_dma_wait_all();
      timer_dma = benchmark_get_cycle() - timer_dma;
    }
  }

  snrt_cluster_hw_barrier();

  if (spmv_sparse_setup_error != 0) {
    ret = spmv_sparse_setup_error;
    set_eoc();
    return ret;
  }
#endif

  snrt_cluster_hw_barrier();

#if USE_CACHE == 1
  if (cid == 0 && spmv_sparse_l.K > 0) {
    const uint32_t first_col = cache_col_idx[cache_row_ptr[0]];
    volatile double warm =
        spmv_sparse_dense_dram[first_col] + cache_x_vec[first_col] +
        (double)cache_row_ptr[1];
    (void)warm;
  }
  snrt_cluster_hw_barrier();
#endif

  SPMV_SPARSE_NO_UNROLL
  for (int iter = 0; iter < measure_iter; ++iter) {
    if (cid == 0) {
      start_kernel();
      timer = benchmark_get_cycle();
    }

#if USE_CACHE == 1
    spmv_sparse_v64b(cache_row_ptr, x_off_cache, spmv_sparse_dense_dram,
                     spmv_sparse_l.N, cache_x_vec, cache_result, row_start,
                     row_end);
#else
    {
      uint32_t cur = 0;
      uint32_t tile_row = 0;

      if (cid == 0) {
        const uint32_t first_rows = SPMV_SPARSE_MIN(rows_per_tile, spmv_sparse_l.M);
        const uint32_t first_re = first_rows;
        const uint32_t first_nnz = row_ptr[first_re] - row_ptr[0];
        snrt_dma_start_1d(dense_buf[0], spmv_sparse_dense_dram,
                          (size_t)first_rows * spmv_sparse_l.N * sizeof(T));
        if (first_nnz > 0) {
          snrt_dma_start_1d(col_buf[0], spmv_sparse_col_idx_dram,
                            (size_t)first_nnz * sizeof(uint32_t));
        }
      }

      do {
        const uint32_t tile_rs = tile_row;
        const uint32_t cur_rows =
            SPMV_SPARSE_MIN(rows_per_tile, spmv_sparse_l.M - tile_rs);
        const uint32_t tile_re = tile_rs + cur_rows;
        const uint32_t tile_nnz_start = row_ptr[tile_rs];
        const uint32_t cur_tile_nnz = row_ptr[tile_re] - tile_nnz_start;

        if (cid == 0) {
          snrt_dma_wait_all();
        }

        tile_row = tile_re;
        snrt_cluster_hw_barrier();

        if (cid == 0 && tile_row < spmv_sparse_l.M) {
          const uint32_t nxt = 1 - cur;
          const uint32_t next_rows =
              SPMV_SPARSE_MIN(rows_per_tile, spmv_sparse_l.M - tile_row);
          const uint32_t next_re = tile_row + next_rows;
          const uint32_t next_nnz_start = row_ptr[tile_row];
          const uint32_t next_tile_nnz = row_ptr[next_re] - next_nnz_start;

          snrt_dma_start_1d(
              dense_buf[nxt],
              spmv_sparse_dense_dram + (size_t)tile_row * spmv_sparse_l.N,
              (size_t)next_rows * spmv_sparse_l.N * sizeof(T));
          if (next_tile_nnz > 0) {
            snrt_dma_start_1d(
                col_buf[nxt], spmv_sparse_col_idx_dram + next_nnz_start,
                (size_t)next_tile_nnz * sizeof(uint32_t));
          }
        }

        if (is_spm_worker) {
          const uint32_t tile_nrows = tile_re - tile_rs;
          const uint32_t core_rs =
              tile_rs + (tile_nrows * cid) / num_cores_spm;
          const uint32_t core_re =
              tile_rs + (tile_nrows * (cid + 1)) / num_cores_spm;

          for (uint32_t row = core_rs; row < core_re; ++row) {
            const uint32_t local_row = row - tile_rs;
            const uint32_t start = row_ptr[row] - tile_nnz_start;
            const uint32_t end = row_ptr[row + 1] - tile_nnz_start;
            const double *row_dense =
                dense_buf[cur] + (size_t)local_row * spmv_sparse_l.N;
            result[row] =
                spmv_sparse_row_spm_scalar(row_dense, col_buf[cur], x_vec,
                                           start, end);
          }
        }

        snrt_cluster_hw_barrier();
        (void)cur_tile_nnz;
        cur = 1 - cur;
      } while (tile_row < spmv_sparse_l.M);
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
    const uint32_t hw_cores_report = snrt_cluster_core_num();
#if USE_CACHE == 1
    const uint32_t active_cores_report =
        (SPMV_SPARSE_NUM_CORES > 0 && SPMV_SPARSE_NUM_CORES < hw_cores_report)
            ? SPMV_SPARSE_NUM_CORES
            : hw_cores_report;
#else
    const uint32_t active_cores_report =
        (SPMV_SPARSE_NUM_CORES > 0 && SPMV_SPARSE_NUM_CORES < hw_cores_report)
            ? SPMV_SPARSE_NUM_CORES
            : hw_cores_report;
#endif

    for (uint32_t i = 0; i < spmv_sparse_l.M; ++i) {
      const double got =
#if USE_CACHE == 1
          cache_result[i];
#else
          result[i];
#endif
      checksum += got;
      if (fp_check(&got, &spmv_sparse_result[i])) {
        ++errors;
        printf("Error: row %lu result=%f golden=%f\n",
               (unsigned long)i, got, spmv_sparse_result[i]);
      }
    }

    if (abs_diff(checksum, spmv_sparse_checksum) > 0.001) {
      ++errors;
      printf("Error: checksum=%f golden=%f\n", checksum,
             spmv_sparse_checksum);
    }

    write_cyc(timer_best);

    {
      const unsigned long performance =
          (timer_best > 0) ? (1000UL * 2UL * spmv_sparse_l.K / timer_best) : 0;
      const unsigned long utilization =
          (timer_best > 0)
              ? performance /
                    (2 * active_cores_report * num_fpu * 8 / sizeof(T))
              : 0;

      printf("\n----- (%lu x %lu, nnz=%lu) spmv_sparse -----\n",
             (unsigned long)spmv_sparse_l.M, (unsigned long)spmv_sparse_l.N,
             (unsigned long)spmv_sparse_l.K);
      printf("LMUL setting: m%d\n", SPMV_LMUL);
      printf("Active cores: %lu / %lu\n",
             (unsigned long)active_cores_report,
             (unsigned long)hw_cores_report);
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
