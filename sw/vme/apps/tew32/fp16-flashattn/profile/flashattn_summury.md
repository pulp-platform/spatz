# FP16 FlashAttention summary

The folder retains three workloads: `Tq16,T64,d64,dv16`,
`Tq64,T64,d64,dv64,Bc64`, and `Tq64,T256,d64,dv64,Bc64`.
The detailed diagram and interval trace below describe the third workload.
The rebuilt direct `Tq16` target passed RTL golden at 6,036 cycles on
2026-11-14; its earlier ELF measured 6,055 cycles. The reproducible
`Tq64,T64` and `Tq64,T256` measurements are 20,177 and 73,859 cycles
respectively.

## What each retained workload does

| Workload | Key step | Score/softmax schedule | Output schedule |
|---|---|---|---|
| `Tq16,T64,d64,dv16` | Four 16-key stages | Online maximum and denominator after each 16×16 QK score tile | Rescale the running 16×16 output, then add each 16×16 `P*V` contribution |
| `Tq64,T64,d64,dv64,Bc64` | One 64-key block | Compute a 64×64 score matrix, then one row maximum, exponential, denominator and FP16 probability pack | One 64×64 `P*V`, then normalize |
| `Tq64,T256,d64,dv64,Bc64` | Four 64-key blocks | Compute all four 64×64 score matrices first, then one maximum across all 256 keys per query | Keep each 32×32 output tile in OPE while accumulating all four `P*V` blocks; normalize after the tile stores |

### Direct 16×16 output (`Tq16,T64,d64,dv16`)

`kernel/flashattn_tq16.c` reuses the same 16 query rows over four successive
16-key slices. Each QK multiplies a 16×64 Q slice by a 64×16 K slice to
produce a 16×16 score tile. After draining that tile, VAU updates the running
row maximum and denominator and evaluates the exponential. In the retained
build, it narrows each probability pair to FP16 in scratch L1, reloads the
packed pair, and feeds it to the next 16×16 `P*V` operation. The
previous output is multiplied by the online rescale factor before that PV
contribution updates `mt4`. QK uses `mt0` and `mt8` in alternation, letting
the next score tile start while the current stage's softmax proceeds. The
kernel normalizes and stores the final 16×16 output after stage four.

### One 64-key block (`Tq64,T64,d64,dv64,Bc64`)

`main_tq64.c` with `FLASH_SINGLE_KEY_BLOCK=1` runs one 64×64 QK,
then `flash_softmax_single_block` computes the row maximum, exponential and
denominator and packs FP16 P into L1. Because this is the first and only key
block, the initial output is zero and there is no older output to rescale.
The OPE GEMM consumes packed P and V to produce the 64×64 FP32 output, which
is then divided by the row denominator.

This case uses the **same one-block attention formula** as the first online
update of the direct kernel, with a 64-key block instead of 16 keys. It is a
separate 64×64 implementation. It does not
run four copies of the direct 16×16 kernel: its QK/PV tile sizes, probability
storage, and output shape differ.

#### Four-stage cycle breakdown

[Open the Tq64/T64 four-stage diagram](tq64-key64-four-stage.html).

The clean production binary takes **20,177 cycles** and passes the FP32
golden check. Enabling the stage counters changes instruction placement and
raises the total to 20,638 cycles, so the following values describe the
instrumented schedule; they must not be added to the clean result.

| Stage | Execution unit and operations | Instrumented cycles |
|---|---|---:|
| 1. QK | OPE: 64×64 `Q*K^T`, including score stores | 2,979 |
| 2. Softmax and pack | VAU/VLSU: row max, O-POPE-equivalent exp, denominator and FP16 P stores | 12,746 |
| 3. PV | OPE: 64×64 `P*V` | 3,572 |
| 4. Normalize/store | VAU/VLSU: reciprocal, output scaling and stores | 897 |
| Accounted operation sum | | 20,194 |
| Call/timer/counter boundaries | | 444 |
| **Instrumented total** | | **20,638** |

Within stage 2, row maximum is 577 cycles, exponent/partial-sum/FP16 pack is
11,688 cycles, final denominator reduction/publish is 169 cycles, and 312
cycles are softmax counter-boundary/bookkeeping work. The counters perturb
the clean schedule by **461 cycles** overall.

