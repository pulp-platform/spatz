# Spatz OPE Architecture and Dataflow

This document describes the current Outer Product Engine (OPE) RTL. It is not an ISA specification or a simulation sign-off.
The OPE supports the RISC-V Vector Matrix Extension, excluding memory instructions and matrix-configuration instructions.
Its datapath contains a distributed `CE×CE` MAC array and a distributed `CE×CE` accumulator array. The floating-point
arithmetic is collectively referred to as the FPU and contains both FMA and SDOTP datapaths; the accumulator stores their
results and provides the feedback addend for subsequent operations.

The top-level module is `spatz_ope.sv`; floating-point arithmetic and storage are implemented by the FPU datapaths and
`opope_accumulator.sv`.
Shared request and tile types come from `spatz_pkg` and `rvv_pkg`; floating-point formats come from `fpnew_pkg`.

## 1. Architecture

```text
Snitch instruction
        |
        v
Decoder + Controller
  vtype/mtype snapshot, instruction ID, vector and matrix scoreboards
        |
        +-- OPE request -------------------------------------------+
        |       |                                                 |
        |       +--> MAC FIFO --> current/next --> VRF read [0:1]  |
        |       |                                  |              |
        |       |                                  v              |
        |       |                     CE x CE FPU pipeline        |
        |       |                      ^           |              |
        |       |                      +-- forward-+              |
        |       |                      ^           | drain/write  |
        |       |                      |           v              |
        |       +--> TV spill --> VRF read [2] --> VRF-side port   |
        |       +--> VT spill <-- VRF-side port --> VRF write      |
        |       +--> Clean spill --> zero metadata / flush         |
        |                                                         |
        +-- LSU request --> VLSU <--> VLSU-side accumulator port
                              |
                              v
                           L1 TCDM

FPU pipeline <--> FPU-side accumulator port

MAC/VT/TV/Clean completion --> response buffers --> arbiter --> Controller
VLSU completion --------------------------------------------> Controller
```

`matrix_enable_i[id]` grants execution after tile dependencies are resolved. Queue ready only indicates available capacity;
it does not indicate that the source tile is ready. Vector operands remain protected by the vector scoreboard and VRF handshakes.

### Operand and Operation Queues

| Path | Implementation | Capacity |
| --- | --- | --- |
| OPE MAC request | Non-fall-through `stream_fifo` | 4 entries |
| OPE TV, VT, Clean requests | One `spill_register` per path, Bypass=0 | Up to 2 entries each |
| OPE MAC, TV, VT, Clean responses | One `spill_register` per path | Up to 2 entries each |
| Double-bandwidth VLSU operation request | Non-fall-through `stream_fifo` | 4 entries |

These request queues hold `spatz_req_t` metadata, not complete A/B vectors. Vector operands are read during execution.
TV additionally has a one-word data latch for a VRF word waiting for accumulator access.
The common_cells spill register contains A/B storage and is not a depth-1 FIFO; it has no configurable DEPTH parameter.
The VLSU uses its depth-4 `i_operation_queue`; memory tags, response queues and operand data buffers have separate capacities.

The MAC FIFO feeds current/next operation metadata. Only the current operation owns the beat/reduction counters.
`spatz_req_ready_o` currently combines all four input readies, so a full queue can block acceptance of other operation classes.
Internal request selection prioritizes Clean, VT, TV, then MAC; active datapaths still obey their own handshakes.

### Current Configuration

| Parameter | Value | Meaning |
| --- | --- | --- |
| `TE = TileEdge` | 16 | Maximum logical tile edge in the current configuration |
| `CE = OPEComputeEdge` | 8 | Outer-product compute edge per beat |
| `EnableTEW16` | 0 | Elaborate the same-format FP16 FMA datapath when set |
| `AccElemWidth` | 32 bits | Fixed physical accumulator word width |
| `VRFWordWidth` | 256 bits | Width of each VRF read/write port |
| `NumPipeRegs` | 4 | Arithmetic pipeline register stages |
| `SpatialBeats = (TE / CE)^2` | 4 | Spatial beats for a full tile |
| `AccDepth` | 16 | Accumulator entries per compute lane |
| `TileDataWidth` | 512 bits | OPE/VLSU tile-row data width |

