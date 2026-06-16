// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Bowen Wang, ETH Zurich
//
// Standalone Gather/Scatter datapath
//

module ventaglio
  import spatz_pkg::*;
  import rvv_pkg::*;
  import vtl_pkg::*;
  #(
    parameter int unsigned NarrowDataWidth = VTGChannelWidth,
    parameter int unsigned WideDataWidth   = VTGChannelWidth * VENTAGLIO_WFACTOR
  ) (
    input  logic            clk_i,
    input  logic            rst_ni,
    input  logic            testmode_i,

    // Spatz request
    input  spatz_req_t       spatz_req_i,
    input  logic             spatz_req_valid_i,
    // VTL admit handshake to the controller. High when the spill
    // register can accept the next use_vtl op (vventclr or vfx).
    output logic             vtl_req_ready_o,
    input  logic             spatz_vfu_req_ready_i,
    // VTL response (retirement path for vventclr; vfx retires via vfu_rsp)
    output logic             vtl_rsp_valid_o,
    output vsldu_rsp_t       vtl_rsp_o,
    // VFU response
    input  logic             vfu_rsp_valid_i,
    input  vfu_rsp_t         vfu_rsp_i,
    // Slave Write port (single)
    input  vrf_addr_t        waddr_i,
    input  vrf_data_t        wdata_i,
    input  logic             we_i,
    input  vrf_be_t          wbe_i,
    output logic             wvalid_o,
    input  logic             wscatter_en_i,
    // Slave Read port (single)
    input  vrf_addr_t        raddr_i,
    input  logic             re_i,
    output vrf_data_t        rdata_o,
    output logic             rvalid_o,
    input  logic             rgather_en_i,

    // Master VRF interface
    // Master VRF write outputs are RESERVED for a future scatter→VRF write-back
    // path (e.g., draining bank results to the regular VRF). Currently tied to
    // zero; see the assignment-site comment at the bottom of this module.
    output vrf_addr_t                       vrf_waddr_o,
    output vrf_data_t                       vrf_wdata_o,
    output logic                            vrf_we_o,
    output vrf_be_t                         vrf_wbe_o,
    input  logic                            vrf_wvalid_i,
    output spatz_id_t  [1:0]                vrf_id_o,
    output vrf_addr_t                       vrf_raddr_o,
    output logic                            vrf_re_o,
    input  vrf_data_t                       vrf_rdata_i,
    input  logic                            vrf_rvalid_i,

    // Per-vreg "writer in flight" from controller. Combinational from
    // write_table_q.valid (which is set on writer issue, cleared on retire).
    // Used to gate the speculative prefetch of the next op's index vreg.
    input  logic       [NRVREG-1:0]         vreg_write_pending_i
  );

