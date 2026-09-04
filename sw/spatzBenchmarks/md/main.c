// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0

#include <benchmark.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/md.h"

#if (PREC != 64)
#error "md currently supports double precision only"
#endif

#define T double

#ifndef MD_NUM_CORES
#define MD_NUM_CORES 0
#endif

#if defined(__clang__)
#define MD_PRAGMA(X) _Pragma(#X)
#define MD_NO_UNROLL MD_PRAGMA(clang loop unroll(disable))
#else
#define MD_NO_UNROLL
#endif

enum {
  MD_M = (uint32_t)(sizeof(md_force_x) / sizeof(md_force_x[0])),
  MD_K = (uint32_t)(sizeof(md_neigh_idx_dram) / sizeof(md_neigh_idx_dram[0])),
};

static T *pos_x, *pos_y, *pos_z;
static T *force_x, *force_y, *force_z;
static uint32_t *neigh_ptr;
static uint32_t *neigh_idx;
static uint32_t *x_off;

// SPM mode only: when the full neigh_idx/x_off arrays don't fit in the SPM
// budget, stream neigh_idx in per-particle via DMA instead (low efficiency
// is acceptable -- see spmv/main.c for the same pattern and rationale).
// pos_x/y/z, force_x/y/z, neigh_ptr all scale with M only and stay
// resident; only neigh_idx scales with K and needs streaming.
#define MD_SPM_BUDGET_BYTES (96 * 1024)
static int md_stream_nnz;
static uint32_t *stream_idx_base;
static uint32_t *stream_off_base;

// In cache mode the temporary offset/output arrays stay in .data (L2, cacheable).
static uint32_t x_off_cache[MD_K] __attribute__((section(".data"), aligned(8)));
static T force_x_cache[MD_M] __attribute__((section(".data"), aligned(8)));
static T force_y_cache[MD_M] __attribute__((section(".data"), aligned(8)));
static T force_z_cache[MD_M] __attribute__((section(".data"), aligned(8)));

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
      (MD_NUM_CORES > 0 && MD_NUM_CORES < num_cores_hw) ? MD_NUM_CORES
                                                        : num_cores_hw;

#if USE_CACHE == 1
  uint32_t spm_size = 16;
#else
  uint32_t spm_size = 120;
#endif

  const uint32_t num_fpu = 4;

  if (cid == 0) {
#if HPDCACHE == 1
    // See spmv/main.c: l1d_init()'s SPM-size-commit tail crashes QuestaSim
    // for some binary layouts, and HPDcache has its own native ispm
    // mechanism anyway, so skip it here too.
    (void)spm_size;
#else
    l1d_init(spm_size);
#endif
  }
  snrt_cluster_hw_barrier();

#if MEAS_1ITER == 1
  volatile int measure_iter = 1;
#else
  volatile int measure_iter = 2;
#endif

  unsigned int timer = (unsigned int)-1;
  unsigned int timer_best = (unsigned int)-1;
  unsigned int timer_1iter = (unsigned int)-1;
  int ret = 0;

  const uint32_t row_start = (cid < num_cores) ? (md_l.M * cid) / num_cores : 0;
  const uint32_t row_end =
      (cid < num_cores) ? (md_l.M * (cid + 1)) / num_cores : 0;

#if USE_CACHE == 1
  x_off = x_off_cache;
  force_x = force_x_cache;
  force_y = force_y_cache;
  force_z = force_z_cache;
  if (cid == 0) {
    build_offsets(x_off, md_neigh_idx_dram, md_l.K);
  }

  neigh_ptr = md_neigh_ptr_dram;
  neigh_idx = md_neigh_idx_dram;
  pos_x = md_pos_x_dram;
  pos_y = md_pos_y_dram;
  pos_z = md_pos_z_dram;
