// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Author: Navaneeth Kunhi Purayil, ETH Zurich <nkunhi@iis.ee.ethz.ch>
//         Diyou Shen,              ETH Zurich

#include "db-gemv.h"

// ---------------------------------------------------------------------------
// Inner compute kernel: one column chunk for one core's row slice.
// All sizes come from the pre-computed params struct.
// ---------------------------------------------------------------------------
static void gemv_chunk_v64b(const double *a_buf, const double *b_buf,
                             double *out_buf, const unsigned int M_core,
                             const unsigned int chunk_cols,
                             const int first_chunk) {
  unsigned int vl, avl = M_core;
  const double *a_start = a_buf;
  double *out_ = out_buf;

  do {
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    const double *a_ = a_start;
    const double *b_ = b_buf;

    for (unsigned int col = 0; col < chunk_cols; col += 2) {
      asm volatile("vle64.v v0, (%0)" ::"r"(a_)); a_ += M_core;
      if (col == 0 && first_chunk) asm volatile("vfmul.vf  v4, v0, %0" ::"f"(*b_));
      else                         asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
      b_++;

      asm volatile("vle64.v v8, (%0)" ::"r"(a_)); a_ += M_core;
      if (col == 0 && first_chunk) asm volatile("vfmul.vf  v12, v8, %0" ::"f"(*b_));
      else                         asm volatile("vfmacc.vf v12, %0, v8" ::"f"(*b_));
      b_++;
    }

    asm volatile("vfadd.vv v4, v4, v12");
    if (first_chunk) {
      asm volatile("vse64.v v4, (%0)" ::"r"(out_));
    } else {
      asm volatile("vle64.v v8, (%0)" ::"r"(out_));
      asm volatile("vfadd.vv v8, v8, v4");
      asm volatile("vse64.v v8, (%0)" ::"r"(out_));
    }
    avl -= vl; out_ += vl; a_start += vl;
  } while (avl > 0);
}

static void gemv_chunk_v32b(const float *a_buf, const float *b_buf,
                             float *out_buf, const unsigned int M_core,
                             const unsigned int chunk_cols,
                             const int first_chunk) {
  unsigned int vl, avl = M_core;
  const float *a_start = a_buf;
  float *out_ = out_buf;

  do {
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
    const float *a_ = a_start;
    const float *b_ = b_buf;

    for (unsigned int col = 0; col < chunk_cols; col += 2) {
      asm volatile("vle32.v v0, (%0)" ::"r"(a_)); a_ += M_core;
      if (col == 0 && first_chunk) asm volatile("vfmul.vf  v4, v0, %0" ::"f"(*b_));
      else                         asm volatile("vfmacc.vf v4, %0, v0" ::"f"(*b_));
      b_++;

      asm volatile("vle32.v v8, (%0)" ::"r"(a_)); a_ += M_core;
      if (col == 0 && first_chunk) asm volatile("vfmul.vf  v12, v8, %0" ::"f"(*b_));
      else                         asm volatile("vfmacc.vf v12, %0, v8" ::"f"(*b_));
      b_++;
    }

    asm volatile("vfadd.vv v4, v4, v12");
    if (first_chunk) {
      asm volatile("vse32.v v4, (%0)" ::"r"(out_));
    } else {
      asm volatile("vle32.v v8, (%0)" ::"r"(out_));
      asm volatile("vfadd.vv v8, v8, v4");
      asm volatile("vse32.v v8, (%0)" ::"r"(out_));
    }
    avl -= vl; out_ += vl; a_start += vl;
  } while (avl > 0);
}

