# FP16 residual-VQ GEMM, 1x4

Computes `C = C_initial + A * decode(Bmeta)` with FP32 accumulation.
`C_initial` is a full row-major 64x64 FP32 bias matrix, not a broadcast
column vector. Requires TE=16, VLEN=512, M=N=64, K=128/256/512.
The fused decode and 1x4 K-loop schedule come from fp16-vq-matmul/kernel/matmul.c.
The kernel has one `gemm_fp16_fp32_vq_1x4` entry point, marked noinline
and aligned to 64 bytes. `#if VQ_BIAS_VLE` selects VLE plus VTMV when
set to 1, or VTLE when set to 0. Both builds share the same compute schedule.

## Bias schedule

Both versions preload all four tiles' bias before loading A and gathering B.
The first-K-group unrolling and next-tile bias overlap experiment has been
reverted. The middle K loop starts at group 0, with the final group and
VTSE drain retaining the original schedule.

VTLE uses one FP32 matrix setup for all four tiles, then `vtle32` per row.
VLE uses the same setup followed by `vle32.v v8` and `vtmv.t.v` per row.
There are no extra VSET transitions between bias loads and VTMV batches.
Bias replaces VTZERO and is loaded once per output block.

Bias transfers use `msetmtypei 1, 2, 0`, TN=TM=16. Before each MAC
quartet, restore `(TE << 10) | (2 << 5) | 2`, vtype=0xC8, TN=16.
TSS bases are 0, 0x20000000, 0x40000000, 0x60000000; each bias row
advances TSS by one and its memory address by 256 bytes.

## Build and validation

The parent apps CMakeLists registers this app. Targets are
`fp16-vq-gemm-{vtle,vle}-1x4_M64_N64_K{128,256,512}` (with the normal test
prefix). Shapes that exceed configured TCDM are skipped. Each executable
loads the same initial bias outside timing and validates its own output.
The generator seeds the FP32 reference with bias before tk=2 accumulation;
this can round differently from adding bias in an epilogue.

The previous all-bias-preloaded K128 baseline passed RTL with zero maximum
absolute error: VTLE 29,126 cycles (14.06%), VLE 29,339 cycles (13.96%).

The reverted overlap experiment passed K128 Questa RTL with zero errors
and zero maximum absolute error:

| Schedule | Cycles | Utilization | Change from preload baseline |
| --- | ---: | ---: | ---: |
| VTLE unrolled / next-tile overlap | 29,145 | 14.05% | +19 |
| VLE gather-phase / four-row VTMV batches | 29,367 | 13.94% | +28 |

Those measurements did not show a speedup; they do not isolate cache misses
from configuration or scheduling overhead. Logs for that experiment are under
`hw/system/spatz_cluster/logs/vq-overlap-k128-fcyzb9/{vtle,vle}/transcript`.
