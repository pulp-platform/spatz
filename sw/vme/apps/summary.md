# VME application advantages

VME extends the RVV datapath with matrix-tile accumulators while retaining
direct movement between matrix tiles and the vector register file (VRF).  The
applications in this directory use that coupling for three recurring
advantages.

## 1. More resident tiles for staggered ping-pong

The kernels can keep four independent accumulator groups resident in
`mt0`, `mt4`, `mt8`, and `mt12`.  MatMul-ReLU uses them as two ping-pong
pairs: `(mt0, mt4)` and `(mt8, mt12)`.  While one pair accumulates the next
output block, rows from the previously completed pair are moved to the VRF,
passed through ReLU, and stored.  The roles alternate at the next block.

This matters because output draining is not free.  Tile moves, vector-mode
setup, ReLU, vector stores, and address updates all add instructions after the
matrix reduction.  In the staggered steady state, those instructions are
placed between independent `vtfmm.tvv` operations for the active pair.  All
intermediate blocks therefore overlap most of their drain/post-processing
work with useful matrix execution; only the final resident pair requires a
standalone epilogue.

The four tiles consequently provide scheduling capacity, not just additional
accumulator storage.  A tile is not overwritten until its post-processing has
finished, yet the matrix unit can continue on the other pair.  The same idea
applies to other fused kernels: completed tiles can drain while independent
tiles compute, reducing exposed store latency and separate prologue/epilogue
time.

VME still cannot beat O-POPE on the retained plain MatMul-ReLU workload.  Its
single-issue front end can fetch and issue only one instruction per cycle, so
matrix setup, tile moves, RVV ReLU, stores, and scalar address updates compete
with the instructions that feed the matrix MAC queue.  O-POPE's more
specialized data path exposes less of that instruction-delivery overhead.

The staggered strategy nevertheless makes VME competitive: it places the
unavoidable move, ReLU, store, and address instructions into otherwise idle
windows between independent matrix operations.  The current
`M192xN64xK128` VME kernel takes 17,172 cycles versus 16,614 cycles for
optimized O-POPE, a remaining gap of 558 cycles (3.36%).  This result is
evidence that four-tile ping-pong can hide most of VME's flexible
post-processing overhead, even though it does not remove the single-issue
front-end limit.

## 2. RVV and VME fusion

RVV handles irregular preparation and post-processing, while VME performs the
dense reductions.  Because both sides exchange operands through the VRF, an
application can alternate vector loads, vector arithmetic, `vtfmm.tvv`, tile
moves, and stores without materializing every intermediate matrix in memory.

VQ-MatMul and DQ-MatMul are representative examples.  RVV loads codebook
vectors selected by the quantization metadata and reconstructs an FP16 weight
group with `vfadd.vv`.  The reconstructed vectors then feed `vtfmm.tvv`
directly from the VRF.  Address generation and decoding of the successor
weight group are interleaved with the current matrix reduction.  A decoded
weight is written to the L1 `cacheW` workspace only when it will be reused by
another M panel; decoding and matrix multiplication otherwise remain one
fused kernel rather than a decode kernel followed by a separate GEMM.

`cacheW` is a reuse optimization with an L1-capacity cost.  The compressed
indices and codebooks remain resident, and the application must additionally
reserve an L1 workspace for decoded FP16 weights in the layout consumed by
VME.  This is analogous to the O-POPE VQ/DQ flow, which reserves a decoded
`wd[N][K]` buffer before launching GEMM.  The VME difference is scheduling:
the first M-panel producer decodes each group directly into the current
`vtfmm.tvv` stream and writes `cacheW` once; later M panels reuse that layout
without decoding again.  For a small M dimension with no reuse, direct decode
can avoid the cache store and workspace.  For multiple M panels, `cacheW`
trades L1 footprint for less repeated decode work.

This fusion provides three practical benefits:

- it removes unnecessary decoded-weight round trips through memory;
- it overlaps lookup, vector addition, matrix reduction, and output draining;
- it lets RVV implement operations that are not naturally matrix operations
  without forcing a boundary between processing stages.

FlashAttention's Tq64 global-maximum schedule is another RVV--VME fusion
benefit.  `mt0`, `mt4`, `mt8`, and `mt12` compute the four 32x32 subtiles of a
64x64 QK score block.  After the four 64-key blocks have been drained, RVV
uses four independent `e32,m4` register groups to load and reduce four score
rows at a time.  Each vector contains the scores of all 64 queries for one
key, so the grouped scan computes one maximum per query across all 256 keys.
Every probability block can then use the same maximum and contribute directly
to the same resident PV accumulators.  This avoids the repeated output
rescaling and four-output merge required by an online-max schedule.

## 3. In-flight transpose through tile-column moves

VME does not require every layout conversion to become a memory transpose.
`vtmv.v.t` can extract a matrix-tile column directly into an RVV vector, and
`vtmv.t.v` can insert a processed vector back into a tile.  Selecting columns
in sequence changes the logical view of a matrix while the data is in flight
between the matrix tiles and the VRF.

FlashAttention initially appears difficult to fuse on VME.  QK naturally
forms the score matrix `S = Q K^T`, whereas the following matrix reduction is
most conveniently expressed as `O^T = V^T P^T`.  Without a way to change the
orientation of `S` and its softmax result `P`, the kernel would have to write
the intermediate matrix, transpose it in memory, and read it again before PV.

The transpose instead happens while the score tile moves to the VRF.  A score
tile column contains one key's score for every query; `vtmv.v.t` turns that
column into the corresponding row of `S^T`, with queries in the vector lanes.
RVV applies softmax to that vector and packs the result directly as `P^T`, so
it can immediately become the operand of `V^T P^T`.  There is no standalone
transpose kernel or intermediate transpose round trip between QK, softmax,
and PV.

O-POPE instead relies on operand layouts transposed offline before its GEMM
calls.  VME's tile-column move performs the necessary intermediate transpose
in flight, which makes the fused QK--softmax--PV pipeline possible without an
offline linear-algebra transformation of `S` or `P`.

The matched Tq64 FlashAttention results show the end-to-end benefit available
from this tighter layout and stage fusion:

| Workload | Optimized O-POPE | VME | Speedup | Cycle reduction |
|---|---:|---:|---:|---:|
| `Tq64,T256,d64,dv64,Bc64` | 85,869 | 70,127 | 1.224x | 18.33% |

This is a **1.224x end-to-end speedup** and an **18.33% cycle reduction** for
Tq64 FlashAttention.  In-flight transpose enables the fusion, while the total
result also includes the four-tile global-max/PV schedule and RVV vector
grouping; the full speedup should not be attributed to transpose alone.

The current VME benchmark still pre-packs external Q/K/V operands offline for
the matrix engine.  The eliminated transformation is the standalone
*intermediate* score/probability transpose.  Removing all input pre-packing
would require a natural-layout input path and is not demonstrated by the
current measurements.

In short, VME's advantage is not only programmability and flexibility.  Its
shared VRF interface, multiple resident matrix tiles, and column-granular tile
moves allow whole producer/consumer pipelines to stay on chip and overlap.
