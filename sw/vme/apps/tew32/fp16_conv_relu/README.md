# FP16 3x3 convolution + bias + ReLU (O-POPE conv: Cin=32, H=W=16, Cout=64)

One GeMM per filter tap on the shifted input (no im2col), 2x1 staggered blocks, bias K-pair, only the result rows computed, ReLU at the end. Kernel and data: `script/gen_conv.py`.

Build and run (from `hw/system/spatz_cluster`; the checks after the measured region can need more than 60 s):

```
(cd sw/build && cmake . && make test-vme-fp16-conv_Cin32_H16_W16_Cout64)
bin/spatz_cluster.vsim sw/build/vme/apps/test-vme-fp16-conv_Cin32_H16_W16_Cout64
```

Results against O-POPE: `runtime/opope_summary.md` (repository root).