Each CE x CE lane always has an FP32 FMA and two mixed-format SDOTP wrappers for standard FP16 and FP16ALT.
The same-format FP16 FMA is generated only when `EnableTEW16=1`; it is absent by default.
Runtime SEW, TEW and ALT-format state select exactly one result path for an operation.
Physical storage is `CE * CE * AccDepth * AccElemWidth / 8`, currently 4096 bytes.
TEW8 is supported for byte storage, moves and clearing only. No FP8 MAC is enabled.

## 2. Four Operation Classes

### MAC: Vector Outer Product into a Tile

`vtfmm` addresses its destination using `mtd`, not TSS:

```text
C[i,j] += sum(A[i,k] * B[k,j]),  i < tm, j < tn, k < tk
```

1. The request enters the MAC FIFO and waits for matrix dependencies before entering current/next state.
2. Its captured mtype/vtype determines tm, tn, tk, SEW and TEW. The matrix extent is bounded by TE, not replaced by VL or LMUL.
3. Source register IDs, K-row stride, reduction index and spatial beat generate VRF addresses.
   Read ports [0:1] supply the two operand vectors for row/column broadcast into the FMA array.
4. Only `mac_fire` advances beat/reduction state. Waiting for operands or FMA readiness must not advance it.
5. The lane mask prevents writes outside tm/tn. Issuing the final beat produces MAC completion.

The work per instruction is `ceil(tm / CE) * ceil(tn / CE) * tk` beats.
For tm=tn=32 and CE=8, tk=1 uses 16 beats and tk=2 uses 32. This is issue work, not end-to-end latency under stalls.

**Multiple-beat benefit:** a CE x CE array is reused across a larger TE x TE tile and across K reduction steps.
The hardware does not need one FMA per element of the entire tile, and software does not issue a separate instruction for every rank-1 step.

**Active-lane-mask benefit:** each beat enables FMA valid and accumulator writes only for row < tm and column < tn.
Fringe tiles do not require padded arithmetic to protect the result, and inactive elements are not overwritten.
This avoids valid work on inactive lanes but does not imply every combinational node is clock-gated.
Real power and utilization still depend on operand delivery, stores and pipeline stalls.

Each source supplies one VRF word per beat. The configuration must satisfy
`CE * SEW <= VRFWordWidth` for FMA and `CE * TEW <= VRFWordWidth` for packed SDOTP.
Increasing CE alone does not add a multiword operand collector.

### Different SEW and TEW

For same-width FMA, SEW controls operand extraction and K-row stride. For widening
SDOTP, each base-vector element contains packed K lanes and has width TEW.
TEW also controls accumulation precision and the tile byte footprint.
`mtype.mtwiden` encodes a multiplier: 01 means 1, 10 means 2, and 11 means 4; 00 invalidates matrix state.
The element width is `TEW = SEW * multiplier`, not SEW times the raw encoded field.

| SEW / TEW | Current behavior |
| --- | --- |
| 8 / 8 | Raw-byte tile load/store, TV/VT and VTZERO; no FP8 MAC |
| 16 / 16 | Same-format FP16 FMA when `EnableTEW16=1`; unsupported by the default OPE instance |
| 16 / 32 | FP16 or FP16ALT source and FP32 destination through the corresponding SDOTP wrapper |
| 32 / 32 | Native FP32 inputs and accumulation |

An `opope_fma` accepts operands only in its configured `FpFormat`; it performs no FP16-to-FP32 type conversion.
When source and destination formats differ, the OPE enables either the FP16-to-FP32 or FP16ALT-to-FP32
`opope_sdotp_wrapper`. The FP32 FMA is enabled only for a native FP32 source and FP32 destination.
The mixed-format result and accumulator remain FP32. Tile memory transfers use the 32-bit TEW layout.
For FP16-to-FP32 with TK=2, each input word is `{even_K_fp16, odd_K_fp16}`.
OPE maps the high halfword to lane 0 and the low halfword to lane 1, computing
`C += A_even * B_even + A_odd * B_odd`. No implicit-register operand fetch or
operand stream-register buffer is needed. A/B each supply one 256-bit VRF word
per accepted beat (eight packed elements). A 16x16 tile on 8x8 CEs takes four
spatial issue beats, independent of TK; pipeline latency and stalls are additional.
The controller still tracks implicit row-stride register dependencies.
The packed TE16 software keeps matrix arithmetic configured as
SEW16/TWIDEN2/TK2. Store-afterK and store-interleave execute one `msetmtype`
followed by `msettn 16` and then interleave individual VLE32 operations with
VTFMM instructions. VLE32 uses its instruction EEW for the memory transfer, so
it transfers sixteen packed 32-bit elements while matrix VSEW remains 16.

