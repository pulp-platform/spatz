# Ventaglio Streamline Sparse Execution Unit

**WIP**

## Change of encoding
- The current vfxmacc.vf instruction does not encode the *vid* for the index metadata; as a result, the dependency in Spatz controller often fail to trace and gate the requests. Therefore, we need to encode this metadata explicitly in the encoding space.

Encoding proposal — vfxmacc.vrrf in custom-0  

   31    27 26 25 24    20 19    15 14  12 11   7 6      0                                                                                                                                                         
  +--------+-----+--------+--------+-----+------+--------+                                                                                                                                                         
  |  vs1   |fnct2|  vs2   |  rs1   |fnct3|  vd  |opcode  |                                                                                                                                                         
  +--------+-----+--------+--------+-----+------+--------+                                                                                                                                                         
     idx          weight    fp_scal         acc   custom-0   

Concretely:                                                                                                                                                                                                      
  VFXMACC_VRRF = 32'b?????00????????????010?????0001011                                                                                                                                                            
  - opcode[6:0] = 0001011 (custom-0)                                                                                                                                                                               
  - funct3[14:12] = 010 (picks this op out of the custom-0 family; reserve 011/100/etc. for future variants)
  - funct2[26:25] = 00 (precision tag — FP32; future 01=FP16, 10=BF16…)                                                                                                                                            
  - vd[11:7], rs1[19:15], vs2[24:20], vs1[31:27]                                                                                                                                                                   
                                                                                                                                                                                                                   
  vfxmacc.vrrf v16, ft0, v8, v20 → vd=16, rs1=ft0(0), vs2=8, vs1=20, funct2=00, funct3=010, opcode=0001011.

## Code-review punch list (`ventaglio.sv`)

Items raised during the 2026-05-27 walkthrough; check off as addressed.

### Status update (2026-06-17) — supersedes stale items below

All four sparse regressions pass: `sp-SpMV_2to4`, `sp-SpMV_1to4`,
`sp-SpMM_2to4_M8`, `sp-SpMM_1to4_M8`. The index path and the LSU `vl` handling
were reworked after the original list was written, so several items below are
now done or obsolete:

- **Index path is now two spill registers** (replaces the old single-spill +
  `fetch_in_flight_q`/`fetched_id_q`/`fetched_vidx_q` snapshot and the
  `VTG_TRACE_IDX_SPILL` probe, both removed):
  - `i_idx_req_reg` (`T = {id, addr}`) buffers the master VRF *read request*.
    Pushed by `eff_fetch` (pre-spill admit handshake); `valid_o`→`vrf_re_o`,
    `data_o`→`vrf_id_o[0]`/`vrf_raddr_o`, `ready_i = vrf_rvalid_i` holds it
    stable until the read returns. The one-cycle registration is the fix for
    the SpMV stale-index bug: it delays the read to the cycle after issue, so
    the controller scoreboard has registered the index vreg's RAW dependency
    before the read lands (otherwise a combinational read against a stale
    scoreboard returned a zeroed index). `ready_o` is intentionally unused —
    no back-pressure; correctness relies on ≤2 requests outstanding.
  - `i_idx_spill` (`T = vrf_data_t`) buffers the returned index *data*
    (`index_q`) feeding the scatter/gather datapaths.
- **Done in the 2026-06-17 cleanup pass:** C5 (`gather_done`→`gather_admitted_q`),
  Doc1 (module architectural header), Doc2 (`is_gather`/`is_scatter` comment),
  M1/M4 (alignment + default-ready comment), `vrf_id_o[1]` tied to `'0`, both
  typos (`2025→2026`, "comsume"→"consume"), and a trailing-whitespace sweep.
  Dead code removed: `op_beat_cnt_q/d`, `req_proceed`, `last_addr_bit_q/d`;
  `ventaglio_gather.load_index_o` left explicitly unconnected (per-beat
  index-refresh hook, no consumer in the single-index-word design).
- **Obsolete:** M2/M3 (referenced `proc_idx_addr_gen`, which is gone — the read
  address now comes from `idx_req_out.addr`); C3/C4/M5 op-beat-counter parts
  (the counter is removed; `num_beats_per_op` stays, used only by the scatter
  datapath).
- **Deferred (latent, follow-up PR):** a slave-port read takes the gather
  datapath whenever a `gather_vd=1` op is latched and `rgather_en_i` is high, so
  a plain `vse` of a bank vreg relies on timing not to coincide with a latched
  `vfxmacc`. Not observed to fail in the four regressions; a robust fix carries
  a per-access gather/plain qualifier from the controller.

### Sparsity-expansion limits

