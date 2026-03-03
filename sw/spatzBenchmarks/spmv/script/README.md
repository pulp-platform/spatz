# SpMV Data Generator

This directory contains the CSR SpMV data generator for
`sw/spatzBenchmarks/spmv`.

## Files

- `gen_data.py`: generates a `data/data_M_N_K_64.h` header
- `spmv.json`: default generator configuration

## Generated Data

The script emits:

- `spmv_row_ptr_dram`
- `spmv_col_idx_dram`
- `spmv_val_dram`
- `spmv_x_dram`
- `spmv_result`
- `spmv_checksum`

The generated header is compatible with `spmv/main.c`.

## Basic Usage

Run with the default config:

```sh
python gen_data.py -c spmv.json -v
```

Run from the repo root:

```sh
python sw/spatzBenchmarks/spmv/script/gen_data.py \
  -c sw/spatzBenchmarks/spmv/script/spmv.json -v
```

## Option Reference

### Required input

- `-c`, `--cfg`
  - Path to the HJSON config file.
  - The script always starts from this config, then applies any CLI overrides.

### Size and sparsity

- `--M`
  - Number of rows in the sparse matrix.
  - Overrides `M` from the config file.

- `--N`
  - Number of columns in the sparse matrix.
  - Overrides `N` from the config file.

- `--K`
  - Requested number of non-zeros.
  - Overrides `K` from the config file.
  - If `power_of_two` is enabled, the final emitted `K` may be adjusted so that
    every row nnz count is a power of two.

- `--density`
  - Used only when `K` is not provided.
  - Computes the requested nnz count as `round(M * N * density)`.
  - Must be in `[0, 1]`.

- `--min-per-row`
  - Minimum number of non-zeros allowed in each row.
  - Useful when you want to avoid empty rows.
  - Must satisfy `M * min_per_row <= K`.

- `--min-nnz-per-row`
  - Alias of `--min-per-row`.
  - Added for clarity because this value controls
    `row_ptr[row + 1] - row_ptr[row]`.

- `--max-per-row`
  - Maximum number of non-zeros allowed in each row.
  - Useful to bound row length and keep the matrix structure controlled.
  - Must satisfy `K <= M * max_per_row` and `max_per_row <= N`.

### Precision

- `--prec`
  - Floating-point precision.
  - Currently only `64` is supported, because the current `spmv` kernel is
    double-precision only.

### Randomization

- `--seed`
  - Random seed for matrix values, vector values, and sparsity pattern.
  - Using the same config and seed gives reproducible output.

### NNZ shaping

- `--power-of-two`
  - Forces each row nnz count, `row_ptr[row + 1] - row_ptr[row]`, to be a power
    of two.
  - In this mode, the script chooses per-row nnz counts from `{1, 2, 4, 8, ...}`
    within the requested `min_per_row` and `max_per_row` bounds.
  - The final total `K` may therefore be adjusted from the requested `K`.

- `--no-power-of-two`
  - Disables the per-row power-of-two rule and allows arbitrary row nnz counts.
  - In this mode, the requested `K` is used directly.

- `power_of_two` in `spmv.json`
  - Config-file version of the same control.
  - Default is `true`.

### Readability / debug output

- `--debug-comment`
  - Adds a large human-readable comment block at the top of the generated
    header.
  - The comment shows:
    - the full dense matrix `A`
    - the dense input vector `x`
    - the dense golden output vector `y`

- `--no-debug-comment`
  - Disables that comment block.

- `debug_comment` in `spmv.json`
  - Config-file version of the same control.
  - Default is `false`.

### Verbose output

- `-v`, `--verbose`
  - Prints a summary to stdout after generation.
  - The summary includes:
    - output filename
    - `M`, `N`, `K`, `prec`, `seed`
    - row-nnz min/max/average

## Config File Fields

The default config file `spmv.json` supports these fields:

- `kernel`
  - Informational only.
  - Not used by the generator logic.

- `M`
  - Number of rows.

- `N`
  - Number of columns.

- `K`
  - Requested total number of non-zeros.
  - When `power_of_two` is enabled, the final emitted `K` may differ slightly
    so that each row nnz count is a power of two.

- `prec`
  - Precision, currently only `64`.

- `seed`
  - Random seed.

- `power_of_two`
  - Whether to force each row nnz count to be a power of two.

- `debug_comment`
  - Whether to emit the dense debug comment in the generated header.

- `min_per_row`
  - Minimum nnz per row.
  - Default is `8`.
  - With `power_of_two: true`, this minimum is applied to the per-row nnz
    counts, so the actual row count will be the smallest allowed power of two
    greater than or equal to this minimum.

- `max_per_row`
  - Maximum nnz per row.

## CLI Overrides

You can override config values on the command line:

```sh
python gen_data.py -c spmv.json --M 128 --N 128 --K 512 --max-per-row 16 -v
```

Disable power-of-two forcing:

```sh
python gen_data.py -c spmv.json --K 300 --no-power-of-two -v
```

Enable the dense debug comment:

```sh
python gen_data.py -c spmv.json --debug-comment -v
```

## Notes

- The current kernel is double-precision only.
- Generated arrays are emitted with 8-byte alignment attributes.
- The sparse matrix is stored in CSR form, but the optional debug comment shows
  the equivalent dense matrix for easier inspection.
- With `power_of_two: true`, the script enforces power-of-two row nnz counts,
  not power-of-two total `K`.

## Example

```sh
python sw/spatzBenchmarks/spmv/script/gen_data.py -c sw/spatzBenchmarks/spmv/script/spmv.json -v --debug-comment
```