The vsetvl variant deliberately groups four packed rows in one vector load.
It executes `vsetvli e32,m4`, which produces VL=64 and lets one VLE32 fill
v0-v3, v8-v11, v16-v19 or v24-v27. `msetmtype` then restores the unchanged
SEW16/TWIDEN2/TK2 state. The following `msettn` receives 64, but the controller
clamps TN and VL to the configured ETE=16.
TV/VT are bit transfers, not floating-point conversions, and require SEW==TEW.
To move a FP32 tile through the VRF, establish valid SEW32/TEW32 matrix state; `vsetvli` alone is insufficient because it clears mtype.
Complete old-format work appropriately before changing configuration; do not reinterpret slices while old results remain resident.
The controller currently rejects SEW8 MAC and SEW8 widening rather than issuing into an unimplemented arithmetic path.

### VT: Tile to VRF, `vtmv.v.t`

TSS selects the tile, row/column direction and index. VT reads the accumulator, selects the TEW-dependent byte slice,
and packs it into the VRF write word with byte enables. The controller requires TEW==SEW.

The active extent considers tm/tn, VL, vstart and register-group capacity. The word counter can cross physical vector registers
and valid tile lines; a move is not restricted to one physical VRF word.
VT completes after the last destination word is accepted, or immediately for an empty operation.
If the latest source value is resident in the FMA pipeline, it must drain before accumulator access.
An earlier MAC response alone does not prove the storage already contains the latest value.

### TV: VRF to Tile, `vtmv.t.v`

TV reads VRF port [2] and maps its data into tile rows/columns using TSS and its word counter.
`tv_data_latched_q` retains an available VRF word when accumulator access stalls.
The TEW/tile-ID mux and read-modify-write path preserve bytes and elements outside the selected update.
TV uses the VRF-side accumulator read/write interface. It can overlap an FMA writeback or VLSU transfer to another tile,
but VT and TV still share the VRF-side grouped read path and same-target dependencies must serialize.
TV completes after the final accumulator word is written; TEW==SEW is required here too.

### Clean: `vtzero` and `vtdiscard`

The decoder supplies `is_zero_tile` and `is_discard`.
VTZERO addresses mtd and waits for its MAC/resident dependencies before updating `acc_zero_q`.
This is **lazy zero**: all spatial beats of the selected logical tile are marked zero, independently of current tm/tn.
Reads return zero for marked bytes without immediately writing every physical word. Subsequent writes clear the corresponding zero metadata.

For TEW16, clearing mt0 preserves mt2. For TEW8, clearing mt1 marks only byte 1 and preserves mt0, mt2 and mt3.
For example, physical word `0x44332211` yields mt0/1/2/3 values 11/22/33/44 under TEW8.
After `vtzero mt1`, reads yield 11/00/33/44 even if the physical word has not changed.
Writing AA to mt1 updates that byte and clears its zero flag.

Partial TV/VTLE writes first materialize selected-slice lazy zeros in their read-modify-write data, then replace valid elements.
This prevents old values in untouched elements from reappearing when metadata is cleared.

VTDISCARD waits for the OPE clean-idle conditions and asserts global `acc_flush`, physically clearing all accumulator memory
and resetting zero metadata. Reset also physically clears memory.
When draining for VTZERO or discard, superseded resident results can be consumed without writing them back.
VTZERO initializes one logical tile; VTDISCARD clears all tile state and must not be used as a single-byte-tile clear.

## 3. Arithmetic Pipeline, Forwarding and Switching

### Data and Tags

