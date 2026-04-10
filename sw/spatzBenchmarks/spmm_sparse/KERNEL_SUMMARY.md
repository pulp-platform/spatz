# `spmm_sparse` Kernel Summary

`spmm_sparse` computes:

`C = A_sparse * B_dense`

where:
- `A` is logically sparse and described by `row_ptr` and `col_idx`
- the values of `A` are stored in a dense row-major backing array
- `B` is a dense `K x N` matrix
- `C` is a dense `M x N` output matrix

In other words:
- the left matrix `A` is sparse logically
- the right matrix `B` is dense with no sparsity in the current kernel

In the vector kernel, the code walks one sparse row of `A`. For each nonzero
`A[row, col]`, it loads the dense row `B[col, :]`, vectorizes across the output
columns with `LMUL=m4`, and accumulates into `C[row, :]`.

# Inner Loop Calculation

`spmm_sparse` inner loop for one output row works like this:

1. Read the sparse range for the row:
   - `start = row_ptr[row]`
   - `end = row_ptr[row + 1]`

2. For each output-column slice `p : p + vl`:
   - take the first sparse column `col = col_idx[start]`
   - load `B[col, p : p + vl]`
   - multiply that vector by the scalar `A[row, col]`
   - use it to initialize the accumulator vector

3. For each remaining sparse entry in the row:
   - read `col = col_idx[k]`
   - load `B[col, p : p + vl]`
   - multiply by the scalar `A[row, col]`
   - accumulate into the same vector accumulator

4. Store the accumulator back to `C[row, p : p + vl]`

Mathematically, for each row `r` and output column `j`:

`C[r, j] = sum over k in sparse row r of A[r, col_k] * B[col_k, j]`

The implementation vectorizes over `j`, not over the sparse index list.

# Source Data Layout

The generated source data stores `A` in two forms:

1. Sparse structure:
   - `row_ptr`
   - `col_idx`

2. Dense value backing:
   - `dense_a[M][K]` in row-major layout

So `A` is logically sparse, but its numeric values are stored in a full dense
array. Only positions listed by `row_ptr` and `col_idx` are used by the kernel.

`B` is stored as a normal dense matrix:
- `b[K][N]` in row-major layout

`C` is also dense:
- `result[M][N]`

The generated data header therefore contains:
- `spmm_sparse_row_ptr_dram`
- `spmm_sparse_col_idx_dram`
- `spmm_sparse_dense_a_dram`
- `spmm_sparse_b_dram`
- `spmm_sparse_result`

This layout is intentional:
- `row_ptr` and `col_idx` define the sparse pattern
- `dense_a` forces dense-row memory traffic for the left matrix
- `b` remains dense because each sparse `A[row, col]` uses the full row
  `B[col, :]`

Cache mode reads the DRAM-backed arrays directly. SPM mode keeps `row_ptr`, `B`,
and `C` resident in L1, and double-buffers dense row tiles of `A` plus matching
`col_idx` tiles with DMA.

# Comparison With `spmv_sparse`

`spmv_sparse` computes:

`y = A_sparse * x`

Its inner loop is different:

1. Read the sparse range for the row:
   - `start = row_ptr[row]`
   - `end = row_ptr[row + 1]`

2. Load a vector of sparse column offsets from `x_off`

3. Use indexed vector loads to gather:
   - `A[row, col_k]` from the dense-backed row
   - `x[col_k]` from the dense input vector

4. Multiply the gathered vectors elementwise and accumulate

5. Reduce the vector accumulator to one scalar result:
   - `y[row]`

Mathematically:

`y[r] = sum over k in sparse row r of A[r, col_k] * x[col_k]`

The main difference is reuse:
- `spmv_sparse`: each nonzero contributes to one scalar output
- `spmm_sparse`: each nonzero contributes to all `N` output columns

That leads to three practical differences:

1. `spmm_sparse` has higher arithmetic intensity.
   One sparse lookup does more useful work.

2. `spmm_sparse` amortizes memory and DMA better.
   One `A[row, col]` is reused across a vector slice of `B[col, :]`.

3. `spmv_sparse` is the stronger cache-stress kernel.
   It has less compute per sparse access, so cache miss cost is more exposed.

Short summary:
- use `spmv_sparse` to emphasize cache-vs-SPM memory-system behavior
- use `spmm_sparse` to evaluate a sparse-dense matrix kernel with more reuse