static void gemv_chunk_v16b(const __fp16 *a_buf, const __fp16 *b_buf,
                             __fp16 *out_buf, const unsigned int M_core,
                             const unsigned int chunk_cols,
                             const int first_chunk) {
  unsigned int vl, avl = M_core;
  const __fp16 *a_start = a_buf;
  __fp16 *out_ = out_buf;

  do {
    asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
    const __fp16 *a_ = a_start;
    const __fp16 *b_ = b_buf;

    for (unsigned int col = 0; col < chunk_cols; col += 2) {
      asm volatile("vle16.v v0, (%0)" ::"r"(a_)); a_ += M_core;
      asm volatile("vle16.v v8, (%0)" ::"r"(a_)); a_ += M_core;

      float t0, t1;
      asm volatile("flh %[t], 0(%[b])" : [t] "=f"(t0) : [b] "r"(b_));
      if (col == 0 && first_chunk) asm volatile("vfmul.vf  v4, v0, %0" ::"f"(t0));
      else                         asm volatile("vfmacc.vf v4, %0, v0" ::"f"(t0));
      b_++;

      asm volatile("flh %[t], 0(%[b])" : [t] "=f"(t1) : [b] "r"(b_));
      if (col == 0 && first_chunk) asm volatile("vfmul.vf  v12, v8, %0" ::"f"(t1));
      else                         asm volatile("vfmacc.vf v12, %0, v8" ::"f"(t1));
      b_++;
    }

    asm volatile("vfadd.vv v4, v4, v12");
    if (first_chunk) {
      asm volatile("vse16.v v4, (%0)" ::"r"(out_));
    } else {
      asm volatile("vle16.v v8, (%0)" ::"r"(out_));
      asm volatile("vfadd.vv v8, v8, v4");
      asm volatile("vse16.v v8, (%0)" ::"r"(out_));
    }
    avl -= vl; out_ += vl; a_start += vl;
  } while (avl > 0);
}

// ---------------------------------------------------------------------------
// DMA load: fill one slot from DRAM using 2D DMA for the matrix slices.
// One 2D transfer per core replaces the column-by-column 1D loop,
// giving the DMA engine maximum visibility to pipeline transfers.
//
// 2D transfer semantics:
//   snrt_dma_start_2d(dst, src, size, dst_stride, src_stride, repeat)
//   Copies `repeat` blocks of `size` bytes.
//   dst advances by dst_stride, src advances by src_stride each repeat.
//
// For core 0's matrix slice (M_core × next_cc, column-major in DRAM):
//   size       = col_bytes  (M_core elements per column)
//   dst_stride = col_bytes  (contiguous in L1)
//   src_stride = col_stride_b (M elements per column in DRAM)
//   repeat     = next_cc
//
// All sizes are pre-computed in params — no arithmetic here.
// ---------------------------------------------------------------------------
static void dma_load_chunk(char *slot,
                            const char *a_col, const char *b_col,
                            const unsigned int next_cc,
                            const db_gemv_params_t *p)
{
  // Core 0 matrix slice: contiguous columns in L1, strided in DRAM
  snrt_dma_start_2d(slot,
                    a_col,
                    p->col_bytes,    // size per column
                    p->col_bytes,    // dst stride (contiguous)
                    p->col_stride_b, // src stride (column stride in DRAM)
                    next_cc);

  // Core 1 matrix slice: same layout, offset by one core's rows in DRAM
  snrt_dma_start_2d(slot + p->core1_mat,
                    a_col + p->row_stride_b,
                    p->col_bytes,
                    p->col_bytes,
                    p->col_stride_b,
                    next_cc);

  // b slice: always contiguous in both DRAM and L1
  snrt_dma_start_1d(slot + p->b_region, b_col,
                    next_cc * p->b_elem_size);
}

// ---------------------------------------------------------------------------
// Common double-buffer loop, shared across all precisions via function ptr.
//
// Correct overlap protocol:
//   (A) Core 0: kick DMA for chunk 0 (no wait yet)
//   barrier    ← both cores aligned before entering loop
//   loop k = 0..num_chunks-1:
//     (B) Core 0: dma_wait_all  ← chunk k is now ready in cur slot
//     barrier    ← both cores see chunk k data
//     (C) Core 0: kick DMA for chunk k+1 into nxt slot  //         Both:  compute chunk k from cur slot            } overlapped
//     barrier    ← compute done; DMA may still be running
//     (D) advance DRAM pointers
//   end loop
//   (Core 0 does NOT wait after last chunk's compute — no next DMA was kicked)
// ---------------------------------------------------------------------------
typedef void (*chunk_fn_t)(const void*, const void*, void*,
                            unsigned int, unsigned int, int);

