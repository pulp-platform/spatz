# spmm_sparse

`spmm_sparse` is a dense-backed sparse matrix-matrix multiplication benchmark.
It is intended to complement `spmv_sparse` with the same high-level goal:
stress the in-situ cache with irregular accesses, while keeping a correct
SPM+DMA baseline for comparison.

## Kernel Purpose

The benchmark computes:

- `A`: sparse logical matrix of shape `M x K`
- `B`: dense matrix of shape `K x N`
- `C`: dense result of shape `M x N`
- operation: `C = A * B`

The sparse structure of `A` is described by CSR-style metadata:

- `row_ptr[M + 1]`
- `col_idx[nnz]`

But the values of `A` are stored in a full dense row-major array `dense_a[M][K]`.
Only the positions listed by `col_idx` are treated as logical nonzeros.

This format is deliberate. It makes the cache version touch:

- sparse positions inside a dense-backed row of `A`
- widely separated rows of `B`

That creates more irregular memory traffic than a compressed sparse-value
format, especially when column order is randomized.

## Data Format

The generated header provides:

- `spmm_sparse_l`
  - metadata with `M`, `N`, `K`, `nnz`, and datatype
- `spmm_sparse_row_ptr_dram`
  - CSR row pointer for `A`
- `spmm_sparse_col_idx_dram`
  - CSR column indices for `A`
- `spmm_sparse_dense_a_dram`
  - dense-backed `A`, stored row-major as `M * K`
- `spmm_sparse_b_dram`
  - dense `B`, stored row-major as `K * N`
- `spmm_sparse_result`
  - golden dense `C`, stored row-major as `M * N`
- `spmm_sparse_checksum`
  - checksum of the golden `C`

Interpretation:

- row `r` of `A` spans `k = row_ptr[r] .. row_ptr[r + 1] - 1`
- each structural nonzero uses `col = col_idx[k]`
- the actual value is `dense_a[r][col]`
- then `dense_a[r][col] * B[col][n]` contributes to every output column `n`

## Cache-Based Kernel

Cache mode is selected with `USE_CACHE=1`.

Behavior:

- use DRAM-backed arrays directly
- do not DMA `A` or `B` into L1 first
- partition rows across active cores
- vectorize across the output-column dimension `N`

Inner-row flow:

1. pick one sparse row `r`
2. for a chunk of output columns, set `vl` with `vsetvli ... e64, m4`
3. take the first structural nonzero `(r, col)`
4. scalar-load `a = dense_a[r][col]`
5. vector-load contiguous `B[col][p : p + vl]`
6. initialize accumulator with `vfmul.vf`
7. for each remaining structural nonzero in the row:
   - scalar-load the next `a`
   - vector-load the matching contiguous row chunk from `B`
   - accumulate with `vfmacc.vf`
8. store the output chunk to `C[r][p : p + vl]`

This is not a gather kernel inside the vector lane dimension. The irregularity
comes from sparse jumps in the `K` dimension: every structural nonzero can jump
to a different dense-backed `A` position and a different row of `B`.

## SPM + DMA Kernel

SPM mode is selected with `USE_CACHE=0`.

The purpose here is correctness-first double buffering, not peak performance.

Resident data kept in L1:

- `row_ptr`
- full dense `B`
- output `C`

Double-buffered tile data:

- dense row tiles from `dense_a`
- matching `col_idx` tile segments

Tile flow:

1. DMA the resident data into L1 once before timing
2. choose `rows_per_tile` from remaining L1 capacity
3. allocate two tile buffers for dense `A` rows and `col_idx`
4. preload tile 0
5. for each tile:
   - wait for current tile DMA to finish
   - launch DMA for the next tile into the other buffer
   - split rows of the current tile across compute cores
   - run the same row kernel on the staged tile data
6. repeat until all rows are processed

The tile-local call passes:

- `row_base`: global row index of the first row in the tile
- `nnz_base`: global nnz index of the first structural nonzero in the tile

That lets the row kernel reuse the global `row_ptr` while indexing tile-local
`dense_a` and `col_idx` buffers correctly.

## Timing

The benchmark follows the same timing policy as `spmv` and `spmv_sparse`.

Included in the kernel-cycle count:

- cache-mode compute
- SPM per-tile DMA launched inside the timed loop
- SPM per-tile compute
- the end barrier of each measured iteration

Excluded from the kernel-cycle count:

- one-time L1/cache setup
- one-time resident DMA setup in SPM mode
- dataset generation

The resident DMA time is reported separately as `timer_dma`.

## Files

- `main.c`
  - benchmark harness, timing, cache/SPM dispatch, validation
- `kernel/spmm_sparse.c`
  - vectorized cache/SPM compute kernel (`LMUL = m4` across output columns)
- `kernel/spmm_sparse.h`
  - kernel interface
- `script/gen_data.py`
  - data generator
- `script/spmm_sparse.json`
  - default dataset configuration
- `data/`
  - generated headers