### Four 64-key blocks (`Tq64,T256,d64,dv64,Bc64`)

`main_tq64.c` uses the fixed global-max4 schedule.
It first computes and retains four 64×64 QK score blocks. VAU finds one
maximum across all 256 keys for each query row, then exponentiates and packs
each block against that shared maximum. OPE keeps a 32×32 output tile live
while adding the PV contribution from blocks 0, 1, 2 and 3. It stores that
tile once, repeats for the other three output tiles, and finally normalizes.
The shared maximum avoids rescaling the output between blocks, at the cost of
holding all score blocks and delaying softmax until QK has finished. This is
why its stage order differs from the direct kernel's online 16-key pipeline.

## How the two kernels process FlashAttention

The workload has 64 query rows and 256 key/value rows. Both kernels split the
keys and values into four 64-row blocks, form FP32 attention scores from FP16
Q and K, evaluate the same degree-5 Taylor exponential with six squarings,
pack probabilities to FP16, accumulate FP32 `P*V`, and normalize the output.

- **O-POPE:** For each 64-key block in order, the O-POPE matrix engine computes
  `S_b = Q*K_b^T`. Spatz then performs the online row-maximum and denominator
  update, computes and packs `P_b`, and rescales the existing FP32 output.
  O-POPE immediately accumulates `P_b*V_b`. It repeats this QK → online
  softmax/pack → PV sequence four times, then normalizes the output.
- **VME global-max4:** OPE first computes all four QK blocks, using four matrix
  tiles (`mt0`, `mt4`, `mt8`, `mt12`) for each 64×64 score block. VAU then
  reduces one maximum per query row across all four blocks. For each block it
  subtracts that shared maximum, evaluates the exponential, adds to the row
  denominator, narrows to FP16, and stores packed P. During PV, OPE keeps one
  32×32 output tile resident while accumulating contributions from all four
  key/value blocks; it then stores that tile and advances to the next of four
  output tiles. VAU/OPE finally normalize and store the output.

The VME kernel uses a **global** maximum across the four blocks; the O-POPE
kernel updates its maximum **online** after each block. Both produce the
checked output for this workload. Their different phase order matters when
reading the cycle diagram: VME's per-block values are an allocation of work,
while its actual execution is all QK → global max → all exp/pack → tile-major
PV. The plot and counters below document the measured `global-max4` binary.

![Measured O-POPE and VME key-block stages, with VME PV expanded into four output tiles](flashattn_tile_stages.png)

[Open the SVG diagram](flashattn_tile_stages.svg). Each top or middle bar
shows one 64-key block, and each bottom bar shows one VME 32×32 output tile.
Bar widths are **instrumented cycle deltas within that row**, not simultaneous
wall-clock issue traces. VME's shared 3,030-cycle maximum is displayed once
in block 0; QK's four matrix tiles overlap and have no independently measured
per-tile durations. The exp/sum/pack timer combines those operations, so the
diagram does not invent separate cycle counts for them.
O-POPE's QK and PV counters time complete 64×64 GEMM jobs; no internal
O-POPE subtile counters were captured.

## Verified result

- Clean RTL: **73,859 cycles**, golden **PASS**.
- The 2026-11-14 Questa run improved the previous 79,966-cycle result by
  **6,107 cycles (7.64%)** and completed with `Errors: 0`, `Warnings: 0`.
- The exponential is unchanged from O-POPE: degree-5 Taylor followed by all
  six repeated squarings.  The 20,177-cycle single-block result also uses
  this exponential; its improvement came from normalize/store scheduling.

## VME versus optimized O-POPE

Each comparison uses identical dimensions, generated Q/K/V and initial
metadata, the same O-POPE-compatible exponential, and the same FP32 golden
output check. The optimized 85,869-cycle O-POPE result is the relevant T256
reference; the older 104,222-cycle checked-out baseline is not used here.

| Implementation | RTL cycles | Difference versus O-POPE | Result |
|---|---:|---:|---|
| Optimized O-POPE — `Tq64,T64` | 22,128 | baseline | PASS |
| **VME single-key-block — `Tq64,T64`** | **20,177** | **-1,951 (-8.82%)** | **PASS** |
| Optimized O-POPE — `Tq64,T256` | 85,869 | baseline | PASS |
| **VME global-max4 — `Tq64,T256`** | **73,859** | **-12,010 (-13.99%)** | **PASS** |

