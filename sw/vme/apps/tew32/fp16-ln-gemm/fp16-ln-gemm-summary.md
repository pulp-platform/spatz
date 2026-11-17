# FP16 LayerNorm -> GEMM

This application reproduces O-POPE transformer P1 with **M=176 output
channels, N=64 tokens, K=96 input channels** (`C96 T64 Cout176`). The local
`data/data_c96_t64_o176.h` is the baseline dataset and supplies the same input,
folded weights/bias, accumulator seed, and FP32 golden output.

For every token, LayerNorm computes

`mean = sum(x) / K`, `variance = sum((x - mean)^2) / K`, and
`normalized = (x - mean) * rsqrt(variance + 1e-5)`.

The baseline generator folds gamma/beta into the GEMM weights and bias. The
cluster has no legal scalar divide or square-root instruction, so the kernel
uses an explicit reciprocal for supported K values and the shared RVV inverse
square-root sequence. It then packs normalized FP16 operands and performs an
FP16-to-FP32 GEMM.

- `non-fused/` calls the LayerNorm/pack stage and GEMM stage separately.
- `fused/` places both stages in one timed kernel call and keeps their register
  schedule contiguous. It does not change the LayerNorm mathematics.

Both variants use the same **115,712-byte** TCDM footprint as the application
baseline. The M160 body uses the 2x2 VTSE kernel; an M16 1x4 tail preserves
the exact M176 footprint. A short main-to-tail drain is required before the
tail reuses `mt0..mt12`; without it, the final eight values of row 160 can
remain at their accumulator seed.

The M160 GEMM body seeds accumulators from the existing broadcast bias and the
M16 tail uses `vtzero` plus a late scalar bias.

Build either configured variant with, for example:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  test-vme-fp16-ln-gemm-fused_M176_N64_K96 -j4
```
