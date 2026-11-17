# FP16 MLP block

64 channels, 64 tokens, 128 hidden channels. LayerNorm feeds an M128 N64 K64 VME GEMM; GELU feeds an M64 N64 K128 VME GEMM, with residual and bias in the output.

This is a standalone VME application: `data/` documents the generated tensors,
`script/gen_data.py` constructs the input and FP32 reference, `kernel/` contains
the VME compute entry points, and `main.c` allocates TCDM, calls the kernels,
and checks output. The build uses `CMakeLists.txt`. Offline layout conversion
and DMA are outside the timed region; dependent nonlinear work stays timed.
Vector nonlinear operations use application-local transformer macros under
`kernel/`. The VME build has no dependency on the baseline checkout.
Generated test tensors are written to `data.h` in the build directory.

From the VME repository root:

```sh
cmake -S hw/system/spatz_cluster/sw -B hw/system/spatz_cluster/sw/build
cmake --build hw/system/spatz_cluster/sw/build --target test-vme-fp16-mlp-block -j8
python3 sw/vme/apps/tew32/fp16-mlp-block/script/run_rtl.py
```

The workload folds the LayerNorm affine transform into W1
and its accumulator seed, uses the same two-step GELU reciprocal as O-POPE,
and has an exact 106,752-byte logical TCDM footprint. Fixed tile offsets are
preloaded outside the timed region into existing scratch storage, and GELU's
row-pair drains are coalesced before the next GEMM.