The FMA and SDOTP datapaths have four distributed register stages. Their ready signals are aggregated into `fma_pipe_ready`;
disabled or unselected paths remain ready and do not produce a valid result.
`fma_pipe_advance` advances the selected computation, its valid pipeline and tag state together. Tags contain valid, tile ID,
TEW, spatial beat, lane mask and `write_acc`. Arithmetic results do not need an instruction ID because MAC completion is generated at issue. Request and response queues still retain IDs
because one core can have multiple outstanding instructions.

Each FMA tag stage includes a valid bit. Pipeline empty/full uses OR/AND reductions of the valid vector, while
`fma_pipe_tile_busy[tile][beat]` is decoded from the four tags. No duplicate occupancy flip-flops or popcount are required.
Multiple stages with the same tile/beat still reduce to busy. Draining one old tile therefore uses its next-state busy map
rather than requiring the entire pipeline to become empty.

### Forwarding

`fma_pipe_result_forward` requires input and result handshakes in the same cycle, matching tile/beat tags,
no resident/pipeline drain, and a result tag not marked `write_acc`.
The result becomes the next FMA addend without an accumulator write/read round trip.
The addend priority is forwarded result, lazy-zero value, then the selected accumulator slice.

Resident state means the newest accumulation may be retained in the FMA pipeline rather than in physical storage.
Forwarding uses actual tags and handshakes, not a fixed software instruction-spacing assumption.

### Resident Switch and Drain

A different incoming MAC tile raises `resident_switch_req`; the switch occurs when that MAC actually fires.
Existing pipeline tags are marked `write_acc`, so old-tile results write back while the new tile starts from its own accumulator data.
Overlap is permitted only when operand, busy and FPU-ready conditions allow it.

External VTSE/VTLE, VT/TV or clean operations that need resident state trigger a drain.
Drain completion uses the old tile's next-state busy map, not global pipeline emptiness; successor work may remain in flight.
Drain state stores only `resident_drain_writeback_q`: VTSE, VTLE and VT/TV preserve old results, while VTZERO and VTDISCARD
supersede them and can discard the drained values. A full drain-reason enum is therefore unnecessary.
An external same-tile read can trigger draining even with a younger MAC queued. That younger MAC waits for the read.
Requiring every queued MAC to disappear first could create a circular wait between VTSE and MAC.

## 4. Accumulator Organization and Byte Mapping

A logical tile ID names an architectural view. A physical slot stores 32-bit words.
TEW16 places two logical tiles in a slot; TEW8 places four. These are views of the same storage, not additional copies.

```text
physical_slot = tile_id / AccElemBytes
byte_offset   = tile_id % AccElemBytes
lane_row      = row % CE
lane_col      = col % CE
spatial_beat  = (row / CE) * (TE / CE) + (col / CE)
acc_address   = physical_slot * SpatialBeats + spatial_beat
```

| TEW | Logical IDs | Physical slot | Bit slices / byte enables |
| --- | --- | --- | --- |
| 8 | mt0 / mt1 / mt2 / mt3 | 0 | [7:0] / [15:8] / [23:16] / [31:24]; 0001 / 0010 / 0100 / 1000 |
| 8 | mt4..mt7, mt8..mt11, mt12..mt15 | 1, 2, 3 | Same four-byte mapping per slot |
| 16 | mt0 / mt2 | 0 | [15:0] / [31:16]; 0011 / 1100 |
| 16 | mt4 / mt6, mt8 / mt10, mt12 / mt14 | 1, 2, 3 | Same half-word mapping per slot |
| 32 | mt0, mt4, mt8, mt12 | 0, 1, 2, 3 | [31:0]; 1111 |

TEW8 allows all 16 mt IDs, TEW16 requires even IDs, and TEW32 requires multiples of four.
The same 4 KiB stores 16, 8 or 4 full logical tiles respectively. The physical word width remains 32 bits.

### Three-Side Port Organization

Each lane's `opope_accumulator` exposes three named clients. These are independent address/data paths, not three names for
one muxed port:

