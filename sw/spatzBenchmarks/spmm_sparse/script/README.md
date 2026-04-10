# SpMM Sparse Data Generator

This directory contains the dense-backed sparse SpMM data generator for
`sw/spatzBenchmarks/spmm_sparse`.

## Purpose

The generated dataset keeps the logical sparse structure of the left matrix `A`
in `row_ptr` and `col_idx`, but stores `A` itself as a full dense row-major
array.

The benchmark then computes:

- `C = A * B`
- sparse structure comes from `row_ptr` and `col_idx`
- dense data comes from `dense_a[row][col]` and dense `B[col][n]`

This is intentionally worse for locality than a compressed sparse-value format,
because each structural nonzero jumps to a different row of `B` and to a sparse
position inside the dense-backed `A` row.

## Files

- `gen_data.py`: generates `data/data_M_N_K_64.h`
- `spmm_sparse.json`: default generator configuration

## Generated Data

The script emits:

- `spmm_sparse_l`
- `spmm_sparse_row_ptr_dram`
- `spmm_sparse_col_idx_dram`
- `spmm_sparse_dense_a_dram`
- `spmm_sparse_b_dram`
- `spmm_sparse_result`
- `spmm_sparse_checksum`

## Basic Usage

```sh
python sw/spatzBenchmarks/spmm_sparse/script/gen_data.py \
  -c sw/spatzBenchmarks/spmm_sparse/script/spmm_sparse.json -v
```

## Options

- `--M`
  - Number of rows in sparse `A` and output `C`.

- `--N`
  - Number of columns in dense `B` and output `C`.

- `--K`
  - Inner dimension. `A` is `M x K`, `B` is `K x N`.

- `--nnz`
  - Requested total number of structural nonzeros in `A`.

- `--density`
  - Alternative to `--nnz`. Uses `M * K * density` when `nnz` is not set.

- `--min-per-row`, `--max-per-row`
  - Lower and upper bounds for `row_ptr[row + 1] - row_ptr[row]`.

- `--power-of-two`, `--no-power-of-two`
  - Control whether each row nnz count must be a power of two.
  - When enabled, the actual emitted total nnz may differ slightly from the
    requested `nnz` if that is the closest feasible per-row assignment.

- `--sorted-cols`, `--unsorted-cols`
  - Sort or randomize column order inside each row.
  - `--unsorted-cols` increases cache stress.

- `--debug-comment`, `--no-debug-comment`
  - Enable or disable a large human-readable comment showing dense `A`, `B`,
    and golden `C`.

- `--seed`
  - RNG seed.

- `-v`, `--verbose`
  - Print output path and per-row nnz counts.

## Notes

- Only `prec=64` is supported.
- All emitted arrays use 8-byte alignment attributes.
- `dense_a` contains zeros for structurally empty entries.
- The benchmark still uses `row_ptr` and `col_idx` to decide which entries are
  logically nonzero.
