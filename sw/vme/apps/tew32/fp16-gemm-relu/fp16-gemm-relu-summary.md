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
| VME fused | **16,414** | **PASS** |

VME is 200 cycles (1.20%) faster on this plain GEMM-ReLU workload. Its
single-issue front end must deliver matrix operations, tile moves, RVV ReLU,
stores, and scalar address updates through the same instruction stream.

The retained schedule uses `(mt0, mt4)` and `(mt8, mt12)` as ping-pong pairs.
While one pair accumulates the next output block, two rows from the completed
pair move to the VRF, pass through ReLU, and store. The kernel issues four
independent `vtfmm.tvv` operations before replacing an operand group and
splits the next K16 operand preload around ReLU and stores while the other
pair drains. Successor address calculations are placed between independent
second-tile `vtfmm.tvv` operations instead of forming an exposed scalar
cluster after them. Fixed
`M192xN64xK128` bounds let the compiler eliminate the unused generic reduction
path and fold loop-common address expressions. Only the final resident pair
uses a standalone drain epilogue.

With `BENCHMARK=1`, the instrumented 16,502-cycle build reports 558 setup,
15,521 steady-state compute/drain, and 234 final-drain cycles (16,313 cycles
accounted inside the three measured regions). Counter reads perturb the ELF,
so the 16,414-cycle clean build is the performance result.

Selected OPE waveform counters show where the final schedule improves over
the preceding 16,702-cycle version:

| OPE-active-window metric | 16,702 schedule | Final | Change |
|---|---:|---:|---:|
| Window cycles | 16,551 | 16,269 | -282 |
| MAC fire | 12,480 | 12,480 | 0 |
| Tile-move busy | 1,574 | 1,774 | +200 |
| MAC or tile move busy | 13,312 | 13,400 | +88 |
| Neither MAC nor tile move | 3,239 | 2,869 | -370 |
| MAC frontend empty | 825 | 717 | -108 |
| MAC internal gap | 1,161 | 934 | -227 |
| MAC operand wait | 1,892 | 2,032 | +140 |

The arithmetic work is unchanged. Interleaving successor-address generation
with `vtfmm.tvv` increases useful MAC/tile-move overlap and reduces exposed
frontend and internal gaps enough to offset the additional operand-wait time.

The cycle count covers only `gemm_relu_fused`; DMA, operand repacking, and the
golden-data check remain in `main.c` outside the measured interval.
