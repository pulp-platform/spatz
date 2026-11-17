# FP16 convolution + bias + ReLU

Current workload (baseline `606abc00`): **Cin32, H16, W16, Cout64, 3x3
valid convolution**.

The implementation matches the current no-im2col algorithm. Input is stored
as channel-pair planes. Each output block uses four 16x16 tiles. The tiles
are seeded once from the FP32 bias-initialized output, then accumulate all
nine filter taps and all 16 packed input-channel pairs per tap without draining
between taps. The A pointer is shifted by `ky * W + kx`; weights are packed
per tap. The fused variant applies ReLU when draining the completed tiles;
the non-fused variant applies ReLU in a separate pass. Only the 14x14 valid
positions are checked.

Input, weights, bias initialization and golden are word-identical to the
reference dataset. The TCDM footprint is 119,040 bytes.

Build target:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target \
  test-vme-fp16-conv-relu-fused_cin32_h16_w16_cout64
```
