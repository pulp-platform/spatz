# Ventaglio — Streamlined Sparse Execution Unit

Ventaglio is a gather/scatter execution unit attached to Spatz. It owns a small
banked buffer (the *VTL bank*) that holds the sparsity-expanded accumulator of
an N:M sparse mat-mul, and it streams products into that buffer at
index-selected positions. The custom `vfx*` instructions drive it; ordinary
loads/stores reach the bank through the regular VRF, transparently redirected by
the controller.

This document is a map of the design: the instruction encoding, the on-chip
structure, and the two sizing constraints worth knowing before you change a
config.

## Instruction encoding

A plain `vfxmacc.vf` cannot encode the *index* vreg, so the Spatz controller
cannot trace the RAW dependency on it. Ventaglio therefore uses a custom-0
encoding that names the index explicitly. This instruction is depreciated.

```
   31    27 26 25 24    20 19    15 14  12 11   7 6      0
  +--------+-----+--------+--------+-----+------+--------+
  |  vs1   |fnct2|  vs2   |  rs1   |fnct3|  vd  |opcode  |
  +--------+-----+--------+--------+-----+------+--------+
     idx          weight    fp_scal         acc   custom-0
```

- `opcode = 0001011` (custom-0), `funct3 = 010` (selects the family),
  `funct2 = 00` (precision tag: FP32; reserve 01/10 for FP16/BF16).
- `vd` = accumulator, `rs1` = FP scalar, `vs2` = weight, `vs1` = **index**.

Three ops use the unit:

| op            | role                                              | retires via |
|---------------|---------------------------------------------------|-------------|
| `vfxmul.vrf`  | scatter `scalar * weight` into a cleared bank     | `vfu_rsp`   |
| `vfxmacc.vrf` | gather + accumulate `scalar * weight` into bank   | `vfu_rsp`   |
| `vventclr`    | zero the whole bank (between accumulator groups)  | `vtl_rsp`   |

Example: `vfxmacc.vrf v16, ft0, v8, v20` → acc=v16, scalar=ft0, weight=v8,
index=v20.

## How an op flows through the unit

1. The controller issues a `use_vtl` op; it is latched into the **operation
   queue** (`i_operation_queue`) and, in the same handshake, an index fetch is
   queued in the **index request register** (`i_idx_req_reg`).
2. The **master VRF read** returns the op's index vector, which lands in the
   **index data spill** (`i_idx_spill` → `index_q`).
3. The VFU computes `scalar * weight`; the **scatter datapath** writes the
   products into the bank at the positions named by `index_q` (accumulating, for
   `vfxmacc`).
4. `vventclr` zeroes the bank between accumulator groups via the **clear
   sequencer**.
5. A later `vse` of the accumulator vreg reads it back out of the bank through
   the **gather / slave-read path** and drains to memory.

## Microarchitecture (`ventaglio.sv`)

- **VTL bank** — `VTGNrChannels` channels × `N_FU` banks of `ELEN` bits
  (`gen_vtg_channels`/`gen_vtg_banks`, each a `ventaglio_regfile`). Words are
  channel-interleaved; `f_channel`/`f_row` split a `vrf_addr_t` into
  (channel, row). It holds the expanded accumulator vreg(s).
- **Operation queue** — `i_operation_queue`, a `spill_register` latching each
  `use_vtl` op. The state handler maintains the `running_q` bitmap and retires
  ops (`vfu_rsp` for vfx, `clear_done` for `vventclr`).
- **Index path** — two spill registers:
  - `i_idx_req_reg` (`T = {id, addr}`) holds the master VRF **read request**.
    It is pushed on the pre-spill admit handshake (`eff_fetch`); its `valid_o`
    drives `vrf_re_o`, its `data_o` drives `vrf_id_o[0]`/`vrf_raddr_o`, and
    `ready_i = vrf_rvalid_i` keeps the request stable until the read returns.
    The one-cycle registration is deliberate: it delays the read to the cycle
    *after* issue, so the controller scoreboard has already registered the
    index vreg's RAW dependency — without it, a combinational read against a
    stale scoreboard returns a zeroed index. `ready_o` is intentionally unused
    (no back-pressure); correctness relies on ≤2 requests outstanding, which
    the op-queue depth and the in-order, one-fetch-per-op admit guarantee.
  - `i_idx_spill` (`T = vrf_data_t`) holds the returned index **data**
    (`index_q`) feeding the scatter/gather datapaths.
