# FP16 GEMM + column softmax

The only configured benchmark target is
`test-vme-fp16-gemm-softmax-fused_M192_N64_K128`.
The matched P6 C128/T64/V192 workload uses 112 KiB TCDM for FP16 packed
operands and the FP32 logits/output buffer.

`gemm_logits_body` computes 16 vocabulary rows per block across 64 tokens.
It drains four rows at a time, adds the scalar bias for each row, updates
the running per-token maximum, and stores FP32 logits. The fused softmax
then reuses those maxima for degree-5 exponent approximation, column sum,
three reciprocal Newton steps, and in-place normalization. The approximation
clamps at -16 and uses six squarings. Register allocation, transformer math,
softmax scheduling and the public interface are consolidated in
`kernel/gemm.h`.

Build the configured target with:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  test-vme-fp16-gemm-softmax-fused_M192_N64_K128 -j4
```