// Include FF
`include "common_cells/registers.svh"

  ///////////////////////
  //  Operation queue  //
  ///////////////////////

  spatz_req_t spatz_req_d;

  spatz_req_t spatz_req;
  logic       spatz_req_valid;
  logic       spatz_req_ready;



  // Spill admits any VTL op regardless of which ex_unit it routes to:
  //   - vfxmacc.vrf / vfxmul.vrf are dispatched as ex_unit=VFU.
  //   - vventclr is dispatched as ex_unit=SLD (bypasses the VFU entirely).
  // Both kinds need ventaglio's bank, so admission is gated solely on use_vtl.
  spill_register #(
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i                                              ),
    .rst_ni (rst_ni                                             ),
    .data_i (spatz_req_d                                        ),
    .valid_i(spatz_req_valid_i && spatz_req_i.op_vtl.use_vtl    ),
    .ready_o(vtl_req_ready_o                                    ),
    .data_o (spatz_req                                          ),
    .valid_o(spatz_req_valid                                    ),
    .ready_i(spatz_req_ready                                    )
  );

  always_comb begin : proc_spatz_req
    spatz_req_d = spatz_req_i;
  end

  /******************************/
  /*       Clear Sequencer      */
  /******************************/
  // vventclr writes 0 to every cell of ventaglio's bank. Walks bank rows
  // 0 → VTGNrWordsPerChannel-1, all VTGNrChannels in parallel each cycle.
  // On the last row, asserts vtl_rsp_valid_o so the controller can retire
  // the op via the (formerly-VSLDU) SLD retirement path.
  typedef logic [$clog2(VTGNrWordsPerChannel)-1:0] vtg_row_addr_t;

  logic           clear_active_q,  clear_active_d;
  vtg_row_addr_t  clear_row_q,     clear_row_d;
  spatz_id_t      clear_id_q,      clear_id_d;
  `FF(clear_active_q, clear_active_d, 1'b0)
  `FF(clear_row_q,    clear_row_d,    '0)
  `FF(clear_id_q,     clear_id_d,     '0)

  logic clear_last_row;
  assign clear_last_row = (clear_row_q == vtg_row_addr_t'(VTGNrWordsPerChannel - 1));

  // True for the cycle that retires the clear op.
  logic clear_done;
  assign clear_done = clear_active_q && clear_last_row;

  /******************************/
  /*       State Handler        */
  /******************************/
  // Currently running instructions
  logic [NrParallelInstructions-1:0] running_d, running_q;
  `FF(running_q, running_d, '0)

  // New instruction
  logic new_vtl_request;
  assign new_vtl_request = spatz_req_valid && !running_q[spatz_req.id];

  always_comb begin : proc_clear
    clear_active_d = clear_active_q;
    clear_row_d    = clear_row_q;
    clear_id_d     = clear_id_q;

    // Start clearing when a clear_buffer op is freshly latched in the spill.
    if (!clear_active_q && new_vtl_request && spatz_req.op_vtl.clear_buffer) begin
      clear_active_d = 1'b1;
      clear_row_d    = '0;
      clear_id_d     = spatz_req.id;
    end else if (clear_active_q) begin
      if (clear_last_row) begin
        clear_active_d = 1'b0;
      end else begin
        clear_row_d = clear_row_q + 1'b1;
      end
    end
  end

  // Retire the clear op via the SLD response port.
  assign vtl_rsp_valid_o = clear_done;
  assign vtl_rsp_o.id    = clear_id_q;

  always_comb begin : proc_vtl_state
    running_d = running_q;
    spatz_req_ready = !spatz_req_valid;

    // A new spatz_req is received
    if (new_vtl_request) begin
      running_d[spatz_req.id] = 1'b1;
    end

    // VFU-routed ops retire via vfu_rsp.
    if (running_q[vfu_rsp_i.id] && vfu_rsp_valid_i) begin
      running_d[vfu_rsp_i.id] = 1'b0;
    end
    // vventclr retires via clear_done (vtl_rsp_valid_o).
    if (clear_done && running_q[clear_id_q]) begin
      running_d[clear_id_q] = 1'b0;
    end

    // Spill release:
    //   - For vventclr (clear_buffer=1): hold the spill until clearing is
    //     done. Ignores spatz_vfu_req_ready_i because vventclr never goes
    //     to the VFU.
    //   - For other VTL ops: release when the VFU latches the op.
    if (spatz_req_valid && spatz_req.op_vtl.clear_buffer) begin
      if (clear_done) spatz_req_ready = 1'b1;
    end else if (spatz_vfu_req_ready_i) begin
      spatz_req_ready = 1'b1;
    end
  end

  //////////////////////////////////
  //   Index Buffer (FIFO)        //
  //////////////////////////////////
  // 2-deep spill_register used as a FIFO between the master VRF read port
  // and the gather/scatter datapaths. The head is the index for the op
  // currently latched in Ventaglio's spatz_req spill; the tail (when
  // present) is the prefetched index for the next-visible op on
  // `spatz_req_i`. This pairs with Ventaglio's own spatz_req spill: pop
  // on the same advance as the spatz_req spill (vfx ops only — vventclr
  // never pushes), so the two FIFOs march in lock-step.
  //
  // PREFETCH-CANCELLATION INVARIANT
  // The substitution relies on the controller never retracting a VTL op
  // once advertised on spatz_req_i. Verified empirically 2026-06-16
  // across sp-SpMV_2to4 / sp-SpMM_1to4 / sp-SpMM_2to4 (probe was silent
  // on all three). See [[project-index-buffer-spill-register-study]] in
  // ~/.claude memory. The probe formulation (kept as a comment block at
  // the bottom of this section for future revival) tests:
  //   `fetch_in_flight_q` AND
  //   `spatz_req_i.id != fetched_id_q` AND
  //   `spatz_req.id != fetched_id_q` AND
  //   `spatz_req_i.ex_unit==VFU && spatz_req_i.op_vtl.use_vtl`.

  // ── Next-op peek ──
  // Controller's offer must be a *different* VTL op than what's already
  // latched in Ventaglio's spill (otherwise nothing to prefetch).
  logic       next_op_visible;
  spatz_id_t  next_id;
  vreg_t      next_idx_vreg;
  assign next_op_visible = spatz_req_valid && spatz_req_valid_i
                           && spatz_req_i.ex_unit == VFU
                           && spatz_req_i.op_vtl.use_vtl
                           && spatz_req_i.id != spatz_req.id;
  assign next_id        = spatz_req_i.id;
  assign next_idx_vreg  = spatz_req_i.op_vtl.idx_vreg;

  // ── In-flight tracker ──
  // Single-bit flag: set when vrf_re_o is issued, cleared when vrf_rvalid_i
  // returns. Blocks re-issue while a fetch is waiting on a multi-cycle VRF
  // response. Replaces the old fetch_lock_q + target/id/vidx snapshot — the
  // spill register handles "where to deposit" (always the FIFO tail), and
  // the prefetch-cancel invariant (above) guarantees the source address
  // stays stable during the wait, so no address snapshot is needed.
  logic fetch_in_flight_q, fetch_in_flight_d;
  `FF(fetch_in_flight_q, fetch_in_flight_d, 1'b0)

  // Index spill outputs (forward-declared so fetch_for_current can read
  // `idx_spill_valid_o` without a circular dependency).
  logic       idx_spill_valid_o;
  logic       idx_spill_ready_o;
  logic       idx_spill_pop;
  vrf_data_t  index_q;
  logic       index_valid_q;

  // ── Fetch arbitration ──
  // fetch_for_current : on-demand fetch when the FIFO head is empty
  //                     (first op of a stream). Rare in steady state.
  // fetch_for_next    : speculative prefetch for the next op visible on
  //                     spatz_req_i. Gated by `vreg_write_pending_i` so the
  //                     fetch only fires once the controller confirms the
  //                     index vreg has no in-flight writer. Common case.
  logic       fetch_for_current;
  logic       fetch_for_next;
  spatz_id_t  fetch_id_sel;
  vreg_t      fetch_vidx_sel;

  logic spatz_req_is_clear;
  assign spatz_req_is_clear = spatz_req_valid && spatz_req.op_vtl.clear_buffer;

  assign fetch_for_current = spatz_req_valid && !idx_spill_valid_o
                             && !spatz_req_is_clear;
  assign fetch_for_next    = next_op_visible
                          && !vreg_write_pending_i[next_idx_vreg];

  // Priority: current beats next.
  always_comb begin : proc_fetch_sel
    if (fetch_for_current) begin
      fetch_id_sel    = spatz_req.id;
      fetch_vidx_sel  = spatz_req.op_vtl.idx_vreg;
    end else begin // fetch_for_next or idle
      fetch_id_sel    = next_id;
      fetch_vidx_sel  = next_idx_vreg;
    end
  end

  // ── In-flight snapshot ──
  // The master VRF read protocol requires `vrf_re_o`/`vrf_raddr_o`/`vrf_id_o`
  // to be held STABLE from issue until `vrf_rvalid_i` returns; dropping
  // `vrf_re_o` mid-flight cancels (or fails to complete) the read. Snapshot
  // `id`/`vidx` at the issue cycle and hold them across the response wait.
  // This is the spill-substitution analog of the old `fetch_lock_q +
  // fetch_id_q + fetch_vidx_q` mechanism — minus the slot pointer, since
  // the spill itself handles "where to deposit" (always the FIFO tail).
  spatz_id_t fetched_id_q,   fetched_id_d;
  vreg_t     fetched_vidx_q, fetched_vidx_d;
  `FF(fetched_id_q,   fetched_id_d,   '0)
  `FF(fetched_vidx_q, fetched_vidx_d, '0)

  // Effective fetch:
  //   - while in-flight: hold last issued (id, vidx), keep `eff_fetch` high
  //     so vrf_re_o stays asserted until the response lands.
  //   - else: combinationally selected from fetch_id_sel / fetch_vidx_sel
  //     and gated on spill room.
  logic       eff_fetch;
  spatz_id_t  eff_id;
  vreg_t      eff_vidx;
  logic       fresh_fetch;
  assign fresh_fetch = (fetch_for_current || fetch_for_next)
                    && idx_spill_ready_o
                    && !fetch_in_flight_q;
  assign eff_fetch = fetch_in_flight_q || fresh_fetch;
  assign eff_id    = fetch_in_flight_q ? fetched_id_q   : fetch_id_sel;
  assign eff_vidx  = fetch_in_flight_q ? fetched_vidx_q : fetch_vidx_sel;

  // Snapshot id/vidx on the issue cycle so they stay stable while the
  // VRF response is in flight (multi-cycle path).
  always_comb begin
    fetched_id_d   = fetched_id_q;
    fetched_vidx_d = fetched_vidx_q;
    if (fresh_fetch) begin
      fetched_id_d   = fetch_id_sel;
      fetched_vidx_d = fetch_vidx_sel;
    end
  end

  // In-flight FF: set on a fresh issue (no rvalid same cycle), cleared on
  // rvalid. For combinational responses (re=1 and rvalid=1 same cycle),
  // set+clear cancel and the flag stays 0.
  always_comb begin
    fetch_in_flight_d = fetch_in_flight_q;
    if (fresh_fetch)  fetch_in_flight_d = 1'b1;
    if (vrf_rvalid_i) fetch_in_flight_d = 1'b0;
  end

  // ── Index spill register (FIFO substitute for the old ping-pong) ──
  // Push: VRF response landing. Pop: Ventaglio's own spatz_req spill
  // advances for a non-vventclr op (the two FIFOs march in lock-step).
  assign idx_spill_pop = spatz_req_ready && spatz_req_valid
                         && !spatz_req.op_vtl.clear_buffer;

  spill_register #(
    .T(vrf_data_t)
  ) i_idx_spill (
    .clk_i  (clk_i             ),
    .rst_ni (rst_ni            ),
    .data_i (vrf_rdata_i       ),
    .valid_i(vrf_rvalid_i      ),
    .ready_o(idx_spill_ready_o ),
    .data_o (index_q           ),
    .valid_o(idx_spill_valid_o ),
    .ready_i(idx_spill_pop     )
  );

  assign index_valid_q = idx_spill_valid_o;

  // ──────────────────────────────────────────────────────────────────
  // OPT-IN DIAGNOSTIC TRACE
  // Compile with `+define+VTG_TRACE_IDX_SPILL` (or add `+define+...` to
  // VLOG_FLAGS) to enable. Writes per-cycle state + event markers to
  // ventaglio_idx_spill_probe.log in the simulator's CWD. Six tags:
  //   VTG-ISSUE   eff_fetch=1   VTG-PUSH    vrf_rvalid_i=1
  //   VTG-POP     idx_spill_pop  VTG-ADMIT  spatz_vfu_req_ready_i=1
  //   VTG-RETIRE  vfu_rsp_valid_i=1
  //   VTG-STATE   per-cycle dump when ventaglio is doing anything
  // Used 2026-06-16 to diagnose the multi-cycle VRF-read protocol bug
  // that caused the spill_register substitution's first attempt to
  // deadlock at ~20605ns. Kept as a re-enable knob for future debug;
  // not compiled into the normal build.
  `ifdef VTG_TRACE_IDX_SPILL
  // synopsys translate_off
  integer vtg_probe_fh;
  initial begin
    vtg_probe_fh = $fopen("ventaglio_idx_spill_probe.log", "w");
    if (vtg_probe_fh == 0)
      $display("[VTG-PROBE] WARNING: could not open probe log file");
  end
  final begin
    if (vtg_probe_fh != 0) $fclose(vtg_probe_fh);
  end

  always_ff @(posedge clk_i) begin
    if (rst_ni && vtg_probe_fh != 0) begin
      if (eff_fetch) begin
        $fdisplay(vtg_probe_fh, "[VTG-ISSUE t=%0t] eff_id=%0d eff_vidx=%0d vrf_raddr=%h fc=%0d fn=%0d inflight_prev=%0d spill[v=%0d r=%0d]",
                  $time, eff_id, eff_vidx, vrf_raddr_o,
                  fetch_for_current, fetch_for_next, fetch_in_flight_q,
                  idx_spill_valid_o, idx_spill_ready_o);
      end
      if (vrf_rvalid_i) begin
        $fdisplay(vtg_probe_fh, "[VTG-PUSH  t=%0t] rdata_hash=%h spill[v_pre=%0d r_pre=%0d] inflight=%0d req[v=%0d id=%0d]",
                  $time, vrf_rdata_i[31:0], idx_spill_valid_o, idx_spill_ready_o,
                  fetch_in_flight_q, spatz_req_valid, spatz_req.id);
      end
      if (idx_spill_pop) begin
        $fdisplay(vtg_probe_fh, "[VTG-POP   t=%0t] req[v=%0d id=%0d idx=%0d rdy=%0d] spill[v=%0d] head_hash=%h",
                  $time, spatz_req_valid, spatz_req.id, spatz_req.op_vtl.idx_vreg,
                  spatz_req_ready, idx_spill_valid_o, index_q[31:0]);
      end
      if (spatz_vfu_req_ready_i) begin
        $fdisplay(vtg_probe_fh, "[VTG-ADMIT t=%0t] req[v=%0d id=%0d uvtl=%0d cb=%0d] offer[v=%0d id=%0d uvtl=%0d cb=%0d]",
                  $time, spatz_req_valid, spatz_req.id,
                  spatz_req_valid ? spatz_req.op_vtl.use_vtl     : 1'b0,
                  spatz_req_valid ? spatz_req.op_vtl.clear_buffer : 1'b0,
                  spatz_req_valid_i, spatz_req_i.id,
                  spatz_req_i.op_vtl.use_vtl, spatz_req_i.op_vtl.clear_buffer);
      end
      if (vfu_rsp_valid_i) begin
        $fdisplay(vtg_probe_fh, "[VTG-RETIRE t=%0t] rsp_id=%0d running=%b",
                  $time, vfu_rsp_i.id, running_q);
      end
      if (spatz_req_valid || fetch_in_flight_q || vrf_rvalid_i || eff_fetch
          || idx_spill_valid_o || clear_active_q) begin
        $fdisplay(vtg_probe_fh, "[VTG-STATE t=%0t] req[v=%0d id=%0d idx=%0d cb=%0d uvtl=%0d rdy=%0d] offer[v=%0d id=%0d uvtl=%0d] spill[v=%0d r=%0d] fetch[fc=%0d fn=%0d eff=%0d eff_id=%0d eff_idx=%0d inflight=%0d] vrf[re=%0d raddr=%h rv=%0d] cons[ig=%0d is=%0d re=%0d raddr=%h rv=%0d idxv=%0d] cnt[b=%0d vrf=%0d]",
                  $time,
                  spatz_req_valid, spatz_req.id,
                  spatz_req_valid ? spatz_req.op_vtl.idx_vreg     : '0,
                  spatz_req_valid ? spatz_req.op_vtl.clear_buffer : 1'b0,
                  spatz_req_valid ? spatz_req.op_vtl.use_vtl      : 1'b0,
                  spatz_req_ready,
                  spatz_req_valid_i, spatz_req_i.id, spatz_req_i.op_vtl.use_vtl,
                  idx_spill_valid_o, idx_spill_ready_o,
                  fetch_for_current, fetch_for_next, eff_fetch,
                  eff_id, eff_vidx, fetch_in_flight_q,
                  vrf_re_o, vrf_raddr_o, vrf_rvalid_i,
                  is_gather, is_scatter, re_i, raddr_i, rvalid_o, index_valid_q,
                  op_beat_cnt_q, vreg_idx_counter_q);
        $fflush(vtg_probe_fh);
      end
    end
  end
  // synopsys translate_on
  `endif // VTG_TRACE_IDX_SPILL

  // Vector register file counter signals (gather multi-beat).
  logic      vreg_idx_counter_en;
  vrf_addr_t vreg_idx_counter_d;
  vrf_addr_t vreg_idx_counter_q;
  `FF(vreg_idx_counter_q, vreg_idx_counter_d, '0)

  always_comb begin : proc_idx_counter
    vreg_idx_counter_d = vreg_idx_counter_q;
    if (vreg_idx_counter_en)
      vreg_idx_counter_d = vreg_idx_counter_q + 1'b1;
    if (running_q[vfu_rsp_i.id] && vfu_rsp_valid_i)
      vreg_idx_counter_d = '0;
  end

  always_comb begin : proc_idx_addr_gen
    // Multi-beat counter only applies to the active op (gather flows);
    // prefetch always starts at counter=0.
    if (eff_id != spatz_req.id) begin
      vrf_raddr_o = {eff_vidx, $clog2(NrWordsPerVector)'(1'b0)};
    end else begin
      vrf_raddr_o = {eff_vidx, $clog2(NrWordsPerVector)'(1'b0)} + vreg_idx_counter_q;
      if (vreg_idx_counter_en)
        vrf_raddr_o = {eff_vidx, $clog2(NrWordsPerVector)'(1'b0)} + vreg_idx_counter_q + 1'b1;
    end
  end

  ////////////////////////////////
  // Counter for each operation //
  ////////////////////////////////

  // signals for gather or scatter
  logic is_gather, is_scatter;

  logic req_proceed;
  logic last_addr_bit_d, last_addr_bit_q;
  `FF(last_addr_bit_q, last_addr_bit_d, '0)

  assign last_addr_bit_d = raddr_i[0];
  assign req_proceed = last_addr_bit_d ^ last_addr_bit_q;

  // Beats per op = vl / elements_per_beat, where elements_per_beat =
  // VRFWordBWidth >> vsew (VRFWordBWidth = N_FU * ELENB). Derivation in
  // README ("Beats per operation"). Truncates for non-aligned vl, which
  // vsetvli prevents in current kernels (VLMAX is a multiple of width).
  logic [4:0] num_beats_per_op;
  assign num_beats_per_op =
      spatz_req.vl >> ($clog2(VRFWordBWidth) - int'(spatz_req.vtype.vsew));

  logic [4:0] op_beat_cnt_d, op_beat_cnt_q;
  `FF(op_beat_cnt_q, op_beat_cnt_d, '0)

  // Per-op beat counter. Kept as an explicit hook for a future long-vector
  // index-refresh: when a single op spans more beats than the index buffer
  // holds, this counter triggers a mid-op refresh. The wrap is gated on
  // `rvalid_o && req_proceed` so the counter only ever wraps the cycle it
  // actually advances — otherwise an idle cycle at `_q == max-1` would
  // silently reset it.
  always_comb begin
    op_beat_cnt_d = op_beat_cnt_q;
    if (is_gather && rvalid_o && req_proceed) begin
      op_beat_cnt_d = (op_beat_cnt_q == num_beats_per_op - 1) ? '0
                                                              : op_beat_cnt_q + 1;
    end
  end

  // VRF master interface — driven by the effective (locked-or-fresh) fetch.
  // The controller's standard scoreboard still gates the read via the RAW
  // dep on op_vtl.idx_vreg of whichever op we're fetching for.
  assign vrf_re_o    = eff_fetch;
  assign vrf_id_o[0] = eff_id;

  /******************************/
  /*           Types            */
  /******************************/

  // The input address has type `vrf_addr_t`
  // It is used to address `NRVREG * NrWordsPerVector` words
  // Word is represented as `N_FU * ELEN`, is the bandwidth VPU or VLSU comsume
  // In VTL, each channel provide `N_FU * ELEN` bandwidth as well, so it is channel addressable

  // We currently utilized a interleaved address scheme
  // Word 0             -> Channel 0 Row 0
  // Word 1             -> Channel 1 Row 0
  // Word VTGNrChannels -> Channel 0 Row 1 ...


  // `f_channel` function extract the channel index
  function automatic logic [$clog2(VTGNrChannels)-1:0] f_channel(vrf_addr_t addr);
    f_channel = addr[$clog2(VTGNrChannels)-1:0];
  endfunction: f_channel

  // `f_row` function extract the word (row) index within one channel
  function automatic logic [$clog2(VTGNrWordsPerChannel)-1:0] f_row(vrf_addr_t addr);
    f_row = addr[$clog2(VTGNrWordsPerChannel * VTGNrChannels)-1:$clog2(VTGNrChannels)];
  endfunction: f_row

  // (vtg_row_addr_t typedef defined earlier alongside the clear sequencer)

  /******************************/
  /*          Signals           */
  /******************************/

  // signals for gather or scatter
  logic gather_done;
  `FF(gather_done, spatz_vfu_req_ready_i&&spatz_req_valid, '0)

  // `rgather_en_i` and `wscatter_en_i` signals are attached in VRF bypass logic
  always_comb begin
    is_gather  = spatz_req.op_vtl.gather_vd && rgather_en_i;
    is_scatter = spatz_req.op_vtl.scatter_vd && wscatter_en_i;
  end

  // write signals
  vtg_row_addr_t          [VTGNrChannels-1:0] waddr;
  ventaglio_narrow_data_t [VTGNrChannels-1:0] wdata;
  logic                   [VTGNrChannels-1:0] we;
  ventaglio_narrow_be_t   [VTGNrChannels-1:0] wbe;

  // read signals
  vtg_row_addr_t          [VTGNrChannels-1:0] raddr;
  ventaglio_narrow_data_t [VTGNrChannels-1:0] rdata;

  // write mapping
  logic [VTGNrChannels-1:0]                    write_request;
  logic [VTGNrChannels-1:0][VTGNrChannels-1:0] scatter_write_request;

  // post scatter signals
  logic      [VTGNrChannels-1:0] scatter_we;
  vrf_addr_t [VTGNrChannels-1:0] scatter_waddr;
  vrf_data_t [VTGNrChannels-1:0] scatter_wdata;
  vrf_be_t   [VTGNrChannels-1:0] scatter_wbe;

  logic      [VTGNrChannels-1:0] scatter_wvalid;

  logic                          post_scatter_wvalid;

  always_comb begin: gen_write_request
    for (int channel = 0; channel < VTGNrChannels; channel++) begin
      write_request[channel] = we_i && f_channel(waddr_i) == channel && !is_scatter;
    end
    // scatter requests
    for (int b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin   // channels from the bank side
      for (int s_channel = 0; s_channel < VTGNrChannels; s_channel++) begin // channels from the scatter datapath side
        scatter_write_request[b_channel][s_channel] = scatter_we[s_channel] && f_channel(scatter_waddr[s_channel]) == b_channel && is_scatter;
      end
    end
  end: gen_write_request

  always_comb begin : proc_write
    waddr          = '0;
    wdata          = '0;
    we             = '0;
    wbe            = '0;
    wvalid_o       = 1'b0;
    scatter_wvalid = '0;

    if (clear_active_q) begin // priority 0: vventclr — zero every cell
      for (int unsigned channel = 0; channel < VTGNrChannels; channel++) begin
        waddr[channel] = clear_row_q;
        wdata[channel] = '0;
        we[channel]    = 1'b1;
        wbe[channel]   = '1;
      end
      // wvalid_o stays 0: no slave-port write is being acknowledged.
    end else if (!is_scatter) begin // priority 1: normal requests
      for (int unsigned channel = 0; channel < VTGNrChannels; channel++) begin
        if (write_request[channel]) begin
          waddr[channel] = f_row(waddr_i);
          wdata[channel] = wdata_i;
          we[channel]    = 1'b1;
          wbe[channel]   = wbe_i;
          wvalid_o       = 1'b1;
        end
      end
    end else begin // priority 2: scatter requests
      for (int unsigned b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin
        for (int unsigned s_channel = 0; s_channel < VTGNrChannels; s_channel++) begin
          if (scatter_write_request[b_channel][s_channel]) begin
            waddr[b_channel]           = f_row(scatter_waddr[s_channel]);
            wdata[b_channel]           = scatter_wdata[s_channel];
            we[b_channel]              = 1'b1;
            wbe[b_channel]             = scatter_wbe[s_channel];
            scatter_wvalid[s_channel]  = 1'b1;
          end
        end
      end
      wvalid_o = post_scatter_wvalid;
    end

  end : proc_write

  // read mapping

  // read_request: non-gather request signals
  // gathered_read_request: gather request signals
  logic [VTGNrChannels-1:0]                    read_request;

  logic      [VTGNrChannels-1:0][VTGNrChannels-1:0] gather_read_request;
  logic      [VTGNrChannels-1:0]                    gather_re;
  vrf_addr_t [VTGNrChannels-1:0]                    gather_raddr;

  vrf_data_t [VTGNrChannels-1:0]                    gather_rdata;
  logic      [VTGNrChannels-1:0]                    gather_rvalid;

  // post gather signals
  vrf_data_t post_gather_rdata;
  logic      post_gather_rvalid;

  always_comb begin: gen_read_request
    // normal requests
    for (int channel = 0; channel < VTGNrChannels; channel++) begin
      read_request[channel] = re_i && f_channel(raddr_i) == channel && !(is_gather);
    end
    // gathered requests
    for (int b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin   // channels from the bank side
      for (int g_channel = 0; g_channel < VTGNrChannels; g_channel++) begin // channels from the gather datapath side
        gather_read_request[b_channel][g_channel] = gather_re[g_channel] && f_channel(gather_raddr[g_channel]) == b_channel && is_gather;
      end
    end
  end: gen_read_request

  // this should be extended for gather
  always_comb begin : proc_read
    raddr    = '0;
    rvalid_o = 1'b0;
    rdata_o  = 'x;

    gather_rvalid = '0;
    gather_rdata  = 'x;

    if (!is_gather) begin // priority 1: normal read requests
      for (int unsigned b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin // channels from the bank side
        if (read_request[b_channel]) begin
          raddr[b_channel] = f_row(raddr_i);
          rdata_o          = rdata[b_channel];
          rvalid_o         = 1'b1;
        end
      end
    end else if (is_gather) begin // priority 2: gather accesses
      for (int unsigned b_channel = 0; b_channel < VTGNrChannels; b_channel++) begin // channels from the bank side
        for (int unsigned g_channel = 0; g_channel < VTGNrChannels; g_channel++) begin // channels from the gather datapath side
          if (gather_read_request[b_channel][g_channel]) begin
            raddr[b_channel]          = f_row(gather_raddr[g_channel]);
            gather_rdata[g_channel]   = rdata[b_channel];
            gather_rvalid[g_channel]  = index_valid_q;
          end
        end
      end
      // assign the gathered data to the output
      rdata_o  = post_gather_rdata;
      rvalid_o = post_gather_rvalid;
    end
  end

  /******************************/
  /*      Scatter DataPath      */
  /******************************/
  ventaglio_scatter #(
    .NarrowDataWidth (NarrowDataWidth),
    .WideDataWidth   (WideDataWidth)
  ) i_vtl_scatter (
    .clk_i       (clk_i),
    .rst_ni      (rst_ni),
    .testmode_i  (testmode_i),
    // narrow ports
    .waddr_i     (waddr_i),
    .wdata_i     (wdata_i),
    .we_i        (we_i && is_scatter),
    .wbe_i       (wbe_i),
    .wvalid_o    (post_scatter_wvalid),
    // wide ports
    .waddr_o     (scatter_waddr),
    .wdata_o     (scatter_wdata),
    .we_o        (scatter_we),
    .wbe_o       (scatter_wbe),
    .wvalid_i    (scatter_wvalid),
    // control
    // TODO: it is better to implement as a counter to track
    .scatter_done_i      (!is_scatter),
    .num_beats_per_op_i  (num_beats_per_op),
    // controls
    .index_i       (index_q                ),
    .vtl_cfg_i     (spatz_req.op_vtl.sp_cfg),
    .index_valid_i (index_valid_q) // meaning a new read data available
  );


  /******************************/
  /*           Buffer           */
  /******************************/

  // Buffer has `VTGNrChannels` channels
  // Each channel is divided into `N_FU` banks, whose width is `ELEN`
  // In this way, each channel can provide the same bandwidth as the VLSU and VPU
  for (genvar channel = 0; channel < VTGNrChannels; channel++) begin : gen_vtg_channels
    for (genvar bank = 0; bank < N_FU; bank++) begin: gen_vtg_banks
      ventaglio_regfile #(
        .NrWords    (VTGNrWordsPerChannel ),
        .WordWidth  (ELEN                 )
      ) i_vtg_vregfile (
        .clk_i     (clk_i                              ),
        .rst_ni    (rst_ni                             ),
        .testmode_i(testmode_i                         ),
        .waddr_i   (waddr[channel]                     ),
        .wdata_i   (wdata[channel][ELEN*bank +: ELEN]  ),
        .we_i      (we[channel]                        ),
        .wbe_i     (wbe[channel][ELENB*bank +: ELENB]  ),
        .raddr_i   (raddr[channel]                     ),
        .rdata_o   (rdata[channel][ELEN*bank +: ELEN]  )
      );
    end
  end

  /******************************/
  /*      Gather  DataPath      */
  /******************************/

  ventaglio_gather #(
    .NarrowDataWidth (NarrowDataWidth),
    .WideDataWidth   (WideDataWidth)
  ) i_vtl_gather (
    .clk_i       (clk_i),
    .rst_ni      (rst_ni),
    .testmode_i  (testmode_i),
    // narrow ports
    .raddr_i     (raddr_i              ),
    .re_i        (re_i && is_gather    ),
    .rdata_o     (post_gather_rdata),
    .rvalid_o    (post_gather_rvalid),
    // wide ports
    .raddr_o     (gather_raddr         ),
    .re_o        (gather_re            ),
    .rdata_i     (gather_rdata         ),
    .rvalid_i    (gather_rvalid        ),
    // control
    .gather_done_i(gather_done          ),
    // index cfg
    .index_i     (index_q              ),
    .vtl_cfg_i   (spatz_req.op_vtl.sp_cfg),
    .load_index_o(vreg_idx_counter_en)
  );

  /******************************/
  /*      Write Requests        */
  /******************************/

  // RESERVED: master VRF write port is held idle. A future scatter→VRF
  // write-back path will drive these to drain bank results to the regular
  // VRF. Keeping the port plumbed so the integration in spatz_vrf.sv does
  // not need to change when that path lands.
  assign vrf_we_o    = '0;
  assign vrf_wbe_o   = '0;
  assign vrf_waddr_o = '0;
  assign vrf_wdata_o = '0;

endmodule : ventaglio