| Side | Read purpose | Write purpose | Transfer organization |
| --- | --- | --- | --- |
| FPU | Read one selected lane/beat as the addend | Write one retired FMA or SDOTP result | One 32-bit word per lane |
| VRF | VT tile-to-vector read and TV read-modify-write | TV vector-to-tile update | Grouped words feeding a `VRFWordWidth` move |
| VLSU | VTSE tile-row read and VTLE read-modify-write | VTLE memory-to-tile update | `TileDataWidth`, currently 512 bits |

The FPU side uses `fma_raddr_i/fma_rdata_o` and `fma_waddr_i/fma_wdata_i/fma_wen_i`.
For a MAC operation, the FPU read address is generated from the selected physical tile slot and spatial beat. The selected
32-bit word is then used as the addend; TEW8 selects one byte, TEW16 selects one half-word, and TEW32 selects the complete
word. When the arithmetic pipeline retires, the result is written back to the same physical slot and beat with the matching
byte enables. Thus, the accumulator memory stores the feedback value, while the FPU performs the actual multiply-accumulate
operation through either the FMA or SDOTP datapath.

The physical address is generated in two stages. First, the architectural tile ID is converted into a physical slot and a
byte or half-word offset. For example, `mt0`, `mt4`, `mt8`, and `mt12` select physical slots 0, 1, 2, and 3 for TEW32.
For a matrix element at logical coordinates `(row, col)`, the local compute bank is selected by `row % CE` and `col % CE`,
while the spatial beat is calculated as `(row / CE) * NrComputeGroupsPerTileEdge + (col / CE)`. The final address is
`physical_slot * SpatialBeats + spatial_beat`. Therefore, each lane-local bank stores all spatial beats for all four
physical slots, and the same addressing rule is used by the FMA, VRF, and VLSU paths.

The VRF and VLSU sides each have independent grouped read and write addresses, data and byte enables. These paths are
intended to access non-conflicting accumulator locations concurrently; for example, an FPU result may write one tile while
TV updates another tile and VTSE reads a third. The arbitration and hazard logic must still prevent overlapping accesses to
the same physical slot, spatial beat, and byte range.

The grouped width is required by the VME tile-subset model; it is not an implementation-only widening of the accumulator
interface. A tile-subset specifier identifies an architectural tile and selects either a row or a column. Therefore, one
tile move instruction describes a logical row or column of the selected tile rather than one isolated accumulator word. The
OPE must consequently assemble or consume the complete active subset while preserving the mapping between the architectural
tile and the distributed compute array. A row is distributed over the eight compute lanes. If the tile edge is larger than
the compute-array edge, the row contains several compute groups along that edge, and each group contributes one consecutive
accumulator word in every lane-local bank. These words are therefore accessed as one aligned group.

For an eight-by-eight compute array, a tile with edge 16 contains two compute groups along each edge. A row operation thus
groups two consecutive accumulator words per lane, giving eight lanes multiplied by two words and 32 bits per word, or
512 logical bits. With tile edge 32, the same architectural row contains four compute groups and the grouped width becomes
1024 bits. The corresponding column operation has the same architectural meaning; internally, the VLSU gathers or scatters
the required lane-local words and uses read-modify-write when only one column is being updated. These internal accesses are
implementation details of one architectural row or column operation, consistent with the TSS semantics.

This also explains why grouped width must not be confused with a single-cycle VRF transfer. The grouped width describes the
logical accumulator payload selected by the VME instruction. The shared VAU/VRF interface is only 256 bits wide, so a 512-bit
tile row is transferred over two VRF cycles and a 1024-bit tile row over four cycles. The VLSU-to-OPE tile interface can carry
the wider logical row, while the VRF path serializes that same row according to its physical port width.

`RD_PORTS` and `WR_PORTS` do **not** count independent clients. They specify how many consecutive physical accumulator words
one grouped VRF/VLSU access transfers from a common aligned base address. The implementation sets both values to
`NrComputeGroupsPerTileEdge`, which is `TE / CE`. With TE=16 and CE=8, two consecutive accumulator rows are grouped per
lane. Across the eight compute lanes, this forms one logical 512-bit tile row:

```text
CE * NrComputeGroupsPerTileEdge * AccElemWidth = 8 * 2 * 32 = 512 bits
```

With TE=32 and CE=8, `NrComputeGroupsPerTileEdge=4`, so four consecutive accumulator rows are grouped per lane and the
logical tile-row width becomes 1024 bits:

```text
CE * NrComputeGroupsPerTileEdge * AccElemWidth = 8 * 4 * 32 = 1024 bits
```

The VRF word itself is only 256 bits because the vector register file is shared with the VAU through three read ports and
one write port. Consequently, a TE=16 tile row requires two 256-bit VAU/VRF transfers, while a TE=32 tile row requires four.
The VLSU-to-OPE tile-row data width is independent of the VRF word width and is directly determined by `TE * AccElemWidth`:
512 bits for TE=16 and 1024 bits for TE=32.

The implementation derives `AddrWidth`, `RdPortIdxWidth` and `WrPortIdxWidth` from `DEPTH`, `RD_PORTS` and `WR_PORTS`.
Grouped read and write addresses are aligned to the corresponding number of consecutive words, so the grouped transfer starts
at a common aligned base address and accesses `base + 0` through `base + NrComputeGroupsPerTileEdge - 1`.

Independent ports do not remove data dependencies. Two writes must serialize when they target an overlapping physical
tile/beat/byte footprint. Different logical tile IDs can still share a physical 32-bit word under TEW8 or TEW16, so conflict
decisions must consider the physical slot and byte enables, not only equality of architectural IDs. Read/write behavior for
the same physical entry must likewise be resolved by the scoreboard, resident drain or explicit forwarding rather than by
procedural assignment order.

Writes place data into the selected byte/half/word and use byte write enables to preserve sibling tiles.
TEW8 reads pack selected bytes contiguously, without leaving 32-bit gaps between elements.
Zero metadata uses the same physical-slot/beat/byte mapping.
Move elements-per-word counters use `vrf_elem_count_t` so VRFWordWidth/8 fits even when it exceeds TE.

## 5. External Tile Path: VLSU

### Vector Memory EEW

The decoder records the element width encoded by standard vector load/store
instructions in `op_mem.ew`. Unit-stride and strided accesses interpret this
field as data EEW. Indexed accesses retain the existing split: `op_mem.ew` is
the index EEW and VSEW is the data EEW.

Both VLSU implementations derive a common data EEW with that rule. The data
EEW controls conversion of VL/VSTART to byte counts, element size, stride
normalization, load commit metadata, store strobes and memory request size.
Consequently, VLE32 transfers 32-bit elements even while the current matrix
VTYPE has VSEW=16. This removes the software-only VTYPE switch that was
previously needed around each packed load.

The controller continues to install the current architectural VTYPE when an
LSU request is dispatched; the decoder's memory EEW survives separately in
`op_mem.ew`. For VTFMM, the scoreboard still records the architectural
implicit row-stride vector-register reads according to VSEW and TK, even
though widening SDOTP obtains both packed FP16 lanes from each base vector
element. `msetmtype` updates matrix VTYPE/mtype and clears VL; `msettn` restores
TN and VL with the LMUL/ETE limits applied.

VTLE/VTSE run in the VLSU, not in the OPE's four request queues.
VLSU owns TSS interpretation, memory addressing, byte strobes, transactions and completion.
OPE owns accumulator access, byte selection and resident/busy protection. The VLSU-facing read/write path is independent of
the FMA and VRF-move accumulator paths; this independence is an OPE storage-port property and is separate from TCDM port count.

### VTSE: Tile to Memory

1. VLSU waits for `matrix_enable_i[id]` before accepting a tile-memory operation.
2. It presents `tile_rvalid_i` and `tile_r_req_i` to OPE, carrying idx, row, elems and tew.
3. OPE waits for relevant drain and read-port availability, then asserts `tile_rready_o`.
4. VLSU captures `tile_rdata_o` on valid && ready. There is no separate delayed tile-read response-valid channel.
5. VLSU buffers the row snapshot and sends memory chunks, tracking acknowledgments by instruction ID.

Reading a tile row does not retire VTSE. `tile_store_completion` tracks remaining chunks until the required memory acknowledgments arrive.

### VTLE: Memory to Tile

VLSU gathers memory responses and presents a stable tile write request containing idx, row, elems, tew and data.
OPE writes the selected accumulator bytes on `tile_wvalid_i && tile_wready_o`.
VLSU responds to the controller only after the required tile writes complete, retaining completion state under response contention.