static void db_gemv_loop(const void *a_dram, const void *b_dram,
                          void *l1_buf, void *out_buf,
                          const unsigned int M_core,
                          const unsigned int N,
                          const unsigned int cid,
                          const db_gemv_params_t *p,
                          chunk_fn_t compute_chunk)
{
  char *slot[2];
  slot[0] = (char *)l1_buf;
  slot[1] = (char *)l1_buf + p->slot_bytes;

  const char *a_chunk = (const char *)a_dram;
  const char *b_chunk = (const char *)b_dram;

  // (A) Kick DMA for chunk 0 — do NOT wait here
  if (cid == 0) {
    const unsigned int first_cc = (p->chunk_cols <= N) ? p->chunk_cols : N;
    dma_load_chunk(slot[0], a_chunk, b_chunk, first_cc, p);
  }
  snrt_cluster_hw_barrier();  // both cores enter loop together

  for (unsigned int col = 0; col < N; col += p->chunk_cols) {
    const unsigned int cur      = (col / p->chunk_cols) & 1;
    const unsigned int nxt      = 1 - cur;
    const unsigned int next_col = col + p->chunk_cols;
    const int first_chunk       = (col == 0);

    // (B) Wait for current chunk to be ready
    if (cid == 0)
      snrt_dma_wait_all();
    snrt_cluster_hw_barrier();  // both cores see current chunk data

    // (C) Kick next chunk DMA + compute current chunk simultaneously
    if (cid == 0 && next_col < N) {
      const unsigned int next_cc = (next_col + p->chunk_cols <= N)
                                   ? p->chunk_cols : N - next_col;
      dma_load_chunk(slot[nxt],
                     a_chunk + p->chunk_col_stride,
                     b_chunk + p->chunk_b_stride,
                     next_cc, p);
    }

    // Both cores compute on current slot — overlaps with DMA above
    compute_chunk((const char *)(slot[cur]) + p->core_mat_off,
                  (const char *)(slot[cur]) + p->b_region,
                  out_buf, M_core, p->chunk_cols, first_chunk);

    snrt_cluster_hw_barrier();  // (D) compute done; advance for next iter

    a_chunk += p->chunk_col_stride;
    b_chunk += p->chunk_b_stride;
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void db_gemv_v64b(const double *a_dram, const double *b_dram,
                  double *c,
                  unsigned int M_core, unsigned int N, unsigned int cid,
                  void *l1_buf, double *out_buf,
                  const db_gemv_params_t *p) {

  db_gemv_loop(a_dram, b_dram, l1_buf, out_buf,
               M_core, N, cid, p,
               (chunk_fn_t)gemv_chunk_v64b);

  unsigned int vl, avl = M_core;
  double *out_ = out_buf;
  do {
    asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle64.v v0, (%0)" ::"r"(out_));
    asm volatile("vse64.v v0, (%0)" ::"r"(c));
    avl -= vl; out_ += vl; c += vl;
  } while (avl > 0);
}

void db_gemv_v32b(const float *a_dram, const float *b_dram,
                  float *c,
                  unsigned int M_core, unsigned int N, unsigned int cid,
                  void *l1_buf, float *out_buf,
                  const db_gemv_params_t *p) {

  db_gemv_loop(a_dram, b_dram, l1_buf, out_buf,
               M_core, N, cid, p,
               (chunk_fn_t)gemv_chunk_v32b);

  unsigned int vl, avl = M_core;
  float *out_ = out_buf;
  do {
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle32.v v0, (%0)" ::"r"(out_));
    asm volatile("vse32.v v0, (%0)" ::"r"(c));
    avl -= vl; out_ += vl; c += vl;
  } while (avl > 0);
}

void db_gemv_v16b(const __fp16 *a_dram, const __fp16 *b_dram,
                  __fp16 *c,
                  unsigned int M_core, unsigned int N, unsigned int cid,
                  void *l1_buf, __fp16 *out_buf,
                  const db_gemv_params_t *p) {

  db_gemv_loop(a_dram, b_dram, l1_buf, out_buf,
               M_core, N, cid, p,
               (chunk_fn_t)gemv_chunk_v16b);

  unsigned int vl, avl = M_core;
  __fp16 *out_ = out_buf;
  do {
    asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
    asm volatile("vle16.v v0, (%0)" ::"r"(out_));
    asm volatile("vse16.v v0, (%0)" ::"r"(c));
    avl -= vl; out_ += vl; c += vl;
  } while (avl > 0);
}
