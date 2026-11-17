# FP16 GCN gather + MLP

Current workload (baseline `606abc00`): **127 nodes, self + 7 neighbours,
D64, Dout128**. The logical 127 columns are padded by one inert VME tile lane.
The generator uses the same natural table rows and packed u8-index words as
O-POPE commit `606abc00`.

Gather uses sequential FP16 accumulation and the FP16 `1/8` mean scale, then
writes the packed GEMM operand directly. DLRM and GCN share the gather/pack and
GEMM implementation; GCN adds the mean scale.

The build writes generated test tensors to `data.h` in the build directory.

Build target:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  test-vme-fp16-gathermlp-gcn-fused_N127_D64_O128
```