#else
  if (cid == 0) {
    neigh_ptr = (uint32_t *)l1alloc_aligned((md_l.M + 1) * sizeof(uint32_t), 8);
    pos_x = (T *)l1alloc_aligned(md_l.M * sizeof(T), 8);
    pos_y = (T *)l1alloc_aligned(md_l.M * sizeof(T), 8);
    pos_z = (T *)l1alloc_aligned(md_l.M * sizeof(T), 8);
    force_x = (T *)l1alloc_aligned(md_l.M * sizeof(T), 8);
    force_y = (T *)l1alloc_aligned(md_l.M * sizeof(T), 8);
    force_z = (T *)l1alloc_aligned(md_l.M * sizeof(T), 8);

    snrt_dma_start_1d(neigh_ptr, md_neigh_ptr_dram,
                      (md_l.M + 1) * sizeof(uint32_t));
    snrt_dma_start_1d(pos_x, md_pos_x_dram, md_l.M * sizeof(T));
    snrt_dma_start_1d(pos_y, md_pos_y_dram, md_l.M * sizeof(T));
    snrt_dma_start_1d(pos_z, md_pos_z_dram, md_l.M * sizeof(T));
    snrt_dma_wait_all();

    const size_t nnz_bytes = (size_t)md_l.K * (sizeof(uint32_t) + sizeof(uint32_t));
    md_stream_nnz = (nnz_bytes > MD_SPM_BUDGET_BYTES);

    if (!md_stream_nnz) {
      neigh_idx = (uint32_t *)l1alloc_aligned(md_l.K * sizeof(uint32_t), 8);
      x_off = (uint32_t *)l1alloc_aligned(md_l.K * sizeof(uint32_t), 8);

      snrt_dma_start_1d(neigh_idx, md_neigh_idx_dram, md_l.K * sizeof(uint32_t));
      snrt_dma_wait_all();
      build_offsets(x_off, neigh_idx, md_l.K);
    } else {
      uint32_t max_row_nnz = 0;
      for (uint32_t i = 0; i < md_l.M; ++i) {
        const uint32_t n = neigh_ptr[i + 1] - neigh_ptr[i];
        if (n > max_row_nnz) max_row_nnz = n;
      }
      stream_idx_base = (uint32_t *)l1alloc_aligned(max_row_nnz * sizeof(uint32_t), 8);
      stream_off_base = (uint32_t *)l1alloc_aligned(max_row_nnz * sizeof(uint32_t), 8);
    }
  }
#endif

  snrt_cluster_hw_barrier();

  MD_NO_UNROLL
  for (int iter = 0; iter < measure_iter; ++iter) {
    if (cid == 0) {
      start_kernel();
      timer = benchmark_get_cycle();
    }

#if USE_CACHE == 1
    md_lj_v64b(neigh_ptr, neigh_idx, x_off, pos_x, pos_y, pos_z, force_x,
              force_y, force_z, row_start, row_end);
#else
    if (!md_stream_nnz) {
      md_lj_v64b(neigh_ptr, neigh_idx, x_off, pos_x, pos_y, pos_z, force_x,
                force_y, force_z, row_start, row_end);
    } else if (cid == 0) {
      // Single-core streaming: only core 0 can issue DMA (see spmv/main.c).
      for (uint32_t row = 0; row < md_l.M; ++row) {
        const uint32_t start = neigh_ptr[row];
        const uint32_t end = neigh_ptr[row + 1];
        const uint32_t nnz = end - start;
        if (nnz > 0) {
          snrt_dma_start_1d(stream_idx_base, md_neigh_idx_dram + start,
                            nnz * sizeof(uint32_t));
          snrt_dma_wait_all();
          build_offsets(stream_off_base, stream_idx_base, nnz);
        }
        // The kernel indexes pos_x/y/z[i] directly by row (for the
        // particle's own position, unlike SpMV which only ever gathers
        // its x vector) -- so we must call with the real global row
        // index, not a fake local [0,1) range. neigh_ptr still needs to
        // resolve to {0, nnz} at indices {row, row+1} though, since the
        // chunk buffers only hold this row's data starting at offset 0;
        // a negative-offset view makes neigh_ptr[row]/[row+1] land on
        // local_neigh_ptr[0]/[1] without needing a full M+1 array.
        uint32_t local_neigh_ptr[2] = {0, nnz};
        uint32_t *row_neigh_ptr = local_neigh_ptr - row;
        md_lj_v64b(row_neigh_ptr, stream_idx_base, stream_off_base, pos_x,
                  pos_y, pos_z, force_x, force_y, force_z, row, row + 1);
      }
    }
#endif

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

    for (uint32_t i = 0; i < md_l.M; ++i) {
      checksum += force_x[i] + force_y[i] + force_z[i];
      if (fp_check(&force_x[i], &md_force_x[i]) ||
          fp_check(&force_y[i], &md_force_y[i]) ||
          fp_check(&force_z[i], &md_force_z[i])) {
        ++errors;
      }
    }

    // Printf over UART is slow in RTL sim -- only print a summary count,
    // not one line per mismatch (can be hundreds at larger sizes).
    if (errors) {
      printf("Errors: %d / %u particles mismatched\n", errors, md_l.M);
    }

    if (abs_diff(checksum, md_checksum) > 0.001) {
      ++errors;
      printf("Error: checksum=%f golden=%f\n", checksum, md_checksum);
    }

    write_cyc(timer_best);

    {
      // 15 FLOPs/pair is a rough LJ-force-eval count (sub x3, mul/macc for
      // r2, div, 2 mul for sr2/sr6-ish chain, mul x3 for force) -- fine as
      // a comparative throughput metric, not a precise FLOP audit.
      const unsigned long performance = 1000UL * 15UL * md_l.K / timer_best;
      const unsigned long utilization =
          performance / (2 * num_cores * num_fpu * 8 / sizeof(T));

      printf("\n----- (%u particles, %u neighbor entries) md-lj -----\n",
             md_l.M, md_l.K);
      printf("LMUL setting: m%d\n", MD_LMUL);
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