The VTL store of the expanded accumulator remaps `vl` by the sparsity ratio
(`vl<<2` for 1:4, `<<1` for 2:4); the VLSU then converts elements→bytes
(`<<2` for e32). Both the byte-domain `vl` (`vlen_t`) and the per-op word index
(`vreg_elem_t` in `spatz_vlsu.sv`) must hold the expanded value, so under
`` `ifdef VENTAGLIO `` both are widened by 2 bits. The bank is
`VENTAGLIO_BUFFER_SIZE = 8192` bits = 256 fp32.

For the default config (`VLEN=512`, `N_FU=4`, `WFACTOR=4`):

| case      | elements | bytes | bank use |
|-----------|----------|-------|----------|
| m2 × 1:4  | 128      | 512   | 50%      |
| m4 × 1:4  | 256      | 1024  | **100%** |
| m4 × 2:4  | 128      | 512   | 50%      |

m4×1:4 works with the widened types but has **zero margin** — a larger `VLMAX`,
a 1:8 ratio, or `WFACTOR<4` overflows the bank and/or the byte-`vl` and must
bump `VENTAGLIO_BUFFER_SIZE` / `vlen_t`. 1:4 at LMUL≤m2 needs no widening.

### Already done
- [x] **C1** — annotate the master VRF write outputs as reserved for future scatter→VRF write-back (port-decl + assignment-site comments).
- [x] **C2** — generalize `num_beats_per_op` formula (no longer hard-coded to `N_FU=4` / `EW_32`); derivation moved to this README.
- [x] **C4** — `op_beat_cnt_q` wrap is now gated on `rvalid_o`; the counter is kept as an explicit hook for long-vector index-refresh and the comment documents that.
- [x] **D1** — dropped the dead `new_vtl_request_q` flop; the signal is now pure combinational `new_vtl_request` (no `_d` / `_q` suffix since there is no register).
- [x] **Slave-port flattening (NrReadPorts / NrWritePorts drop)** — both parameters were always 1; removed them, flattened the 11 slave ports to scalar, dropped `VrfRd` / `VrfWd` localparams, collapsed inner `for (port = ...)` loops, and dropped one `[port]` dimension from internal `read_request` / `write_request` / `gather_*` / `scatter_*` arrays. Connected callers in `spatz.sv` already declared the corresponding signals as scalar (the previous vector ports relied on SV's relaxed 1-bit-packed-vector binding); no caller change required. Also dropped the dead D3 commented-out `gather_rvalid` line that lived in the same region.
- [x] **D3** — dropped along with the slave-port flattening.
- [x] **D4** — dropped the dead `// assign rdara_pre_gather[...]` comment along with the internal bank-port simplification below.
- [x] **Internal bank-port simplification** — `ventaglio_regfile.sv`'s `NrReadPorts` parameter and per-port `rr_arb_tree` `gen_reads`/`gen_rp` wrappers are gone (single scalar `raddr_i` / `rdata_o`); `VTGNrReadPortsPerBank` retired from `spatz_pkg.sv` + `.tpl`; the 2D `raddr` / `rdata` in `ventaglio.sv` collapsed to 1D and the `gen_rdata_assignment` intermediate (which also held the D4 dead comment) replaced by a direct slice assignment from each regfile instance's scalar `rdata_o` into `rdata[channel][ELEN*bank +: ELEN]`.
- [x] **Index buffer → `spill_register`** — the ID-tagged 2-slot ping-pong (`index_data_q[1:0]` / `index_buf_valid_q` / `index_buf_id_q` / `active_buf_q` / `fetch_lock_q` + slot-target snapshot) is gone, replaced by a single `common_cells/spill_register` instance treated as a 2-deep FIFO. Push on `vrf_rvalid_i`, pop on Ventaglio's `spatz_req` spill advance (for non-vventclr ops). The two FIFOs march in lock-step. `fetch_in_flight_q` + `fetched_id_q` + `fetched_vidx_q` (1+3+5 = 9 bits) replaces the old multi-field fetch-lock snapshot — much smaller than the old 2-slot state (~520 bits) but still required by the master VRF read protocol, which needs `vrf_re_o` / `vrf_raddr_o` / `vrf_id_o[0]` held asserted/stable until `vrf_rvalid_i` returns. A first refactor attempt that dropped `vrf_re_o` post-issue deadlocked at the first multi-cycle VRF response (see `[[project-index-buffer-spill-register-study]]` memory note); fix re-introduced the id/vidx snapshot. Verified 2026-06-16 across all three sparse regressions. Opt-in trace block kept under `\`ifdef VTG_TRACE_IDX_SPILL` for future debug.

