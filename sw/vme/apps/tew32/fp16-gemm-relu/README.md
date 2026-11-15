# FP16 GEMM -> ReLU

This application reproduces O-POPE transformer P5 with **M=192, N=64,
K=128** (`C128 T64 Cout192`). It uses the repository-local copy of the
baseline dataset, including the complete FP32 accumulator seed `d_zinit`, and
checks against the same FP32 golden output.

The application retains only the fastest fused M192 schedule. Input conversion
to the VME layout is outside the measured interval. The kernel allocates the
same **114,688-byte** TCDM footprint (`W + X + Z`) as the reference workload
and uses no temporary output matrix.

| Implementation | RTL cycles | Result |
|---|---:|---|
| Optimized O-POPE | 16,614 | PASS |
| VME fused | **17,172** | **PASS** |

VME is 558 cycles (3.36%) slower on this plain GEMM-ReLU workload. Its
single-issue front end must deliver matrix operations, tile moves, RVV ReLU,
stores, and scalar address updates through the same instruction stream.

The retained schedule uses `(mt0, mt4)` and `(mt8, mt12)` as ping-pong pairs.
While one pair accumulates the next output block, two rows from the completed
pair move to the VRF, pass through ReLU, and store. The kernel issues four
independent `vtfmm.tvv` operations before replacing an operand group and
preloads the next K16 operands while the other pair drains. Fixed
`M192xN64xK128` bounds let the compiler eliminate the unused generic reduction
path and fold loop-common address expressions. Only the final resident pair
uses a standalone drain epilogue.

The cycle count covers only `gemm_relu_fused`; DMA, operand repacking, and the
golden-data check remain in `main.c` outside the measured interval.
