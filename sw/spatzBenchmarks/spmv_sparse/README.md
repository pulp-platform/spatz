# spmv_sparse

`spmv_sparse` is a separate benchmark from `spmv`.

It keeps the sparse structure in CSR-style `row_ptr` and `col_idx`, but stores
matrix data as a full dense row-major array. The cache-mode kernel then gathers
both matrix values and `x` values using the same 32-bit byte offsets.

The intended effect is:

- more cache misses than compressed-CSR `spmv`
- more in-flight miss pressure on the in-situ cache
- a deliberately bad but correct SPM baseline that DMA-moves dense row tiles

Implementation split:

- `main.c`
  - benchmark harness
  - cache-mode dispatch
  - SPM double-buffer baseline for dense row tiles
- `kernel/spmv_sparse.c`
  - vectorized cache-mode row kernels for `LMUL = 1/2/4/8`
- `script/gen_data.py`
  - dense-backed sparse data generator

This directory is self-contained. It is not wired into the shared benchmark
`CMakeLists.txt` here, because that file already has unrelated local changes.