### Rows, Columns and Bandwidth

One architectural instruction selects one row or column. The OPE/VLSU interface directly accesses rows.
VLSU implements a column using multiple row gather/scatter accesses; a column load uses read-modify-write to preserve other columns.
These are internal steps of one column instruction, not automatic progression to another architectural row/column instruction.

Row length uses tn, column length uses tm, and both are bounded by TE.
A full TEW32/16/8 row currently occupies 512/256/128 bits. Runtime TEW selects 4/2/1 bytes per element,
row-byte counts, memory strobes and column byte offsets.
Fitting one tile-interface beat does not guarantee a one-cycle memory transfer: alignment, port splitting, backpressure and responses still matter.

Ready considers resident state, in-flight work and port conflicts. It is not merely inverted busy and is not guaranteed glitch-free.
Synchronous endpoints sample valid && ready at the clock edge.

## 6. Commit and Completion

Four response spill registers feed an arbiter returning OPE instruction IDs. Valid and ID must remain stable under backpressure.
Distinguish three events:

- MAC completion: final beat issued, or empty operation.
- Accumulator accessibility: required values drained/written or available through internal forwarding.
- VTSE completion: corresponding memory acknowledgments received.

Scoreboard retirement therefore complements, rather than replaces, resident and pipeline-busy tracking.
In particular, MAC response can precede the final accumulator write.

## 7. Matrix Scoreboard

### Vector-Register LMUL and TK Tracking

The vector scoreboard separately protects VRF operands. `vreg_group_count` decodes the request's LMUL as one, two, four or
eight architectural vector registers. Fractional and reserved LMUL encodings conservatively use one register here; legality
is checked by the configuration path.

For memory operations, a vector load claims every register in its destination LMUL group in `write_table`, while a vector
store claims every register in its data-source LMUL group in `read_table`. This prevents a consumer from observing only the
first completed register of a grouped load and prevents a younger writer from overwriting any register still needed by a
grouped store. Index and other independently encoded operands retain their own dependency handling.

For `vtfmm`, both `vs1` and `vs2` are registered as readers across the complete LMUL group for every active K row. The K-row
base stride is selected from SEW:

| SEW | K-row register stride |
| --- | --- |
| 8 | 2 registers |
| 16 | 4 registers |
| 32 | 8 registers |

For example, FP32 with LMUL=2 and `tk=2` records `{v0,v1,v8,v9}` for a source based at v0 and
`{v16,v17,v24,v25}` for a source based at v16. Each listed register contributes its outstanding writer dependency and is
entered in `read_table` for WAR protection.

LMUL expands the VRF footprint protected by the scoreboard; it does not independently add OPE MAC steps. MAC execution is
controlled by `tm` and `tn`: spatial beat address generation crosses vector-register boundaries naturally.
For same-width FMA, `tk` selects separately strided K rows; widening SDOTP consumes
the packed K lanes in each base-vector element in parallel, without extra reduction beats.

### Address-Aware VRF Chaining in `DOUBLE_BW`

VRF operand availability has three separate meanings which must not be collapsed into one signal:

1. The controller scoreboard decides whether an older producer has written the exact VRF word requested by the consumer.
2. The shared-read arbiter decides whether OPE owns the physical VRF read ports in the current cycle.
3. `vrf_rvalid_i` confirms that the read request accepted by those ports produced valid data in the current cycle.

Consequently, OPE must retain `vrf_rvalid_i[0] && vrf_rvalid_i[1]` in `mac_fire`. A low value can mean that OPE did not receive
the shared-port grant or that the VRF did not accept the read; it does not prove that the architectural register has never been
written. Likewise, forcing read-valid from an address comparison would be unsafe because the VRF read-during-write behavior may
return an old or undefined value.

The previous `DOUBLE_BW` chaining gate used the instruction-level `wrote_result_q` pulse. A grouped load can remain in flight
after writing some of its words, so an already-written A or B word could become inaccessible whenever the same load was writing
a different word. This unnecessarily stalled `vtfmm` even though its requested operands were resident in the VRF.

