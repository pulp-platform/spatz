# FP16 GEMM -> GELU

This application reproduces O-POPE transformer P2 with **M=192, N=64,
K=128** (`C128 T64 Cout192`). It uses the repository-local baseline dataset,
the complete FP32 accumulator seed `d_zinit`, and the same FP32 golden output.

Both variants use the same FP16-to-FP32 GEMM and GELU approximation, including
the same exponential and reciprocal instruction sequences:

- `non-fused/` stores GEMM output and then runs a separate RVV load/GELU/store
  pass.
- `fused/` applies that identical GELU sequence to completed accumulator tiles
  in vector registers immediately before their final stores.

There is no reduced-precision or reduced-iteration shortcut in the fused
version. Dataset layout conversion is outside timing. Both variants use the
same **114,688-byte** TCDM footprint (`W + X + Z`).

Build either configured variant with, for example:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  test-vme-fp16-gemm-gelu-fused_M192_N64_K128 -j4
```