For T64, VME is **1,951 cycles faster** and gives a **1.0967x speedup**. For
T256, VME is **12,010 cycles faster** and gives a **1.1626x speedup**. VME and
O-POPE are simulated from their respective RTL builds, so these are
matched-workload comparisons rather than same-binary comparisons. Four
independent T64 calls are not used as a T256 result because they reset the
maximum, denominator and output state and therefore do not implement the
same attention problem.

## Memory traffic and VRF-residency opportunities

The timed inputs already reside in L1, so “memory-bound” here means the
L1/TCDM-to-VRF/OPE path, not external DRAM. For the T64 workload, Q, K and V
are 8 KiB each, the FP32 score matrix is 16 KiB, packed FP16 P is 8 KiB, and
the FP32 output is 16 KiB.

- **Score/softmax traffic:** max and exp read the 16 KiB score matrix in
  separate phases. Keeping the complete score matrix in VRF is impractical
  while the eight LMUL=4 groups are also needed by the four-way exponential.
  Tile streaming can reduce score spills, but it needs online-max recurrence
  or recomputation; VRF storage alone cannot remove the dependency.
- **Packed P producer to PV consumer:** this is the clearest avoidable L1
  round trip. The current kernel narrows and stores 8 KiB of P, then PV loads
  it again. A safe next step is pair/quartet ping-pong: once a probability
  register group is packed and its PV operands have been consumed, reuse that
  group for the next batch. Keeping all P in VRF at once is not feasible.
- **QK/PV operand reuse:** OPE already keeps each accumulator tile resident.
  Reusing a Q panel across K/output tiles and a V panel across query tiles can
  reduce repeated vector loads, provided VAU and OPE do not contend for the
  same VRF read ports.
- **Normalize/store:** the current epilogue reads and rewrites the 16 KiB
  output. Normalizing each output tile as it drains from OPE would avoid a
  full output round trip, but it requires a safe tile-to-VRF handoff after the
  denominator is final.
- **Metadata:** `m` and `l` together are only 512 bytes. Keeping them in VRF
  is straightforward and useful across key blocks, but its payoff is much
  smaller than eliminating score, packed-P, or output traffic.

VRF residency therefore helps most at **pack-P → PV** and **PV → normalize**
boundaries. It does not make stage 2 purely memory-bound: the unchanged
degree-5 exponential and six squarings still dominate its 11,688-cycle
exp/pack interval. Overfilling the VRF can also increase
`vrf_re_i && !vrf_rvalid_o`, so the useful design is incremental
produce/consume/release rather than retaining every intermediate.

## Instrumented stage breakdown

The stage profile predates the current address-elimination changes. Its
instrumented run was 81,613 cycles and its matching clean binary was 79,966
cycles. The counter deltas below remain exact for that historical
instrumented schedule, but must not be added to the current 73,859-cycle
result.

| Stage | Operations | Cycles |
|---|---|---:|
| 1. Four QK blocks | configure/zero four matrix tiles; load Q/K; `vtfmm`; interleaved score stores | 11,474 |
| 2. Global maximum | load all score rows; four-way `vfmax`; cross-block row reduction; store `m` | 3,030 |
| 3. Exp, denominator, pack P | subtract global max; degree-5 Taylor; six squares; row sum; FP32-to-FP16 narrowing; strided packed-P stores | 48,698 |
| 4. Tile-resident PV | zero each output tile once; accumulate four `P_b*V_b` blocks; store each tile once | 16,642 |
| Epilogue | two-step reciprocal; normalize output; final stores | 880 |
| Accounted stage sum | | 80,724 |
| Profile call/timer overhead | | 889 |
| **Profile total** | | **81,613** |

### Per key block

Global max is a shared operation and is charged to block 0 in the table.

| Block | QK | Shared max | Exp/sum/pack | PV MAC contribution |
|---:|---:|---:|---:|---:|
| 0 | 2,978 | 3,030 | 12,185 | 2,194 |
| 1 | 2,832 | 0 | 12,171 | 2,236 |
| 2 | 2,832 | 0 | 12,171 | 2,236 |
| 3 | 2,832 | 0 | 12,171 | 2,236 |

The four algorithmic stages for each block are therefore:

