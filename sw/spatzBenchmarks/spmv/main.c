// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/spmv.c"

#if (PREC != 64)
#error "spmv currently supports double precision only"
#endif

#define T double

// Use all cluster cores by default. Set SPMV_NUM_CORES to a positive value to
// limit the kernel to a smaller number of worker cores.
#ifndef SPMV_NUM_CORES
#define SPMV_NUM_CORES 0
#endif

enum {
  SPMV_M = (uint32_t)(sizeof(spmv_result) / sizeof(spmv_result[0])),
  SPMV_N = (uint32_t)(sizeof(spmv_x_dram) / sizeof(spmv_x_dram[0])),
  SPMV_K = (uint32_t)(sizeof(spmv_col_idx_dram) / sizeof(spmv_col_idx_dram[0])),
};

static T *row_val;
static T *x_vec;
static T *result;
static uint32_t *row_ptr;
static uint32_t *col_idx;
static uint32_t *x_off;

// In cache mode we keep temporary arrays in .data (L2-backed, cacheable).
static uint32_t x_off_cache[SPMV_K] __attribute__((section(".data"), aligned(8)));
static T result_cache[SPMV_M] __attribute__((section(".data"), aligned(8)));

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

int main() {
  const uint32_t num_cores_hw = snrt_cluster_core_num();
  const uint32_t cid = snrt_cluster_core_idx();
  const uint32_t num_cores =
      (SPMV_NUM_CORES > 0 && SPMV_NUM_CORES < num_cores_hw) ? SPMV_NUM_CORES
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

#if MEAS_1ITER == 1
  const int measure_iter = 1;
#else
  const int measure_iter = 2;
#endif

  unsigned int timer = (unsigned int)-1;
  unsigned int timer_best = (unsigned int)-1;
  unsigned int timer_1iter = (unsigned int)-1;
  int ret = 0;

  const uint32_t row_start = (cid < num_cores) ? (spmv_l.M * cid) / num_cores : 0;
  const uint32_t row_end =
      (cid < num_cores) ? (spmv_l.M * (cid + 1)) / num_cores : 0;

#if USE_CACHE == 1
  if (cid == 0) {
    x_off = x_off_cache;
    result = result_cache;
    build_offsets(x_off, spmv_col_idx_dram, spmv_l.K);
  }

  row_ptr = spmv_row_ptr_dram;
  col_idx = spmv_col_idx_dram;
  row_val = spmv_val_dram;
  x_vec = spmv_x_dram;
#else
  if (cid == 0) {
    row_ptr = (uint32_t *)l1alloc_aligned((spmv_l.M + 1) * sizeof(uint32_t), 8);
    col_idx = (uint32_t *)l1alloc_aligned(spmv_l.K * sizeof(uint32_t), 8);
    x_off = (uint32_t *)l1alloc_aligned(spmv_l.K * sizeof(uint32_t), 8);
    row_val = (T *)l1alloc_aligned(spmv_l.K * sizeof(T), 8);
    x_vec = (T *)l1alloc_aligned(spmv_l.N * sizeof(T), 8);
    result = (T *)l1alloc_aligned(spmv_l.M * sizeof(T), 8);

    snrt_dma_start_1d(row_ptr, spmv_row_ptr_dram,
                      (spmv_l.M + 1) * sizeof(uint32_t));
    snrt_dma_start_1d(col_idx, spmv_col_idx_dram, spmv_l.K * sizeof(uint32_t));
    snrt_dma_start_1d(row_val, spmv_val_dram, spmv_l.K * sizeof(T));
    snrt_dma_start_1d(x_vec, spmv_x_dram, spmv_l.N * sizeof(T));
    snrt_dma_wait_all();
    build_offsets(x_off, col_idx, spmv_l.K);
  }
#endif

  snrt_cluster_hw_barrier();

  for (int iter = 0; iter < measure_iter; ++iter) {
    if (cid == 0) {
      start_kernel();
      timer = benchmark_get_cycle();
    }

    spmv_v64b(row_ptr, x_off, row_val, x_vec, result, row_start, row_end);

    snrt_cluster_hw_barrier();

    if (cid == 0) {
      stop_kernel();
      timer = benchmark_get_cycle() - timer;
      if (iter == 0) {
        timer_1iter = timer;
      } else {
        timer_best = (timer_best > timer) ? timer : timer_best;
      }
    }

    snrt_cluster_hw_barrier();
  }

  if (measure_iter == 1) timer_best = timer_1iter;

  if (cid == 0) {
    double checksum = 0.0;
    int errors = 0;

    for (uint32_t i = 0; i < spmv_l.M; ++i) {
      checksum += result[i];
      if (fp_check(&result[i], &spmv_result[i])) {
        ++errors;
        printf("Error: row %u result=%f golden=%f\n", i, result[i],
               spmv_result[i]);
      }
    }

    if (abs_diff(checksum, spmv_checksum) > 0.001) {
      ++errors;
      printf("Error: checksum=%f golden=%f\n", checksum, spmv_checksum);
    }

    write_cyc(timer_best);

    {
      const unsigned long performance = 1000UL * 2UL * spmv_l.K / timer_best;
      const unsigned long utilization =
          performance / (2 * num_cores * num_fpu * 8 / sizeof(T));

      printf("\n----- (%u x %u, nnz=%u) spmv -----\n", spmv_l.M, spmv_l.N,
             spmv_l.K);
      printf("LMUL setting: m%d\n", SPMV_LMUL);
      printf("Active cores: %u / %u\n", num_cores, num_cores_hw);
      printf("The first iter takes %u cycles.\n", timer_1iter);
      printf("The best execution took %u cycles.\n", timer_best);
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
