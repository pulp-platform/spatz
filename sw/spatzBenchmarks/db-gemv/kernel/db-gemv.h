// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0

#ifndef _DB_GEMV_H
#define _DB_GEMV_H

#include <snrt.h>

// ---------------------------------------------------------------------------
// Pre-computed kernel parameters.
// Fill once in main before the timer starts, pass to kernel each iteration.
//
// All byte-level sizes and strides are computed here so the kernel's hot
// path (the chunk loop and DMA helper) contains only pointer additions.
// ---------------------------------------------------------------------------
typedef struct {
  // Chunk configuration
  unsigned int chunk_cols;       // columns per DMA chunk (even, >= 2)

  // Per-column sizes
  unsigned int col_bytes;        // M_core * elem_size
  unsigned int col_stride_b;     // M * elem_size  (DRAM column stride)
  unsigned int row_stride_b;     // M_core * elem_size  (offset to core 1 rows)

  // Slot layout offsets (in bytes, from slot base)
  unsigned int core1_mat;        // offset of core 1's matrix region
  unsigned int b_region;         // offset of b slice  (= 2 * core1_mat)
  unsigned int slot_bytes;       // total size of one ping-pong slot

  // Per-iteration DRAM advances
  unsigned int chunk_col_stride; // chunk_cols * col_stride_b
  unsigned int chunk_b_stride;   // chunk_cols * elem_size

  // This core's matrix offset within a slot
  unsigned int core_mat_off;     // cid * core1_mat

  // b slice size for a full chunk and output accumulator size
  unsigned int b_bytes;          // chunk_cols * elem_size
  unsigned int b_elem_size;      // elem_size (for partial last chunk b transfer)
  unsigned int out_bytes;        // M_core * elem_size
} db_gemv_params_t;

// ---------------------------------------------------------------------------
// Fill a db_gemv_params_t.  Call once per core before the timer.
// Both cores can call this independently — no sync required.
// ---------------------------------------------------------------------------
static inline void db_gemv_params_init(
    db_gemv_params_t   *p,
    const unsigned int  M,
    const unsigned int  M_core,
    const unsigned int  N,
    const unsigned int  num_cores,
    const unsigned int  cid,
    const unsigned int  elem_size,
    const unsigned int  l1_budget_bytes)
{
  // Compute chunk_cols:
  //   2 slots × (num_cores × M_core + 1) × chunk_cols × elem
  //   + num_cores × M_core × elem  (output buffers)
  //   ≤ l1_budget_bytes
  const unsigned int out_total = num_cores * M_core * elem_size;
  unsigned int cc = 2;
  if (l1_budget_bytes > out_total)
    cc = (l1_budget_bytes - out_total) /
         (2 * (num_cores * M_core + 1) * elem_size);
  cc = cc & ~1u;          // round down to even
  if (cc < 2)  cc = 2;
  if (cc > N)  cc = N & ~1u;
  if (cc < 2)  cc = 2;

  p->chunk_cols      = cc;
  p->col_bytes       = M_core * elem_size;
  p->col_stride_b    = M * elem_size;
  p->row_stride_b    = M_core * elem_size;
  p->core1_mat       = M_core * cc * elem_size;
  p->b_region        = 2 * p->core1_mat;
  p->b_bytes         = cc * elem_size;
  p->b_elem_size     = elem_size;
  p->slot_bytes      = p->b_region + p->b_bytes;
  p->chunk_col_stride = cc * p->col_stride_b;
  p->chunk_b_stride  = cc * elem_size;
  p->core_mat_off    = cid * p->core1_mat;
  p->out_bytes       = M_core * elem_size;
}

// ---------------------------------------------------------------------------
// L1 buffer size needed for the two ping-pong slots.
// Output accumulators are allocated separately (num_cores × out_bytes).
// ---------------------------------------------------------------------------
static inline unsigned int db_gemv_l1_buf_bytes(const db_gemv_params_t *p)
{
  return 2 * p->slot_bytes;
}

// ---------------------------------------------------------------------------
// Double-buffered GEMV: c = A * b
//
//   a_dram  : full A base in DRAM (column-major, M × N), NOT offset by cid
//   b_dram  : full b vector in DRAM (N elements)
//   c       : output slice for this core (M_core elements) — written to DRAM
//   M_core  : rows per core
//   N       : columns of A
//   cid     : this core's index
//   l1_buf  : L1 ping-pong scratch, size = db_gemv_l1_buf_bytes(p)
//   out_buf : L1 accumulator for this core, size = p->out_bytes
//   p       : pre-computed parameters (from db_gemv_params_init)
// ---------------------------------------------------------------------------
void db_gemv_v64b(const double *a_dram, const double *b_dram,
                  double *c,
                  unsigned int M_core, unsigned int N, unsigned int cid,
                  void *l1_buf, double *out_buf,
                  const db_gemv_params_t *p);

void db_gemv_v32b(const float *a_dram, const float *b_dram,
                  float *c,
                  unsigned int M_core, unsigned int N, unsigned int cid,
                  void *l1_buf, float *out_buf,
                  const db_gemv_params_t *p);

void db_gemv_v16b(const __fp16 *a_dram, const __fp16 *b_dram,
                  __fp16 *c,
                  unsigned int M_core, unsigned int N, unsigned int cid,
                  void *l1_buf, __fp16 *out_buf,
                  const db_gemv_params_t *p);

#endif // _DB_GEMV_H
