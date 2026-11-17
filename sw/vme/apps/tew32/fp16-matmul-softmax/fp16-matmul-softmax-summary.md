# FP16 matmul + column softmax, 1x4

Computes `softmax(A @ B, dim=1).T` in FP32, stored `[vocab=64][token=64]`.
Inputs are packed as B^T and A^T by the data generator, so the kernel
computes C^T=B^T*A^T without a runtime transpose. M=N=64, TE=16,
VLEN=512, K=128/256/512. The output layout is transposed relative to the
previous row-reduction matmul-softmax implementation.

Uses the same 1x4 FP16 MAC and ping-pong scheduling as fp16-gemm-softmax,
with no bias argument, allocation, DMA, scalar bias loads, or bias adds.
A grouped operand is consumed by all four accumulator tiles before reuse;
B alternates v16-v19 and v24-v27, preloading the next tile's operands.

The kernel keeps its softmax schedule and transformer arithmetic macros under
its application-local `kernel/` directory. Column max/sum use elementwise vector
operations, not horizontal reductions. Exp interleaves two independent
rows and alternates v8/v12 with v24/v28, loading the next pair before
computing the current pair. Normalization alternates v8/v12. No floating
point division, square root or reciprocal-estimate instruction is used by
the kernel.

The complete column statistics require all 64 vocabulary rows. The output
buffer is reused as L1 scratch for logits, then exp, then probabilities.
Only the final stores contain normalized output. One L1 operand buffer
is sufficient for the single complete-K job.

Targets: `test-vme-fp16-matmul-softmax-te16-rolled_M64_N64_K{128,256,512}`.
The FP32 PyTorch reference uses the same quantized FP16 operands and true
softmax before transposition. Every output is checked with tolerance
`2e-5 + 1e-3*abs(reference)`.

For example:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  run-rtl-test-vme-fp16-matmul-softmax-te16-rolled_M64_N64_K128 -j4
```