The controller now keeps a sticky `vrf_word_ready_q[instruction_id][word_address]` bitmap for outstanding vector loads. The bit
is set only when the physical VRF write port accepts that exact address. Load metadata records the destination LMUL group's base
and exclusive word limit. For each consumer port, a load dependency blocks access only when the requested address lies inside
that producer's range and its corresponding ready bit is clear. Dependencies on non-load producers retain the previous
instruction-level wrote/done behavior. This also lets one MAC depend on separate A and B loads without requiring either load to
claim the other operand's address.

The read gate deliberately observes the registered bitmap, not its same-cycle next state. A read of an address written in the
same cycle waits until the following cycle. Supporting that case without a bubble requires an explicit VLSU-write-to-OPE-read
forwarding path; asserting valid early is not equivalent. The bitmap is cleared when the load retires or its instruction ID is
reused. This address-aware mechanism is compiled only for `DOUBLE_BW`; the non-double-bandwidth scoreboard remains unchanged.

`matrix_table_q` records outstanding instruction dependencies, not matrix data.
Each entry contains a footprint mask, read/write classification, MAC/LSU classification and older instruction dependencies.
TEW8/16/32 footprints occupy one/two/four mask bits. FP32 mt0 overlaps FP16 mt0/mt2 and byte tiles mt0..mt3.

| Operation | Tile access |
| --- | --- |
| VTLE | Write |
| VTSE | Read |
| VTFMM | Read + write |
| VTMV_VT | Read |
| VTMV_TV, VTZERO | Write |
| VTDISCARD | Global write, all mask bits set |

Dispatch snapshots older RAW/WAR/WAW dependencies. Retirement clears dependency bits, and a zero dependency vector enables execution.
Instructions may enter queues earlier but cannot perform tile side effects until enabled.
MAC/MAC pairs are exempt from this dependency check and rely on OPE scheduling, forwarding, switching and busy tracking.
VRF operands remain the vector scoreboard's responsibility.

VTDISCARD's all-ones mask is independent of TEW and TSS. It waits for older tile readers/writers,
and younger tile operations wait for discard retirement. Discard is not a MAC and does not use the MAC/MAC exemption.
OPE drain/idle guards remain necessary because an older MAC can respond before all its pipeline data is retired.

## 8. Packed FP16 Software

The TE16 rolled FP16xFP16-to-FP32 generators first quantize A and B to FP16,
then compute the golden C with FP32 operands and accumulation. They pack two
adjacent reduction rows into each `uint32_t` word:

```text
word[31:16] = K[2p]
word[15:0]  = K[2p+1]
```

Both operands use this order, so one widening SDOTP performs the two matching
products. The generated layouts are `A[m_tile][K/2][TE]` and
`B[n_tile][K/2][TE]`; no FP16 value is converted to FP32 in memory.

`matmul_l.K`, `MATMUL_K_CHUNK`, `valid_k`, and the K argument passed to
`matmul_fp16_fp32` all remain original FP16 reduction counts. Only packed-memory
calculations divide K by two: DMA extents and source offsets count packed
32-bit words. The kernel A/B parameters are `const __fp16 *`, so a complete
tile advances by `TE*K` halfwords. Each four-instruction VTFMM phase consumes
eight original K rows, hence the middle-loop count is `K/8 - 3`. Every VLE32
transfers packed words directly; C and the golden reference remain FP32.

The rolled schedules follow their FP32 counterparts after removing the
implicit row-stride VLE/VTFMM operations. Store-afterK distributes individual
VLE32 operations between VTFMM instructions and stores four completed tiles
after the K loop. Store-interleave overlaps tile stores, vector loads and
VTFMMs and retains its cross-block path. The vsetvl schedule replaces four
individual loads with one e32,m4 grouped load, restoring matrix state before
VTFMM as described above.

Utilization counts real FP16 FMAs. Peak throughput is `CE * CE * 2` FMAs per
cycle because each CE lane consumes two packed FP16 products per SDOTP.

## 9. Maintenance and Verification

Shared configuration comes from `hw/ip/spatz/src/spatz_pkg.sv.tpl`; do not edit only the generated package.
The cluster template currently links to this source. Check CE/TE, VRF operand width, accumulator depth and tile bandwidth together.
