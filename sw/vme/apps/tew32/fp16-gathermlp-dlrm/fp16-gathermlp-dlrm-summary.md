# FP16 DLRM gather + MLP

Current workload (baseline `606abc00`): **R144, B127, L8, D64,
Dout128**. The generator preserves all 127 logical bags and pads one inert
column internally because the VME tile width is 16. The generator now also
matches the `606abc00` data representation: natural table rows and two packed
u8-index words per bag. Only the first 127 columns are checked.

The gather performs sequential FP16 sum pooling and writes the packed GEMM
operand directly. The fused path applies bias and ReLU before its only output
store.

The build writes generated test tensors to `data.h` in the build directory.

Build target:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  test-vme-fp16-gathermlp-dlrm-fused_R144_B127_D64_O128
```