### Deferred (do not attempt under this PR)
- **D2 / vfu_*-removal refactor** — attempted on 2026-05-21..27 (removing
  `vfu_req_ready_i` / `vfu_rsp_valid_i` / `vfu_rsp_i` and deriving retire from
  in-module `op_retire`). Compiled and passed `sp-SpMV_2to4` but hung the
  `sp-SpMM_*to4_M8` kernels mid-iteration. Root cause was not isolated within
  the PR window; the controller-side scoreboard chain check appears to be
  timing-coupled to the original `vfu_rsp_valid` retire path in ways that
  warrant its own targeted PR. Reverted in full.

### Open

**Maintainability**
- [ ] **M1** — Fix the `.ready_o` line alignment inside `i_operation_queue` (extra trailing space breaks the column alignment).
- [ ] **M2** — Factor `{eff_vidx, $clog2(NrWordsPerVector)'(1'b0)}` into a `vreg_base` local (or a helper like `vreg_addr(eff_vidx, offset)`) so the three sites in `proc_idx_addr_gen` stop repeating themselves.
- [ ] **M3** — Document the "look-ahead +1" trick in `proc_idx_addr_gen`: when `vreg_idx_counter_en` is high, the address issued this cycle is the *next* beat's address. Easy to mistake for a bug without a comment.
- [ ] **M4** — Add a one-line comment above `spatz_req_ready = !spatz_req_valid;` explaining the "no op in flight → accept anything" default. Reads backwards otherwise.
- [ ] **M5** — Replace magic constants with derived ones:
  - Bit width of `num_beats_per_op` (`logic [4:0]`) — derive from `$clog2(VLEN/min_elem_per_beat)` or assert.
  - The `>> 3` is now removed by C2, but `op_beat_cnt_q` still has width 5 — same width follow-up.

**Correctness / scope (need decisions)**
- [ ] **C3** — Skipped per current discussion; revisit if `op_beat_cnt_q` is ever exposed: the current rollover condition is fine because the `if` is now guarded by `rvalid_o`. Old C3 concern (`_q` vs `_d`) no longer applies after C4 fix.
- [ ] **C5** — Rename `gather_done` to `gather_active_q` or `gather_admitted_q`. The name reads as "gather finished" but the signal is actually "1-cycle-delayed gather-admit".

**Documentation**
- [ ] **Doc1** — Add a 5–10 line architectural header at the top of the module summarising: the VTL bank shape (channels × banks), the double-buffered index RAM, the scatter/gather sub-datapaths, the clear sequencer, and how slave vs master VRF ports flow.
- [ ] **Doc2** — Clarify `is_gather` / `is_scatter` (lines around 455): both fields require the controller-driven `rgather_en_i` / `wscatter_en_i` arbitration grant — without that context the `gather_vd && rgather_en_i` looks redundant.

**Typos**
- [ ] Bump `Copyright 2025` → `2026` at line 1.
- [ ] Line 433 (function-helper comment): "comsume" → "consume".

## Beats per operation

A *beat* is one cycle of operation through the gather/scatter datapath.
Each beat moves one VRF word — `VRFWordWidth = N_FU * ELEN` bits — through
the Ventaglio bank. So the number of beats needed to drain a vector op is:

```
elements_per_beat = VRFWordWidth / SEW_bits
                  = VRFWordBWidth >> vsew         // VRFWordBWidth = N_FU * ELENB
                                                  // SEW_bits = 8 << vsew

num_beats_per_op  = vl / elements_per_beat
                  = vl >> ($clog2(VRFWordBWidth) - vsew)
```

For the default config (`N_FU=4`, `ELEN=64`, so `VRFWordBWidth = 32`
bytes):

| vsew     | SEW    | elements/beat | shift |
|----------|--------|---------------|-------|
| EW_8  (0)| 8 bit  | 32            | 5     |
| EW_16 (1)| 16 bit | 16            | 4     |
| EW_32 (2)| 32 bit | 8             | 3     |
| EW_64 (3)| 64 bit | 4             | 2     |

### Assumptions

- **`vl` is a multiple of `elements_per_beat`.** `vsetvli` snaps `vl` to a
  multiple of the datapath width via `VLMAX`, so this holds for every
  upstream kernel. For non-aligned `vl`, switch to ceil-div:
  `(vl + ((1<<shift)-1)) >> shift`.
- **Width budget.** `num_beats_per_op` is currently 5 bits, sufficient for
  all hjson configs today (max 16 beats for `LMUL=8`, `EW_8`, default
  `N_FU=4`). If `N_FU` is ever reduced enough that
  `vlmax / elements_per_beat > 31`, widen to `vlen_t` here and propagate
  the change to the `num_beats_per_op_i` input port of
  `ventaglio_scatter` and `ventaglio_scatter_datapath`.
