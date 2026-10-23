# FP16 residual-VQ MatMul

Computes FP32 C = A * decode(Bidx), without a bias addend.
Requires TE=16, VLEN=512, M=N=64 and K=128/256/512.
No instruction-buffer setting is changed by this app.

## Preprocessing

Two 256x8 FP16 codebooks are added and scaled, with FP16 rounding after
each operation. Standard RVV indexed loads, addition and multiplication
use e16,m4, VL=128. Scalar code expands the compressed indices to byte
offsets. Decoded B is materialized in TCDM once and reused across M blocks;
this is not a fused decode/compute pipeline. Timing includes this decode.
Packing is even K in the upper halfword and odd K in the lower halfword.

## Compute schedule

The four tiles cover a 16x64 output block. Grouped operand loads use
e32,m4, VL=64; every load phase restores msetmtype and msettn before
matrix operations. Matrix compute uses e16, tk=2, FP32 accumulation.
vtse32 follows the reference FP16 VTSE kernel's matrix configuration.

The middle loop handles K-16 elements. The final 16 K elements are written
explicitly as eight tk=2 vtfmm instructions per tile:

- Finish mt0.
- Compute mt4 while storing all 16 rows of mt0.
- Compute mt8 while storing all 16 rows of mt4.
- Compute mt12 while storing all 16 rows of mt8.
- Store mt12 during the next M block's first four mt0 instructions.
  Only then clear mt12 for reuse. Skip the already computed mt0 K group.
- Drain the last block's mt12 separately.

The tail retains A in v0-v3/v8-v11 and B in v16-v19/v24-v27.
The companion registers used implicitly by tk=2 remain available.
Each vtse advances its destination by 256 bytes and its TSS by one.

## Validation

Run script/check_schedule.py for an independent host model of decode
packing, K coverage and output-address coverage. This is not an RTL test.
CMake skips shapes whose buffers do not fit SNRT_TCDM_SIZE. Buffers are
single-resident, not double buffered; the budget includes packed B and
768 bytes of decode scratch, but does not represent total runtime usage.