1. **QK:** configure/zero `mt0/mt4/mt8/mt12`, load Q/K pairs, issue the
   four-tile `vtfmm` stream, and drain the 64x64 score block.
2. **Maximum:** scan the block's scores into four row-max accumulators.  The
   accumulators are finalized once after block 3, so the measured 3,030-cycle
   shared reduction is charged to block 0 above rather than counted four
   times.
3. **Softmax/pack:** subtract the shared max, evaluate the degree-5
   polynomial and six squarings, accumulate the denominator, narrow FP32 to
   FP16, and write packed P.
4. **PV:** for each output tile, load that block's packed P and V pairs and
   issue `vtfmm` into the tile-resident accumulator.  The tile is not stored
   until all four blocks have contributed.

### Four output tiles during stage 4

The four block columns measure only each block's load/MAC issue interval.
`Tile overhead` contains one-time tile zero/configuration, the final tile
stores, drain latency, and profiling instructions.  `Function overhead` is
377 cycles outside the four measured tile intervals.

| Output tile | Block 0 | Block 1 | Block 2 | Block 3 | Tile overhead | Tile total |
|---:|---:|---:|---:|---:|---:|---:|
| 0 (`rows 0..31, cols 0..31`) | 568 | 559 | 559 | 559 | 1,846 | 4,091 |
| 1 (`rows 0..31, cols 32..63`) | 542 | 559 | 559 | 559 | 1,839 | 4,058 |
| 2 (`rows 32..63, cols 0..31`) | 542 | 559 | 559 | 559 | 1,839 | 4,058 |
| 3 (`rows 32..63, cols 32..63`) | 542 | 559 | 559 | 559 | 1,839 | 4,058 |

The QK kernel also updates `mt0`, `mt4`, `mt8`, and `mt12`, but those four
tiles execute as one overlapped 2x2 kernel.  Assigning independent wall-clock
times to them would double-count overlap, so QK is reported per key block.

## VFU and VRF evidence from the previous clean binary

The native Questa VCD was converted to an event-interval CSV; therefore the
trace itself does not add cycles. This retained trace describes the previous
79,966-cycle ELF and passed its golden check; a new VCD was not captured for
the current 73,859-cycle ELF.

| Region | Cycles in clean trace | VFU request/running/FPU-busy | Utilization |
|---|---:|---:|---:|
| QK/OPE | 11,557 | 0 | 0% |
| Global max + exp/pack | 51,789 | 47,977 | 92.64% |
| PV/OPE | 15,699 | 0 | 0% |
| Normalize | 921 | 910 | 98.81% |
| Whole timed kernel | 79,966 | 48,887 | 61.14% |

The zero VFU occupancy during QK and PV is intentional in this fastest
schedule: those phases execute on OPE, while vector loads use VLSU.  This
version does **not** overlap later-block QK/PV with softmax.  Its one-global-max
dependency requires all four QK blocks before exponentiation, and the
tile-resident PV loop keeps one output tile live while consuming all four
probability blocks.

VRF observations over the clean timed region:

- VFU read-request cycles: 48,159.
- OPE read-request cycles: 16,556.
- Shared operand requests denied by arbitration: **0 cycles**.
- `vfu_re & ~vfu_rvalid`: 2,303 cycles in 352 latency runs; maximum 26
  cycles.  The corresponding running-operation operand-wait count is 1,951
  cycles, maximum 25 cycles.
- Within global-max/exp/pack, VFU idle time is 3,812 cycles in 133 gaps;
  maximum 169 cycles.  Sixty gaps are 53 cycles and arise at score-row-group
  load/narrow/packed-store boundaries, where all eight m4 register groups are
  live and the next group cannot safely overwrite them.

## Artifacts

- Measured clean ELF for the global-max4 profile:
  `test-vme-fp16-flashattn-global-max4_TQ64_T256_D64_DV64`
- Latest disassembly: `test-vme-fp16-flashattn-global-max4_TQ64_T256_D64_DV64.s`
- Current ELF SHA-256:
  `332c00f6162c38a536e47f21ca0730c67dbb1ec3777d44a9280f1528a5f36d4a`
- Event-interval CSV: `global_max4_vfu_vrf_intervals.csv`
- Converter: `vfu_vrf_vcd_to_csv.awk`
