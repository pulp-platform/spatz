# FP16 GEMM + column softmax, 1x4

Computes `softmax(A @ B + bias, dim=1).T`, stored as FP32
`[vocab=64][token=64]`. The generator packs `B.T` as the left matrix
operand and `A.T` as the right operand, so there is no runtime transpose.
Bias is one FP32 value per vocabulary entry, broadcast over tokens.
Supported K: 128, 256, 512, TE=16, VLEN=512.

The kernel retains the packed FP16 1x4 matmul shape and four accumulator
tiles. One e32,m4 load provides four K-pair vectors; A is consumed by all
four tiles before replacement. B alternates v16-v19 and v24-v27, with the
next tile's load issued ahead of the current tile's MAC quartet. The final
MAC group completes before two-row VTMV drains. Bias is added to completed
FP32 logits before scratch stores (epilogue addition, not accumulator seed).

A 1x4 block contains only 16 of the 64 vocabulary rows. All blocks must
finish before column softmax can normalize. C serves as in-place L1
scratch for logits and exponentials, followed by final normalized stores.
This implementation does not claim to eliminate intermediate VSE stores.
No extra logits array is allocated. One L1 operand buffer is used.

Softmax follows the baseline feature-major algorithm: each vector lane
owns one token; `vfmax.vv` and `vfadd.vv` accumulate across vocabulary
rows. No `vfred*` instructions are used. Shared `apps/include/ope_transformer.h` macros provide the math.
Exp uses the baseline clamped
Taylor polynomial and six squarings; reciprocal uses three Newton steps.
The -16 clamp intentionally matches baseline and approximates tiny tails.

Register schedule (e32,m4, VL=64):

- v0: column maximum, then reciprocal; v4: column sum.
- v8/v12: first pair of rows; v24/v28: alternate pair.
- v16/v20: independent polynomial temporaries.

The next pair is loaded before computing the current pair. Polynomial
instructions alternate between the two rows to space dependent operations.
The final pair is drained without an out-of-bounds prefetch. Normalization
alternates v8/v12 loads, multiplies, and final stores. Actual overlap and
performance remain dependent on the RTL scoreboard and functional units.

Targets: `test-vme-fp16-gemm-softmax-te16-rolled_M64_N64_K{128,256,512}`.
The reference uses PyTorch FP32 GEMM on quantized FP16 operands, broadcast
bias and true softmax, then transpose. Validation checks every output with
`2e-5 + 1e-3*abs(reference)` tolerance.

The public math macros are also documented in `../../include/README.md`. ReLU and rsqrt are
provided for later applications; only exp/reciprocal are exercised here.

## RTL validation

Questa RTL, M=N=64, one core, one L1 buffer:

| K | Cycles | Errors | Maximum absolute error |
|---|---:|---:|---:|
| 128 | 19,385 | 0 | 7.2122e-6 |
| 256 | 24,063 | 0 | 8.0466e-6 |
| 512 | 33,407 | 0 | 8.7619e-6 |

Logs: `hw/system/spatz_cluster/logs/gemm-softmax/k{128,256,512}.log`
from the repository root. Cycles cover the kernel (GEMM, bias, scratch
passes, softmax), excluding initial DMA and golden-reference validation.
The kernel disassembly contains no horizontal reductions or floating-point
divide/sqrt/reciprocal-estimate instructions. The linked printf library may
contain unused floating-format paths; the benchmark prints only integers
and FP32 bit patterns.
