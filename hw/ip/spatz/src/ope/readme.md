# Spatz OPE Architecture and Dataflow

This document describes the current Outer Product Engine (OPE) RTL. It is not an ISA specification or a simulation sign-off.
The top-level module is `spatz_ope.sv`; computation and storage are implemented by `opope_fma.sv` and `opope_accumulator.sv`.
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
        |       |                     CE x CE FMA pipeline        |
        |       |                      ^           |              |
        |       |                      +-- forward-+              |
        |       |                      ^           | drain/write  |
        |       |                      |           v              |
        |       +--> TV spill --> VRF read [2] --> Accumulator     |
        |       +--> VT spill <-- Accumulator --> VRF write        |
        |       +--> Clean spill --> zero metadata / flush         |
        |                                                         |
        +-- LSU request --> VLSU <--- tile read/write ---> Accumulator
                              |
                              v
                           L1 TCDM

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
| `TE = TileEdge` | 16 | Maximum logical tile edge |
| `CE = OPEComputeEdge` | 8 | Outer-product compute edge per beat |
| `AccElemWidth` | 32 bits | Fixed physical accumulator word width |
| `NumPipeRegs` | 4 | FMA pipeline register stages |
| `SpatialBeats = (TE / CE)^2` | 4 | Spatial beats for a full tile |
| `AccDepth` | 16 | Accumulator entries per compute lane |
| `TileDataWidth` | 512 bits | OPE/VLSU tile data interface width |

Each of the CE x CE lanes has dedicated FP16 and FP32 FMA units and a 32-bit accumulator bank.
Runtime TEW selects the arithmetic path; the two formats do not simultaneously compute two results for one element.
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
For tm=tn=16 and CE=8, tk=1 uses four beats and tk=2 uses eight. This is issue work, not end-to-end latency under stalls.

**Multiple-beat benefit:** a CE x CE array is reused across a larger TE x TE tile and across K reduction steps.
The hardware does not need one FMA per element of the entire tile, and software does not issue a separate instruction for every rank-1 step.

**Active-lane-mask benefit:** each beat enables FMA valid and accumulator writes only for row < tm and column < tn.
Fringe tiles do not require padded arithmetic to protect the result, and inactive elements are not overwritten.
This avoids valid work on inactive lanes but does not imply every combinational node is clock-gated.
Real power and utilization still depend on operand delivery, stores and pipeline stalls.

Each source supplies one VRF word per beat. The configuration must satisfy `CE * SEW <= VRFWordWidth`.
Increasing CE alone does not add a multiword operand collector.

### Different SEW and TEW

SEW controls vector operand extraction, bit offsets and K-row stride. TEW controls accumulation precision and the tile byte footprint.
`mtype.mtwiden` encodes a multiplier: 01 means 1, 10 means 2, and 11 means 4; 00 invalidates matrix state.
The element width is `TEW = SEW * multiplier`, not SEW times the raw encoded field.

| SEW / TEW | Current behavior |
| --- | --- |
| 8 / 8 | Raw-byte tile load/store, TV/VT and VTZERO; no FP8 MAC |
| 16 / 16 | FP16 inputs, FP16 accumulation/rounding, selected 16-bit slice |
| 16 / 32 | FP16 or alternate FP16 inputs widened to FP32, FP32 FMA/accumulation, full word |
| 32 / 32 | Native FP32 inputs and accumulation |

For SEW16/TEW32, vector loads provide 16-bit operands, and the FP32 FMA input path converts them to FP32 before arithmetic.
The accumulator remains FP32; it does not narrow each result back to FP16. Tile memory transfers use the 32-bit TEW layout.
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
Accumulator access must avoid FMA writeback, VT and external tile-port conflicts.
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

## 3. FMA Pipeline, Forwarding and Switching

### Data and Tags

Each `opope_fma` has four distributed register stages. The FP16/FP32 ready signals are aggregated into `fma_pipe_ready`.
`fma_pipe_advance` advances computation and tag state together. Tags contain tile ID, TEW, spatial beat, lane mask,
instruction ID and `write_acc`. A single core still has multiple outstanding instructions, so instruction IDs remain necessary.

`fma_pipe_count_q` tracks in-flight beats; `fma_pipe_tile_busy_q[tile][beat]` tracks outstanding work per tile/beat.
Simultaneous issue and result consumption can leave the total count unchanged.
Consequently, draining one old tile cannot require the entire pipeline count to become zero.

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
Overlap is permitted only when operand, busy and FMA-ready conditions allow it.

External VTSE/VTLE, VT/TV or clean operations that need resident state trigger a drain.
Drain completion uses the old tile's next-state busy map, not global pipeline emptiness; successor work may remain in flight.
An external same-tile read can trigger draining even with a younger MAC queued. That younger MAC waits for the read.
Requiring every queued MAC to disappear first could create a circular wait between VTSE and MAC.

## 4. Accumulator Byte Mapping

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

The scalar read port supplies the selected lane/beat addend. Grouped ports support VT/TV and external tile accesses.
Writes place data into the selected byte/half/word and use byte write enables to preserve sibling tiles.
TEW8 reads pack selected bytes contiguously, without leaving 32-bit gaps between elements.
Zero metadata uses the same physical-slot/beat/byte mapping.
Move elements-per-word counters use `vrf_elem_count_t` so VRFWordWidth/8 fits even when it exceeds TE.

TV, external writes and FMA writeback share the accumulator write mux. FMA writeback is handled last in `acc_access_proc`;
handshakes must exclude conflicting accepted accesses. Procedural assignment priority alone does not make concurrent requests safe.

## 5. External Tile Path: VLSU

VTLE/VTSE run in the VLSU, not in the OPE's four request queues.
VLSU owns TSS interpretation, memory addressing, byte strobes, transactions and completion.
OPE owns accumulator access, byte selection and resident/busy protection.

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

## 8. Maintenance and Verification

Shared configuration comes from `hw/ip/spatz/src/spatz_pkg.sv.tpl`; do not edit only the generated package.
The cluster template currently links to this source. Check CE/TE, VRF operand width, accumulator depth and tile bandwidth together.