- **Scatter datapath** (`ventaglio_scatter`) — routes `vfx` products into the
  bank using `index_q`; `vfxmul` overwrites, `vfxmacc` read-modify-writes.
- **Gather datapath** (`ventaglio_gather`) — reads bank words back out (used
  when a VTL-mapped vreg is read, e.g. the `vse` drain).
- **Clear sequencer** — on `vventclr`, walks every bank row writing zero, then
  asserts `vtl_rsp_valid_o` on the last row to retire the op.
- **VRF ports** — *slave* ports serve controller-redirected VLSU/VFU accesses
  to bank-mapped vregs; the *master* read port fetches indices from the regular
  VRF. `is_gather`/`is_scatter` qualify a slave access and need **both** the
  op's decode intent (`op_vtl.*_vd`) **and** the controller's per-access
  redirect grant (`rgather_en_i`/`wscatter_en_i`). The master *write* port is
  reserved (tied off) for a future scatter→VRF write-back.

## Beats per operation

A *beat* is one datapath cycle moving one VRF word
(`VRFWordWidth = N_FU * ELEN` bits) through the bank:

```
elements_per_beat = VRFWordBWidth >> vsew          // VRFWordBWidth = N_FU * ELENB
num_beats_per_op  = vl >> ($clog2(VRFWordBWidth) - vsew)
```

Default config (`N_FU=4`, `ELEN=64` → `VRFWordBWidth = 32` B):

| vsew      | SEW    | elements/beat | shift |
|-----------|--------|---------------|-------|
| EW_8 (0)  | 8 bit  | 32            | 5     |
| EW_16 (1) | 16 bit | 16            | 4     |
| EW_32 (2) | 32 bit | 8             | 3     |
| EW_64 (3) | 64 bit | 4             | 2     |

`vsetvli` snaps `vl` to a multiple of `elements_per_beat` (via `VLMAX`), so the
truncating shift is exact for every upstream kernel. `num_beats_per_op` is
5 bits — fine for all current configs; widen to `vlen_t` if `N_FU` ever shrinks
enough that `vlmax / elements_per_beat > 31`.

## Sparsity-expansion limits

The store of the expanded accumulator remaps `vl` by the sparsity ratio
(`vl<<2` for 1:4, `<<1` for 2:4); the VLSU then converts elements→bytes
(`<<2` for e32). Both the byte-domain `vl` (`vlen_t`) and the per-op word index
(`vreg_elem_t` in `spatz_vlsu.sv`) must hold the expanded value, so under
`` `ifdef VENTAGLIO `` both are widened by 2 bits. The bank is
`VENTAGLIO_BUFFER_SIZE = 8192` bits = 256 fp32.

Default config (`VLEN=512`, `N_FU=4`, `WFACTOR=4`):

| case      | elements | bytes | bank use |
|-----------|----------|-------|----------|
| m2 × 1:4  | 128      | 512   | 50%      |
| m4 × 1:4  | 256      | 1024  | **100%** |
| m4 × 2:4  | 128      | 512   | 50%      |

m4×1:4 works with the widened types but has **zero margin**: a larger `VLMAX`,
a 1:8 ratio, or `WFACTOR<4` overflows the bank and/or the byte-`vl` and would
need a bigger `VENTAGLIO_BUFFER_SIZE` / `vlen_t`. 1:4 at LMUL ≤ m2 needs no
widening.

## Known limitations / future work

- **Slave-port gather/plain ambiguity (latent).** A slave read takes the gather
  datapath whenever a `gather_vd=1` op is latched and `rgather_en_i` is high, so
  a plain `vse` of a bank vreg relies on timing not to coincide with a latched
  `vfxmacc`. Not observed to fail in the regressions; a robust fix would carry a
  per-access gather/plain qualifier from the controller.
- **VFU-handshake slimming (deferred).** Removing
  `vfu_req_ready_i`/`vfu_rsp_valid_i`/`vfu_rsp_i` and deriving retirement
  in-module was attempted and reverted — the controller scoreboard retire path
  is timing-coupled to `vfu_rsp_valid`; warrants its own PR.
- **No overflow guard.** An over-config (see limits above) silently zero-stores
  rather than failing loudly; an assertion on the expanded byte-`vl` vs
  `vlen_t`/bank range is a worthwhile addition.
