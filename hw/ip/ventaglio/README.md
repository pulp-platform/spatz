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

### Already done
- [x] **C1** — annotate the master VRF write outputs as reserved for future scatter→VRF write-back (port-decl + assignment-site comments).
- [x] **C2** — generalize `num_beats_per_op` formula (no longer hard-coded to `N_FU=4` / `EW_32`); derivation moved to this README.
- [x] **C4** — `op_beat_cnt_q` wrap is now gated on `rvalid_o`; the counter is kept as an explicit hook for long-vector index-refresh and the comment documents that.
- [x] **D1** — dropped the dead `new_vtl_request_q` flop; the signal is now pure combinational `new_vtl_request` (no `_d` / `_q` suffix since there is no register).

### Open (correctness / scope)
- [ ] **D3** — drop the dead `// gather_rvalid[g_channel][VrfRd] = 1'b1;` left next to its live replacement.
- [ ] **D4** — drop the dead `// assign rdara_pre_gather[...]` comment.

### Deferred (do not attempt under this PR)
- **D2 / vfu_*-removal refactor** — attempted on 2026-05-21..27 (removing
  `vfu_req_ready_i` / `vfu_rsp_valid_i` / `vfu_rsp_i` and deriving retire from
  in-module `op_retire`). Compiled and passed `sp-SpMV_2to4` but hung the
  `sp-SpMM_*to4_M8` kernels mid-iteration. Root cause was not isolated within
  the PR window; the controller-side scoreboard chain check appears to be
  timing-coupled to the original `vfu_rsp_valid` retire path in ways that
  warrant its own targeted PR. Reverted in full.
- **Port-simplification (slave-port flattening + multi-read-port hook)** — the
  multi-read-port hook in `ventaglio_regfile.sv` (`NrReadPorts` parameter +
  per-port `rr_arb_tree` loop), the `VTGNrReadPortsPerBank` constant in
  `spatz_pkg.sv` / `.tpl`, and the vector-shaped slave VRF ports in
  `ventaglio.sv` form one coupled change. Started but reverted alongside D2
  since the regfile / pkg side cannot land without the matching
  `ventaglio.sv`-side flattening. Land as a single follow-up PR.

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
