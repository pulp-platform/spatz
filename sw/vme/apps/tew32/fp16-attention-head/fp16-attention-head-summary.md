# FP16 Causal attention head

The active targets use `d=64`, `T=96`. The score GEMM is `M96 N96 K64`,
the causal mask is the complete FP32 accumulator and scale is folded into Q;
the PV GEMM is `M64 N96 K96`. Logical TCDM is exactly 116,736 bytes.

## Online-softmax experiment

`online-fused/` is a separate, checksum-validated implementation of the
standard running-statistics recurrence.  It processes three K/V blocks of 32
tokens and maintains, for every query column, the running maximum `m`,
denominator `l`, and unnormalised output `O`:

`m' = max(m, max(S_block))`, `l' = l * exp(m - m') + sum(exp(S_block - m'))`,
and `O' = O * exp(m - m') + V_block * exp(S_block - m')`.

The K32/P32 workspace is reused block by block: the current packed K panel is
overwritten by its FP16 unnormalised probability panel only after QK consumes
it, and the persistent `m/l` state moves into the already-consumed P0 panel.
This retains the same 116,736-byte logical TCDM footprint.  At `T=96, d=64`,
the extra three running-state updates and three short-K QK/PV calls outweigh
the saved full-score softmax pass, so exact fused remains the selected kernel.

This is a standalone VME application: `data/` documents the generated tensors,
`script/gen_data.py` constructs the input and FP32 reference, `kernel/` contains
the VME compute entry points, and `main.c` allocates TCDM, calls the kernels,
and checks output. The build uses `CMakeLists.txt`. Offline layout conversion
and DMA are outside the timed region; dependent nonlinear work stays timed.
Vector nonlinear operations use application-local transformer macros under
`kernel/`. The VME build has no dependency on the baseline checkout.
Generated test tensors are written into the build directory by
`script/gen_data.py`.

From the VME repository root:

```sh
cmake -S hw/system/spatz_cluster/sw -B hw/system/spatz_cluster/sw/build
cmake --build hw/system/spatz_cluster/sw/build --target test-vme-fp16-attention-head -j8
python3 sw/vme/apps/tew32/fp16-attention-head/script/run_rtl.py
```
