# SpMV Sparse Data Generator

This directory contains the dense-backed sparse SpMV data generator for
`sw/spatzBenchmarks/spmv_sparse`.

## Purpose

The generated dataset keeps the logical sparse structure in `row_ptr` and
`col_idx`, but stores the matrix itself as a full dense row-major array.

That means the benchmark can:

- gather `x[col_idx[k]]`
- gather `A[row][col_idx[k]]`

This is intentionally worse for locality than the compressed `spmv` benchmark.

## Files

- `gen_data.py`: generates a `data/data_M_N_K_64.h` header
- `spmv_sparse.json`: default generator configuration

## Generated Data

The script emits:

- `spmv_sparse_row_ptr_dram`
- `spmv_sparse_col_idx_dram`
- `spmv_sparse_dense_dram`
- `spmv_sparse_x_dram`
- `spmv_sparse_result`
- `spmv_sparse_checksum`

## Basic Usage

```sh
python sw/spatzBenchmarks/spmv_sparse/script/gen_data.py \
  -c sw/spatzBenchmarks/spmv_sparse/script/spmv_sparse.json -v
```

## Options

- `--M`, `--N`, `--K`
  - Override matrix shape and requested nnz count.

- `--min-per-row`, `--max-per-row`
  - Bound `row_ptr[row + 1] - row_ptr[row]`.

- `--power-of-two`, `--no-power-of-two`
  - Control whether each row nnz count must be a power of two.
  - With `--power-of-two`, total emitted `K` may differ from requested `K`.

- `--sorted-cols`, `--unsorted-cols`
  - Control column order within each row.
  - `--unsorted-cols` is the default because it increases cache stress.

- `--debug-comment`, `--no-debug-comment`
  - Enable or disable a large human-readable dense matrix/vector comment.

- `--seed`
  - Select the RNG seed.

- `-v`, `--verbose`
  - Print output filename and row-nnz summary.

## Notes

- Only `prec=64` is supported.
- Arrays are emitted with 8-byte alignment attributes.
- The dense matrix contains zeros for structurally empty entries.
- The benchmark still uses `row_ptr` and `col_idx` to decide which entries are
  logically nonzero.
