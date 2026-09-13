// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The vector load/store unit is used to load vectors from memory
// and to the vector register file and store them back again.

module spatz_vlsu
  import spatz_pkg::*;
  import rvv_pkg::*;
  import cf_math_pkg::idx_width; #(
    parameter int unsigned   NrMemPorts         = 1,
    parameter int unsigned   NrOutstandingLoads = 16,
    // Usable tile resp ports for the 2-wide burst receive (TwinROB0). 1 = legacy
    // single-beat receive (all TwinROB0 logic const-folds out, bit-identical netlist).
    parameter int unsigned   NumRespPorts       = 1,
    // Memory request
    parameter  type          spatz_mem_req_t    = logic,
    parameter  type          spatz_mem_rsp_t    = logic,
    // Dependant parameters. DO NOT CHANGE!
    localparam int  unsigned IdWidth            = idx_width(NrOutstandingLoads),
    // Beats of one port-0 burst received per cycle (parity drain: even beats arrive on
    // mem port 0, odd beats on mem port 1; both land in ROB0's single contiguous id range).
    // TwinROB0 is gone: burst beats now reach every ROB by the ordinary word->port
    // rule, so no port needs a second write/read port or parity bookkeeping. Pinned
    // to 1 so every `BurstRecvPorts > 1` generate const-folds away. NumRespPorts is
    // kept on the interface but no longer selects a receive datapath.
    localparam int  unsigned BurstRecvPorts     = 1
  ) (
    input  logic                            clk_i,
    input  logic                            rst_ni,
    // Spatz request
    input  spatz_req_t                      spatz_req_i,
    input  logic                            spatz_req_valid_i,
    output logic                            spatz_req_ready_o,
    // VLSU response
    output logic                            vlsu_rsp_valid_o,
    output vlsu_rsp_t                       vlsu_rsp_o,
    // Interface with the VRF
    output vrf_addr_t                       vrf_waddr_o,
    output vrf_data_t                       vrf_wdata_o,
    output logic                            vrf_we_o,
    output vrf_be_t                         vrf_wbe_o,
    input  logic                            vrf_wvalid_i,
    output spatz_id_t      [2:0]            vrf_id_o,
    output vrf_addr_t      [1:0]            vrf_raddr_o,
    output logic           [1:0]            vrf_re_o,
    input  vrf_data_t      [1:0]            vrf_rdata_i,
    input  logic           [1:0]            vrf_rvalid_i,
    // Memory Request
    output spatz_mem_req_t [NrMemPorts-1:0] spatz_mem_req_o,
    output logic           [NrMemPorts-1:0] spatz_mem_req_valid_o,
    input  logic           [NrMemPorts-1:0] spatz_mem_req_ready_i,
    //  Memory Response
    input  spatz_mem_rsp_t [NrMemPorts-1:0] spatz_mem_rsp_i,
    input  logic           [NrMemPorts-1:0] spatz_mem_rsp_valid_i,
    // Memory Finished
    output logic                            spatz_mem_finished_o,
    output logic                            spatz_mem_str_finished_o,
    // Request-side: pulses once per vector mem instruction when ALL its request beats
    // have been issued to the interconnect (responses may still be in flight).
    output logic                            spatz_mem_req_sent_o
  );

// Include FF
`include "common_cells/registers.svh"


  ////////////////
  // Parameters //
  ////////////////

  localparam int unsigned MemDataWidth  = ELEN;
  localparam int unsigned MemDataWidthB = MemDataWidth/8;
  // Burst emission requires a memory system that EXPANDS a burst_len>1 request into
  // burst_len responses (tcdm_burst_expander or an MSHR that does the same). Against a
  // plain word-granular memory the VLSU charges mem_pending by burst_len, receives one
  // beat, and waits forever: ROB0 keeps its allocated entries, rob_rvalid never asserts
  // and the instruction never completes. Default 1 preserves the burst-capable
  // integration; set SPATZ_VLSU_BURST=0 where the memory side cannot expand.
  localparam bit BurstEn =
    `ifdef SPATZ_VLSU_BURST `SPATZ_VLSU_BURST `else 1 `endif;

  localparam int unsigned MaxBurstWords = spatz_pkg::MaxBurstWords;
  localparam int unsigned BurstLenWidth = spatz_pkg::BurstLenWidth;
  // The bank stripe is the contiguous region owned by one TCDM tile.
  localparam int unsigned TileBurstWords =
    `ifdef SPATZ_TCDM_BANKS_PER_TILE `SPATZ_TCDM_BANKS_PER_TILE
    `elsif NUM_CORES_PER_TILE
      `NUM_CORES_PER_TILE * N_FU * `BANKING_FACTOR
    `else 16 `endif;
  localparam int unsigned TileBurstBytes = TileBurstWords * MemDataWidthB;
  localparam int unsigned TileOffsetWidth = $clog2(TileBurstBytes);
  localparam int unsigned TileCountWidth = $clog2(TileBurstWords + 1);
  localparam int unsigned FullBurstBytes = MaxBurstWords * MemDataWidthB;

  // Share of a burst of `len` words that lands in lane `port`. Beat k goes to lane
  // k % NrMemPorts, so lane p owns beats p, p+NrMemPorts, ... -- ceil((len-p)/NrMemPorts)
  // of them, and none once p >= len. The lanes are deliberately allowed to DIFFER:
  // nothing keeps the reorder buffers' allocation counts equal, because each lane
  // carries its own base id to the burst adapter. That is what lets a burst be any
  // length -- a tail included -- and lets MaxBurstWords be a free parameter rather
  // than a multiple of NrMemPorts.
  // Rows a burst of `len` words occupies: ceil(len/NrMemPorts). EVERY lane allocates this
  // many ids, so all reorder buffers advance by the same amount and ONE base id describes
  // the whole burst. Lanes the last row does not reach take a dummy for their final id.
  function automatic logic [BurstLenWidth-1:0] burst_rows
      (input logic [BurstLenWidth-1:0] len);
    burst_rows = BurstLenWidth'((len + BurstLenWidth'(NrMemPorts - 1)) >> $clog2(NrMemPorts));
  endfunction

  // Real beats lane `port` receives. Differs from burst_rows only on the last row; the
  // difference is exactly the lane's dummy.
  function automatic logic [BurstLenWidth-1:0] burst_port_share
      (input logic [BurstLenWidth-1:0] len, input int unsigned port);
    if (len > BurstLenWidth'(port))
      burst_port_share = BurstLenWidth'((len - BurstLenWidth'(port) +
                                         BurstLenWidth'(NrMemPorts - 1)) >> $clog2(NrMemPorts));
    else
      burst_port_share = '0;
  endfunction

  // R2: commit-metadata FIFO depth. The push in queue_control is gated on
  // !mem_insn_pending_q[mem_spatz_req.id]; mem_insn_pending_q has exactly
  // NrParallelInstructions bits and is indexed by spatz_id_t, the bit is set at the push
  // and cleared only when *that* entry pops (commit_insn_q.id), so each of the
  // NrParallelInstructions ids contributes at most one resident entry and the FIFO can
  // never hold more than NrParallelInstructions. 0 = legacy DEPTH = NrOutstandingLoads
  // (default, bit-identical); 1 = the reachable depth only.
  localparam int unsigned CommitQMin =
    `ifdef SPATZ_VLSU_COMMIT_QMIN `SPATZ_VLSU_COMMIT_QMIN
    `else 0 `endif;
  localparam int unsigned CommitQDepth = CommitQMin ? NrParallelInstructions
                                                    : NrOutstandingLoads;

  // Block ROB-id reservation ( -- the memory-level-parallelism
  // lever). 0 = OFF (default): the port-0 burst allocator walks its ROB ids ONE PER CYCLE, so a
  // 16-beat burst needs 18 cycles from becoming eligible to its request handshake (1 decide +
  // 16 walk + 1 send). 1 = ON: ROB0 grants the whole MaxBurstWords-wide id window in a single
  localparam int unsigned BlockAlloc =
    `ifdef SPATZ_VLSU_BLOCK_ALLOC `SPATZ_VLSU_BLOCK_ALLOC
    `else 0 `endif;
  // Width of one reservation. 1 = feature absent: every added statement is guarded by
  // (BlockWords > 1) and const-folds away, leaving the legacy netlist bit-identical.
  // PER LANE, not per burst. A burst's beats are distributed one word per lane, so a
  // full-length burst takes burst_rows(MaxBurstWords) = MaxBurstWords/NrMemPorts ids in
  // EVERY reorder buffer -- that, not MaxBurstWords, is the window to reserve, and it has
  // to be reserved in all four rather than in ROB0 alone. Reserving MaxBurstWords in ROB0
  // is what the funnel needed; doing it now would allocate four times too many ids in one
  // buffer and none in the others, diverging the allocators the single base id rests on.
  localparam int unsigned BlockWords = (BlockAlloc != 0) ? (MaxBurstWords / NrMemPorts) : 1;


  // H1 dual-load runahead. MaxInflight=1 (default)
  // const-folds every added term away -> bit-identical legacy netlist. MaxInflight=2
  // admits a SECOND burst-safe load while the elder drains. Validated design point:
  // NrOutstandingLoads=64 (two e32,m2 loads = 2*32 ids exactly fill ROB0).
  localparam int unsigned MaxInflight =
    `ifdef SPATZ_VLSU_DUAL_LOAD `SPATZ_VLSU_DUAL_LOAD `else 1 `endif;
  localparam bit Runahead = (MaxInflight > 1);
  localparam int unsigned InflWidth = idx_width(MaxInflight + 1);

  logic [InflWidth-1:0] inflight_d, inflight_q;
  logic                 dual_adv, dual_run, dual_safe, dual_blk, opq_hold, no_older;
  logic                 opq_ready_int;

  //////////////
  // Typedefs //
  //////////////

  typedef logic [IdWidth-1:0] id_t;
  typedef logic [$clog2(NrWordsPerVector*8)-1:0] vreg_elem_t;

  ///////////////////////
  //  Operation queue  //
  ///////////////////////

  spatz_req_t spatz_req_d;

  spatz_req_t mem_spatz_req;
  logic       mem_spatz_req_valid;
  logic       mem_spatz_req_ready;

  spill_register #(
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i                                          ),
    .rst_ni (rst_ni                                         ),
    .data_i (spatz_req_d                                    ),
    .valid_i(spatz_req_valid_i && spatz_req_i.ex_unit == LSU && !opq_hold),
    .ready_o(opq_ready_int                                  ),
    .data_o (mem_spatz_req                                  ),
    .valid_o(mem_spatz_req_valid                            ),
    .ready_i(mem_spatz_req_ready                            )
  );
  // H1: hold a third LSU op out of the VLSU while MaxInflight are already in flight.
  assign spatz_req_ready_o = opq_ready_int && !opq_hold;

  // Convert the vl to number of bytes for all element widths
  always_comb begin: proc_spatz_req
    spatz_req_d = spatz_req_i;

    unique case (spatz_req_i.vtype.vsew)
      EW_8: begin
        spatz_req_d.vl     = spatz_req_i.vl;
        spatz_req_d.vstart = spatz_req_i.vstart;
      end
      EW_16: begin
        spatz_req_d.vl     = spatz_req_i.vl << 1;
        spatz_req_d.vstart = spatz_req_i.vstart << 1;
      end
      EW_32: begin
        spatz_req_d.vl     = spatz_req_i.vl << 2;
        spatz_req_d.vstart = spatz_req_i.vstart << 2;
      end
      default: begin
        spatz_req_d.vl     = spatz_req_i.vl << MAXEW;
        spatz_req_d.vstart = spatz_req_i.vstart << MAXEW;
      end
    endcase
  end: proc_spatz_req

  // Do we have a strided memory access
  logic mem_is_strided;
  assign mem_is_strided = (mem_spatz_req.op == VLSE) || (mem_spatz_req.op == VSSE);

  // Do we have an indexed memory access
  logic mem_is_indexed;
  assign mem_is_indexed = (mem_spatz_req.op == VLXE) || (mem_spatz_req.op == VSXE);

  // Use only port 0 for word-aligned unit-stride vector loads.
  logic [TileCountWidth-1:0] burst_first_tile_words;
  assign burst_first_tile_words = TileCountWidth'(TileBurstWords) -
      TileCountWidth'(mem_spatz_req.rs1[TileOffsetWidth-1:$clog2(MemDataWidthB)]);
  logic use_port0_burst_req;
  logic mem_use_port0_burst;
  logic commit_use_port0_burst;
  // burst_tail_phase / switch_to_tail_phase are GONE, and so are burst_full_bytes_req and
  // burst_has_tail_req -- their last reader was dual_safe. See the note at mem_use_port0_burst.
  logic [NrMemPorts-1:0] mem_port_active;
  logic [N_FU-1:0]       commit_port_active;
  assign use_port0_burst_req =
      BurstEn &&
      mem_spatz_req.op_mem.is_load &&
      !mem_is_strided &&
      !mem_is_indexed &&
      // No element-width restriction. The burst path is WORD granular end to end --
      // mem_remaining_words is bytes >> clog2(MemDataWidthB) and a word's lane is
      // its word index mod NrMemPorts (the `port << MAXEW` term in the non-burst
      // address generation) -- so how many elements sit inside a 32-bit word never
      (mem_spatz_req.vl >= (2 * MemDataWidthB)) &&
      // Total data must fit in one ROB batch to avoid multi-burst deadlock
      // (scoreboard-blocked VRF writes prevent ROB drain between batches).
      // Capacity is now the WHOLE reorder-buffer set, not ROB0 alone: a burst's beats
      // are distributed one lane per word, so each ROB holds vl/NrMemPorts of them.
      // Four times the old ceiling -- 256 B at depth 16, covering LMUL up to 4.
      (mem_spatz_req.vl <= (NrOutstandingLoads * MemDataWidthB * NrMemPorts)) &&
      (mem_spatz_req.rs1[$clog2(MemDataWidthB)-1:0] == '0) &&
      (mem_spatz_req.vl[$clog2(MemDataWidthB)-1:0] == '0) &&
      // Internal splits must finish a complete ROB row: each burst starts at lane 0.
      // A contained load may finish on any lane. Other crossings use the normal
      // word-interleaved path until the allocator supports carrying partial rows.
      (((MaxBurstWords % NrMemPorts) == 0) ||
       (mem_spatz_req.vl <= FullBurstBytes)) &&
      ((mem_spatz_req.vl <= (burst_first_tile_words * MemDataWidthB)) ||
       (((mem_spatz_req.rs1 % (NrMemPorts * MemDataWidthB)) == 0) &&
        ((TileBurstWords % NrMemPorts) == 0) &&
        ((MaxBurstWords % NrMemPorts) == 0))) &&
      // The first beat belongs to lane 0. Non-zero vstart needs lane rotation.
      (mem_spatz_req.vstart == '0);
  // Internal bursts end on complete ROB rows; the final burst may be shorter.
  // A one-word final remainder uses the word path and pads the other ROB lanes.
  assign mem_use_port0_burst     = use_port0_burst_req;
  assign commit_use_port0_burst  = commit_insn_q.use_port0_burst;
  // TwinROB0 2-wide commit window gate (assigned after the commit counters are declared).
  logic commit_pair_active;
  assign mem_port_active =
      mem_use_port0_burst ? {{(NrMemPorts-1){1'b0}}, 1'b1} : {NrMemPorts{1'b1}};
  // A burst is distributed across the ROBs by the ordinary word->port rule, so every
  // FU participates in its commit exactly as on the non-burst path. Masking down to
  // FU0 forced a VRF row to be filled by several PARTIAL writes; a consumer reading
  // between them saw stale lanes, which corrupted every 8th output row.
  assign commit_port_active = {N_FU{1'b1}};

  /////////////
  //  State  //
  /////////////

  typedef enum logic {
    VLSU_RunningLoad, VLSU_RunningStore
  } state_t;
  state_t state_d, state_q;
  `FF(state_q, state_d, VLSU_RunningLoad)


  // Store requests are not allocated in the load ROB, so their outstanding
  // count is not bounded by the load-ID width. Up to NrParallelInstructions
  // vector stores can be in flight, and an e8,m8 strided/indexed store can emit
  // MAXVL one-byte requests across the ports.
  localparam int unsigned MaxStoreRequestsPerPort =
      NrParallelInstructions * ((MAXVL + NrMemPorts - 1) / NrMemPorts);
  localparam int unsigned StoreCountWidth = idx_width(MaxStoreRequestsPerPort + 1);
  typedef logic [StoreCountWidth-1:0] store_count_t;

  store_count_t   [NrMemPorts-1:0] store_count_q;
  store_count_t   [NrMemPorts-1:0] store_count_d;
  logic           [NrMemPorts-1:0] store_req_fire;
  logic           [NrMemPorts-1:0] store_rsp_fire;
  spatz_mem_req_t [NrMemPorts-1:0] spatz_mem_req;
  logic           [NrMemPorts-1:0] spatz_mem_req_valid;
  logic           [NrMemPorts-1:0] spatz_mem_req_ready;

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_store_count_q
    `FF(store_count_q[port], store_count_d[port], '0)
  end: gen_store_count_q

  always_comb begin: proc_store_count
    store_count_d = store_count_q;

    for (int port = 0; port < NrMemPorts; port++) begin
      unique case ({store_req_fire[port], store_rsp_fire[port]})
        2'b10: store_count_d[port] = store_count_q[port] + 1'b1;
        2'b01: begin
          if (store_count_q[port] != '0)
            store_count_d[port] = store_count_q[port] - 1'b1;
        end
        default:;
      endcase
    end
  end: proc_store_count

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_store_handshake
    // Count a request as soon as it enters the output spill register. This
    // includes buffered requests when the downstream TCDM port is stalled.
    assign store_req_fire[port] = spatz_mem_req[port].write &&
                                  spatz_mem_req_valid[port] &&
                                  spatz_mem_req_ready[port];
  `ifdef TARGET_MEMPOOL
    assign store_rsp_fire[port] = spatz_mem_rsp_valid_i[port] &&
                                  spatz_mem_rsp_i[port].write;
  `else
    // Generic HCI responses do not carry a write flag. Preserve the original
    // assumption that loads and stores do not overlap on a port.
    assign store_rsp_fire[port] = spatz_mem_rsp_valid_i[port] &&
                                  (store_count_q[port] != '0);
  `endif
  end: gen_store_handshake

`ifndef SYNTHESIS
  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_store_count_assertions
    always_ff @(posedge clk_i) begin
      if (rst_ni) begin
        assert (!(store_rsp_fire[port] && !store_req_fire[port] &&
                  (store_count_q[port] == '0)))
          else $error("Spatz store response arrived with no outstanding request");
        assert (!(store_req_fire[port] && !store_rsp_fire[port] &&
                  (store_count_q[port] == store_count_t'(MaxStoreRequestsPerPort))))
          else $error("Spatz outstanding-store counter overflow");
      end
    end
  end: gen_store_count_assertions
`endif

  //////////////////////
  //  Reorder Buffer  //
  //////////////////////

  typedef logic [int'(MAXEW)-1:0] addr_offset_t;

  elen_t [NrMemPorts-1:0] rob_wdata;
  id_t   [NrMemPorts-1:0] rob_wid;
  logic  [NrMemPorts-1:0] rob_push;
  logic  [NrMemPorts-1:0] rob_rvalid;
  elen_t [NrMemPorts-1:0] rob_rdata;
  logic  [NrMemPorts-1:0] rob_pop;
  id_t   [NrMemPorts-1:0] rob_rid;
  logic  [NrMemPorts-1:0] rob_req_id;
  id_t   [NrMemPorts-1:0] rob_id;
  logic  [NrMemPorts-1:0] rob_id_valid;
  logic  [NrMemPorts-1:0] rob_full;
  logic  [NrMemPorts-1:0] rob_empty;

  // Block ROB-id reservation (BlockWords > 1; ROB0 only). The ROB owns the room check
  // (room_block_o, NON-STRICT <=) so a VLSU-side bug cannot over-allocate its id space;
  // block_mask_o is the granted window [rob_id, rob_id+BlockWords) as a bitmap, consumed here
  // only by the odd-expected bookkeeping. All const 0 when the knob is off.
  logic  [NrMemPorts-1:0]                         rob_req_block;   // to the ROBs: the GRANT
  logic  [NrMemPorts-1:0]                         rob_blk_req;     // per-port REQUEST
  logic  [NrMemPorts-1:0]                         rob_room_block;
  logic  [NrMemPorts-1:0][NrOutstandingLoads-1:0] rob_block_mask;

  // TwinROB0 (2-wide burst receive): ROB0 gains a second slot-addressed write port (odd burst
  // beats arriving on mem port 1 are steered into ROB0's own id range) and a second in-order
  // read head + dual pop (2 elements/cycle commit). All '0/unused when BurstRecvPorts == 1.
  elen_t [NrMemPorts-1:0] rob_wdata2;
  id_t   [NrMemPorts-1:0] rob_wid2;
  logic  [NrMemPorts-1:0] rob_push2;
  elen_t [NrMemPorts-1:0] rob_rdata2;
  logic  [NrMemPorts-1:0] rob_rvalid2;
  logic  [NrMemPorts-1:0] rob_pop_dual;
  // Which outstanding ROB0 ids are ODD burst beats (expected on mem port 1 under the MSHR's
  // parity drain). Set at id allocation (alloc-walk cnt parity); cleared on ANY consuming push
  // of that id (either port) -- so MSHR-bypassed / group-LOCAL bursts, whose beats all funnel
  // to port 0 under the legacy contract, self-clear their odd bits (no delivery-contract mode
  // exists to violate: the slots are the same either way).
  logic  [NrOutstandingLoads-1:0] burst_odd_expected_d, burst_odd_expected_q;
  `FF(burst_odd_expected_q, burst_odd_expected_d, '0)
  // Beat-parity pattern for a block reservation. Beat k of a burst lands at id base+k and has
  // parity k[0]; since (base+k)[0] == base[0] ^ k[0], the odd beats inside the window are
  // exactly the ids whose LSB differs from base[0]. So this is a 2:1 mux between the two
  // constants 0xAAAA.. and 0x5555.. -- one inverter's worth of logic, not a shifter.
  logic  [NrOutstandingLoads-1:0] burst_odd_alt;
  for (genvar i = 0; i < NrOutstandingLoads; i++) begin : gen_burst_odd_alt
    assign burst_odd_alt[i] = (BlockWords > 1) ? (((i % 2) != 0) ^ rob_id[0][0]) : 1'b0;
  end : gen_burst_odd_alt

  // This lane's final id carries no beat: request it as a dummy.
  logic [NrMemPorts-1:0] rob_req_dummy;
  // ALLOC-SITE TRACE (SPATZ_ROB_ALLOC_TRACE, sim-only): which arm asked for an id this
  // cycle. The four buffers must allocate in lockstep or the single burst base id stops
  // describing them all; when they diverge this says which arm broke step.
  //   1 burst walk   2 burst-row dummy   3 word/burst request   4 store
  //   5 non-burst pad   6 one-word-remainder dummy
  logic [NrMemPorts-1:0][2:0] rob_alloc_site;
  // Non-burst padding: bytes this lane is short of lane 0 for the current instruction,
  // the request granularity that retires the shortfall, and the per-lane pad request.
  logic [NrMemPorts-1:0][idx_width(ELENB+1)-1:0] pad_init;
  logic [NrMemPorts-1:0][idx_width(ELENB+1)-1:0] pad_bytes_q, pad_bytes_d;
  logic [idx_width(ELENB+1)-1:0]                 pad_step;
  logic [NrMemPorts-1:0]                         pad_fire;
  // The head of this lane's buffer is a dummy and must be drained without committing.
  logic [NrMemPorts-1:0] rob_dummy;

  // The reorder buffer decouples the memory side from the register file side.
  // All elements from one side to the other go through it.
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_rob
`ifdef TARGET_MEMPOOL
    reorder_buffer #(
      .DataWidth (ELEN              ),
      .NumWords  (NrOutstandingLoads),
      .NumWrPorts(1),
      .NumRdPorts(1),
      // EVERY ROB now sees a block reservation: a burst allocates in all of them. Four
      // instances of the block logic, but each a quarter of the width the ROB0-only form
      // needed (MaxBurstWords/NrMemPorts vs MaxBurstWords).
      .BlockWords(BlockWords)
    ) i_reorder_buffer (
      .clk_i    (clk_i           ),
      .rst_ni   (rst_ni          ),
      .data_i   (rob_wdata[port] ),
      .id_i     (rob_wid[port]   ),
      .push_i   (rob_push[port]  ),
      .data2_i  (rob_wdata2[port]),
      .id2_i    (rob_wid2[port]  ),
      .push2_i  (rob_push2[port] ),
      .data_o   (rob_rdata[port] ),
      .valid_o  (rob_rvalid[port]),
      .id_read_o(rob_rid[port]   ),
      .pop_i    (rob_pop[port]   ),
      .data2_o  (rob_rdata2[port]),
      .valid2_o (rob_rvalid2[port]),
      .pop_dual_i(rob_pop_dual[port]),
      .id_req_i (rob_req_id[port]),
      .id_dummy_i(rob_req_dummy[port]),
      .dummy_o  (rob_dummy[port]  ),
      .id_o     (rob_id[port]    ),
      .id_valid_o(rob_id_valid[port]),
      .full_o   (rob_full[port]  ),
      .empty_o  (rob_empty[port] ),
      .id_req_block_i(rob_req_block[port] ),
      .room_block_o  (rob_room_block[port]),
      .block_mask_o  (rob_block_mask[port])
    );
`else
    fifo_v3 #(
      .DATA_WIDTH(ELEN              ),
      .DEPTH     (NrOutstandingLoads)
    ) i_reorder_buffer (
      .clk_i     (clk_i           ),
      .rst_ni    (rst_ni          ),
      .flush_i   (1'b0            ),
      .testmode_i(1'b0            ),
      .data_i    (rob_wdata[port] ),
      .push_i    (rob_push[port]  ),
      .data_o    (rob_rdata[port] ),
      .pop_i     (rob_pop[port]   ),
      .full_o    (rob_full[port]  ),
      .empty_o   (rob_empty[port] ),
      .usage_o   (/* Unused */    )
    );
    assign rob_rvalid[port] = !rob_empty[port];
    assign rob_id_valid[port] = 1'b1;
    assign rob_dummy[port]    = 1'b0;   // no dummy allocation without the reorder_buffer
    assign rob_rdata2[port]  = '0;
    assign rob_rvalid2[port] = 1'b0;
    // No block reservation without the reorder_buffer: room stays low, so burst_block_fire is
    // constant 0 and the legacy walk is always the active allocator here.
    assign rob_room_block[port] = 1'b0;
    assign rob_block_mask[port] = '0;
`endif
  end: gen_rob

  // (Odd-expected bitmap maintenance lives after the burst-alloc state declarations below.)

  //////////////////////
  //  Memory request  //
  //////////////////////

  // Is the memory operation valid and are we at the last one?
  logic [NrMemPorts-1:0] mem_operation_valid;
  logic [NrMemPorts-1:0] mem_operation_last;

  // For each memory port we count how many elements we have already loaded/stored.
  // Multiple counters are needed all memory ports can work independent of each other.
  vlen_t [N_FU-1:0]       mem_counter_max;
  logic  [NrMemPorts-1:0] mem_counter_en;
  logic  [NrMemPorts-1:0] mem_counter_load;
  vlen_t [NrMemPorts-1:0] mem_counter_delta;
  vlen_t [NrMemPorts-1:0] mem_counter_d;
  vlen_t [NrMemPorts-1:0] mem_counter_q;
  logic  [NrMemPorts-1:0] mem_port_finished_q;
  // Per-port working values for gen_mem_counter_proc, hoisted out of the generate loop
  // for waveform visibility (RTL convention: no signal declarations inside generate blocks).
  vlen_t [NrMemPorts-1:0] mem_max_elements;
  vlen_t [NrMemPorts-1:0] mem_remaining_bytes;
  vlen_t [NrMemPorts-1:0] mem_remaining_words;

  vlen_t [NrMemPorts-1:0] mem_idx_counter_delta;
  vlen_t [NrMemPorts-1:0] mem_idx_counter_d;
  vlen_t [NrMemPorts-1:0] mem_idx_counter_q;

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_mem (
      .clk_i     (clk_i                  ),
      .rst_ni    (rst_ni                 ),
      .clear_i   (1'b0                   ),
      .en_i      (mem_counter_en[port]   ),
      .load_i    (mem_counter_load[port] ),
      .down_i    (1'b0                   ), // We always count up
      .delta_i   (mem_counter_delta[port]),
      .d_i       (mem_counter_d[port]    ),
      .q_o       (mem_counter_q[port]    ),
      .overflow_o(/* Unused */           )
    );

    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_mem_idx (
      .clk_i     (clk_i                      ),
      .rst_ni    (rst_ni                     ),
      .clear_i   (1'b0                       ),
      .en_i      (mem_counter_en[port]       ),
      .load_i    (mem_counter_load[port]     ),
      .down_i    (1'b0                       ), // We always count up
      .delta_i   (mem_idx_counter_delta[port]),
      .d_i       (mem_idx_counter_d[port]    ),
      .q_o       (mem_idx_counter_q[port]    ),
      .overflow_o(/* Unused */               )
    );

    assign mem_port_finished_q[port] =
        mem_spatz_req_valid &&
        (mem_port_active[port] ? ((!Runahead || !mem_counter_load[port]) &&
                                  (mem_counter_q[port] == mem_counter_max[port])) : 1'b1);
  end: gen_mem_counters

  // Did the current instruction finished the memory requests?
  logic [NrParallelInstructions-1:0] mem_insn_finished_q, mem_insn_finished_d;
  `FF(mem_insn_finished_q, mem_insn_finished_d, '0)

  // Is the current instruction pending?
  logic [NrParallelInstructions-1:0] mem_insn_pending_q, mem_insn_pending_d;
  `FF(mem_insn_pending_q, mem_insn_pending_d, '0)

  ///////////////////
  //  VRF request  //
  ///////////////////

  typedef struct packed {
    spatz_id_t id;

    vreg_t vd;
    vew_e vsew;

    vlen_t vl;
    vlen_t vstart;
    logic [2:0] rs1;

    logic is_load;
    logic is_strided;
    logic is_indexed;
    logic use_port0_burst;
  } commit_metadata_t;

  commit_metadata_t commit_insn_d;
  logic             commit_insn_push;
  commit_metadata_t commit_insn_q;
  logic             commit_insn_pop;
  logic             commit_insn_empty;
  logic             commit_insn_valid;
  logic             commit_insn_full;
  logic  [NrMemPorts-1:0] commit_finished_q;
  logic  [NrMemPorts-1:0] commit_finished_d;
  // Commit-FIFO occupancy for the H1 A5 assertion (sim-only consumer).
  // fifo_v3 drives usage_o as [ADDR_DEPTH-1:0] = idx_width(CommitQDepth) bits. This was one
  // bit wider, so the MSB was never driven and read as 'x -- Spyglass W110 (Error):
  // "Incompatible width for port usage_o (width 6 in fifo_v3) ... actual width 7".
  logic  [idx_width(CommitQDepth)-1:0] commit_usage;

  fifo_v3 #(
    .DEPTH       (CommitQDepth          ),
    .FALL_THROUGH(1'b1                  ),
    .dtype       (commit_metadata_t     )
  ) i_fifo_commit_insn (
    .clk_i     (clk_i            ),
    .rst_ni    (rst_ni           ),
    .flush_i   (1'b0             ),
    .testmode_i(1'b0             ),
    .data_i    (commit_insn_d    ),
    .push_i    (commit_insn_push ),
    .full_o    (commit_insn_full ),
    .data_o    (commit_insn_q    ),
    .empty_o   (commit_insn_empty),
    .pop_i     (commit_insn_pop  ),
    .usage_o   (commit_usage     )
  );

  // R2 bound (elaboration time): the FIFO must be able to hold one entry per in-flight
  // vector-instruction id, which is the most that can ever be resident.
  if (CommitQDepth < NrParallelInstructions)
    $error("[spatz_vlsu] Commit metadata FIFO is shallower than NrParallelInstructions.");

`ifndef VERILATOR
  // pragma translate_off
  // ...and the runtime tripwire for the same bound: the reduced depth must never block a
  // push that DEPTH = NrOutstandingLoads would have accepted. Armed in both elaborations
  // (with the legacy depth commit_insn_full simply never asserts), so it validates the
  // bound before the knob is turned on.
  commit_q_never_blocks : assert property (@(posedge clk_i) disable iff (!rst_ni)
      !(commit_insn_full && mem_spatz_req_valid && !mem_insn_pending_q[mem_spatz_req.id]))
    else $fatal(1, "[spatz_vlsu] Commit metadata FIFO full blocked a new instruction push.");
  // pragma translate_on
`endif

  assign commit_insn_valid = !commit_insn_empty;
  assign commit_insn_d     = '{
      id        : mem_spatz_req.id,
      vd        : mem_spatz_req.vd,
      vsew      : mem_spatz_req.vtype.vsew,
      vl        : mem_spatz_req.vl,
      vstart    : mem_spatz_req.vstart,
      rs1       : mem_spatz_req.rs1[2:0],
      is_load   : mem_spatz_req.op_mem.is_load,
      is_strided: mem_is_strided,
      is_indexed: mem_is_indexed,
      use_port0_burst: use_port0_burst_req
  };

  always_comb begin: queue_control
    // Maintain state
    mem_insn_finished_d = mem_insn_finished_q;
    mem_insn_pending_d  = mem_insn_pending_q;

    // Do not ack anything
    mem_spatz_req_ready = 1'b0;

    // Do not push anything to the metadata queue
    commit_insn_push = 1'b0;

    // Did we start a new instruction?
    if (mem_spatz_req_valid && !mem_insn_pending_q[mem_spatz_req.id] && !commit_insn_full) begin
      mem_insn_pending_d[mem_spatz_req.id] = 1'b1;
      commit_insn_push                     = 1'b1;
    end

    // Mark an instruction memory-finished once all request beats are issued.
    if (&mem_port_finished_q) begin
      mem_insn_finished_d[mem_spatz_req.id] = 1'b1;
    end

    // Advance operation queue only when committed metadata retires.
    if (commit_insn_pop && commit_insn_valid &&
        (commit_insn_q.id == mem_spatz_req.id)) begin
      mem_spatz_req_ready = 1'b1;
    end
    // H1: ... or when the single in-flight burst-safe load has issued ALL its requests
    // and a burst-safe successor waits at the head. Cap: exactly one extra instruction
    // (commit_insn_q.id == mem_spatz_req.id inside dual_adv).
    if (dual_adv) begin
      mem_spatz_req_ready = 1'b1;
    end
    // Retire bookkeeping only when the committed instruction actually pops.
    if (commit_insn_pop) begin
      // Clear the pending/finished bits for the committed instruction.
      // Use commit_insn_q.id to avoid stale/unknown IDs on the response path.
      mem_insn_finished_d[commit_insn_q.id] = 1'b0;
      mem_insn_pending_d[commit_insn_q.id]  = 1'b0;
    end
  end

  // For each FU that we have, count how many elements we have already loaded/stored.
  // Multiple counters are necessary for the case where not every single FU will
  // receive the same number of elements to work through.
  vlen_t [N_FU-1:0]       commit_counter_max;
  logic  [N_FU-1:0]       commit_counter_en;
  logic  [N_FU-1:0]       commit_counter_load;
  vlen_t [N_FU-1:0]       commit_counter_delta;
  vlen_t [N_FU-1:0]       commit_counter_d;
  vlen_t [N_FU-1:0]       commit_counter_q;
  for (genvar fu = 0; fu < N_FU; fu++) begin: gen_vreg_counters
    delta_counter #(
      .WIDTH($bits(vlen_t))
    ) i_delta_counter_vreg (
      .clk_i     (clk_i                   ),
      .rst_ni    (rst_ni                  ),
      .clear_i   (1'b0                    ),
      .en_i      (commit_counter_en[fu]   ),
      .load_i    (commit_counter_load[fu] ),
      .down_i    (1'b0                    ), // We always count up
      .delta_i   (commit_counter_delta[fu]),
      .d_i       (commit_counter_d[fu]    ),
      .q_o       (commit_counter_q[fu]    ),
      .overflow_o(/* Unused */            )
    );

    assign commit_finished_q[fu] = commit_insn_valid && (commit_counter_q[fu] == commit_counter_max[fu]);
    assign commit_finished_d[fu] = commit_insn_valid && ((commit_counter_q[fu] + commit_counter_delta[fu]) == commit_counter_max[fu]);

    // DIAGNOSTIC ONLY (sim-only, pragma translate_off -- no netlist effect, and this does NOT
    // change the completion condition). The two comparisons above are EXACT EQUALITY. If a
    // commit ever advances the counter PAST max, neither can ever match again: the load never
    // completes, mem_finish_ready never asserts, and the destination vector register is never
    // pragma translate_off
`ifndef TARGET_SYNTHESIS
    always_ff @(posedge clk_i) begin
      if (rst_ni && commit_insn_valid && (commit_counter_q[fu] > commit_counter_max[fu]))
        $display("[VLSU OVERSHOOT] t=%0t fu=%0d q=%0d > max=%0d delta=%0d vsew=%0d vl=%0d id=%0d burst=%0b tail=%0b",
                 $time, fu, commit_counter_q[fu], commit_counter_max[fu],
                 commit_counter_delta[fu], commit_insn_q.vsew, commit_insn_q.vl,
                 commit_insn_q.id, commit_use_port0_burst, 1'b0);
    end
`endif
    // pragma translate_on
  end: gen_vreg_counters

  // TwinROB0 2-wide commit window: pairs only at even element offsets and never across the
  // burst-region boundary (the tail region commits 1-wide legacy). An odd vstart self-aligns
  // with one single commit. Const-folds to 0 when BurstRecvPorts == 1.
  assign commit_pair_active = 1'b0;

  ////////////////////////
  // Address Generation //
  ////////////////////////

  elen_t [NrMemPorts-1:0] mem_req_addr;

  vrf_addr_t vd_vreg_addr;
  vrf_addr_t vs2_vreg_addr;

  // Current element index and byte index that are being accessed at the register file
  vreg_elem_t vd_elem_id;
  vreg_elem_t vs2_elem_id_d, vs2_elem_id_q;
  `FF(vs2_elem_id_q, vs2_elem_id_d, '0)

  // Pending indexes
  logic [NrMemPorts-1:0] pending_index;

  // Calculate the memory address for each memory port
  addr_offset_t [NrMemPorts-1:0] mem_req_addr_offset;
  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_req_addr
    logic [31:0] addr;
    logic [31:0] stride;
    logic [31:0] offset;

    // Pre-shuffling index offset
    typedef logic [int'(MAXEW)-1:0] maxew_t;
    maxew_t idx_offset;
    assign idx_offset = mem_idx_counter_q[port];

    always_comb begin
      stride = mem_is_strided ? mem_spatz_req.rs2 >> mem_spatz_req.vtype.vsew : 'd1;

      if (mem_is_indexed) begin
        // What is the relationship between data and index width?
        automatic logic [1:0] data_index_width_diff = int'(mem_spatz_req.vtype.vsew) - int'(mem_spatz_req.op_mem.ew);

        // Pointer to index
        automatic logic [idx_width(N_FU*ELENB)-1:0] word_index = (port << (MAXEW - data_index_width_diff)) + (maxew_t'(idx_offset << data_index_width_diff) >> data_index_width_diff) + (maxew_t'(idx_offset >> (MAXEW - data_index_width_diff)) << (MAXEW - data_index_width_diff)) * NrMemPorts;

        // Index
        unique case (mem_spatz_req.op_mem.ew)
          EW_8 : offset   = $signed(vrf_rdata_i[1][8 * word_index +: 8]);
          EW_16: offset   = $signed(vrf_rdata_i[1][8 * word_index +: 16]);
          default: offset = $signed(vrf_rdata_i[1][8 * word_index +: 32]);
        endcase
      end else begin
        if (mem_use_port0_burst && (port == 0))
          offset = mem_counter_q[port] * stride;
        else
          offset = ({mem_counter_q[port][$bits(vlen_t)-1:MAXEW] << $clog2(NrMemPorts), mem_counter_q[port][int'(MAXEW)-1:0]} + (port << MAXEW)) * stride;
      end

      addr                      = mem_spatz_req.rs1 + offset;
      mem_req_addr[port]        = (addr >> MAXEW) << MAXEW;
      mem_req_addr_offset[port] = addr[int'(MAXEW)-1:0];

      pending_index[port] = (mem_idx_counter_q[port][$clog2(NrWordsPerVector*ELENB)-1:0] >> MAXEW) != vs2_vreg_addr[$clog2(NrWordsPerVector)-1:0];
    end
  end: gen_mem_req_addr

  // Burst request tracking (per port)
  logic [NrMemPorts-1:0]                    burst_mode_req;
  logic [NrMemPorts-1:0]                    burst_use;
  logic [NrMemPorts-1:0][TileCountWidth-1:0] burst_tile_words;
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_burst_tile_words
    assign burst_tile_words[port] = TileCountWidth'(TileBurstWords) -
        TileCountWidth'(mem_req_addr[port][TileOffsetWidth-1:$clog2(MemDataWidthB)]);
  end
  logic [NrMemPorts-1:0][BurstLenWidth-1:0] burst_len_calc;
  logic [NrMemPorts-1:0][BurstLenWidth-1:0] burst_len_eff;
  logic [NrMemPorts-1:0][BurstLenWidth-1:0] burst_len_issue;
  logic [NrMemPorts-1:0]                    burst_alloc_q, burst_alloc_d;
  logic [NrMemPorts-1:0][BurstLenWidth-1:0] burst_len_q, burst_len_d;
  logic [NrMemPorts-1:0][BurstLenWidth-1:0] burst_alloc_cnt_q, burst_alloc_cnt_d;
  id_t  [NrMemPorts-1:0]                    burst_base_id_q, burst_base_id_d;
  logic [NrMemPorts-1:0]                    burst_send;
  logic [NrMemPorts-1:0]                    burst_alloc_fire;
  // Block reservation state. Bursts are port-0 only, so this is ONE flop per core: set the
  // cycle the ROB grants the window, cleared when the burst request handshakes.
  logic                                     burst_reserved_q, burst_reserved_d;
  // Total length of the burst being assembled, in words. Distinct from burst_len_q,
  // which is now each LANE's share of it.
  logic [BurstLenWidth-1:0]                 burst_total_q, burst_total_d;
  logic                                     burst_block_fire;

  // Block reservation request -- driven ONLY from registers ( resolution): burst_alloc_q
  // (a burst is live), !burst_reserved_q (its window is not reserved yet) and
  // burst_alloc_cnt_q == 0 (nothing has been walked for it -- once the legacy fallback walk
  // has taken even one id this drops for good, so the two allocators can never both serve one
  // burst). ~4 levels, and deliberately WITHOUT the combinational burst_use guard, whose
  // ~25-level arrival is exactly what made the naive placement slow.
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_rob_req_block
    // burst_total_q == MaxBurstWords is load-bearing, not conservatism. The ROB grants a
    // window of exactly BlockWords ids; a TAIL needs burst_rows(len) < BlockWords of them,
    // so the grant would over-allocate, and the surplus ids are never pushed -- valid_q
    // never sets under the read head and the lane stalls forever. Every lane over-allocates
    // by the same amount, so the alignment invariant would still hold and the assertion
    // would stay silent: the failure is a hang, not a diverged base id. A full-length burst
    // consumes the window exactly, and a tail falls back to the walk (at most BlockWords-1
    // cycles, which is what distributing the beats already bought down from MaxBurstWords-1).
    assign rob_blk_req[port] = (BlockWords > 1) &&
                               burst_alloc_q[port] && !burst_reserved_q &&
                               (burst_alloc_cnt_q[port] == '0) &&
                               (burst_total_q == BurstLenWidth'(MaxBurstWords));
    // THE ROBs ARE DRIVEN BY THE ALL-PORTS DECISION, NOT BY THEIR OWN REQUEST.
    //
    // reorder_buffer computes block_fire = id_req_block_i && room_block_o internally and
    // allocates on it. Feeding it the per-port request let each buffer decide ALONE: if one
    // lacked room, the other three still advanced their write pointers by BlockWords while it
    // did not, and the VLSU -- which correctly required all four -- fell through to the
    // per-cycle walk and allocated on top. The buffers were then permanently skewed, so the
    // single base id on the request addressed the wrong slot in the lagging lane: its beats
    // arrive, valid_q[read_pointer_q] never sets, and the load never commits. Driving them
    // from the reduction makes "all four or none" structural rather than a promise.
    assign rob_req_block[port] = burst_block_fire;
  end : gen_rob_req_block
  // Term-for-term the reorder_buffer's own internal block_fire (id_req_block_i && room_block_o),
  // so the VLSU state update and the ROB pointer update commit together or not at all -- the
  // F2/A5 divergence class is structurally impossible, not merely asserted. The ROB remains the
  // authority: room_block_o is its output and it re-checks it internally.
  // ALL ports, not port 0: one request covers every lane's share, so a window that exists
  // in only some buffers is worse than none -- it would hand the burst a base id the other
  // buffers have not reserved behind.
  // Every port wants the window AND every buffer has room. Both terms come from registers
  // (rob_blk_req from the allocator flops, room_block_o from status_cnt_q), so feeding this
  // back into id_req_block_i above is not a loop.
  always_comb begin : proc_burst_block_fire
    burst_block_fire = (BlockWords > 1);
    for (int port = 0; port < NrMemPorts; port++)
      burst_block_fire &= rob_blk_req[port] && rob_room_block[port];
  end : proc_burst_block_fire

  // Burst load element/lane tracking (port0 only).
  // Declared HERE rather than beside their assigns further down: burst_word_idx is read by
  // gen_vreg_addr immediately below, and SystemVerilog requires a typed variable to be declared
  // before use. VCS and QuestaSim accept the forward reference, but Spyglass rejects it outright
  // -- "Identifier (burst_word_idx) not declared in current scope" was a FATAL that aborted rule
  // checking for the whole design, so no lint rule ran at all. The assigns stay next to the
  // burst logic they belong to; only the declarations move.
  localparam int unsigned LaneIdxWidth = (N_FU > 1) ? $clog2(N_FU) : 1;
  vreg_elem_t                       burst_elem_idx;
  logic [LaneIdxWidth-1:0]          burst_lane_idx;
  vreg_elem_t                       burst_word_idx;
  logic [ELENB-1:0]                 burst_lane_wbe;

  // Calculate the register file address
  always_comb begin : gen_vreg_addr
    vd_vreg_addr  = (commit_insn_q.vd << $clog2(NrWordsPerVector)) + $unsigned(vd_elem_id);
    vs2_vreg_addr = (mem_spatz_req.vs2 << $clog2(NrWordsPerVector)) + $unsigned(vs2_elem_id_q);
  end

  ///////////////
  //  Control  //
  ///////////////

  // Are we busy?
  logic busy_q, busy_d;
  `FF(busy_q, busy_d, 1'b0)

  // Did we finish an instruction?
  logic vlsu_finished_req;
  logic mem_finish_ready;
  logic store_drain_ready;

  // Memory requests


  always_comb begin: control_proc
    // Maintain state
    busy_d = busy_q;

    // Do not pop anything
    commit_insn_pop = 1'b0;

    // Do not ack anything
    vlsu_finished_req = 1'b0;

    // Finished the execution!
    if (mem_finish_ready) begin
      commit_insn_pop = 1'b1;
      busy_d          = 1'b0;

      // Acknowledge response when the last load commits to the VRF, or when the store finishes
      vlsu_finished_req = 1'b1;
    end
    // Do we have a new instruction?
    else if (commit_insn_valid && !busy_d)
      busy_d = 1'b1;
  end: control_proc

  // Is the VRF operation valid and are we at the last one?
  logic [N_FU-1:0] commit_operation_valid;
  logic [N_FU-1:0] commit_operation_last;

  // Is instruction a load?
  logic mem_is_load;
  logic exec_is_load;
  assign mem_is_load  = mem_spatz_req.op_mem.is_load;
  // Use the active VLSU mode for request classification. Using commit_insn_q
  // can transiently see the next instruction and misclassify in-flight beats.
  assign exec_is_load = (state_q == VLSU_RunningLoad);

  // A store instruction is complete only after counters are done and the
  // buffered store data path is drained.
  assign store_drain_ready = (&rob_empty) && (store_count_q == '0);
  assign mem_finish_ready  = commit_insn_valid &&
                             (&commit_finished_q) &&
                             mem_insn_finished_q[commit_insn_q.id] &&
                             (commit_insn_q.is_load || store_drain_ready);

  // Signal when we are finished with with accessing the memory (necessary
  // for the case with more than one memory port)
  assign spatz_mem_finished_o     = mem_finish_ready;
  assign spatz_mem_str_finished_o = mem_finish_ready && !commit_insn_q.is_load;
  // Request-sent one-shot for the snitch request-sent fence (acc_mem_req_sent_i[1]).
  // Pulses when the current vector mem op has issued ALL its request beats: every active
  // port has reached mem_counter_max (equivalently remaining_words==0). This is the UNGATED
  // issue-complete point -- unlike mem_insn_finished (whose mem_spatz_req_valid qualifier +
  logic [NrMemPorts-1:0] mem_port_req_issued;
  logic                  mem_req_all_issued;
  logic                  mem_req_all_issued_q;
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_mem_port_req_issued
    // Guard with !mem_counter_load: mem_spatz_req_valid rises one cycle BEFORE the registered
    // delta_counter loads mem_counter_q, so on a new instruction's first cycle the counter still
    // holds the PREVIOUS instruction's value -- which equals the new mem_counter_max whenever
    // consecutive ops share vl (e.g. matmul vle32). Without this guard that stale match yields a
    // false "all request beats issued" pulse before any request has been sent.
    assign mem_port_req_issued[port] =
        mem_port_active[port] ?
          (!mem_counter_load[port] && (mem_counter_q[port] == mem_counter_max[port])) : 1'b1;
  end
  assign mem_req_all_issued   = mem_spatz_req_valid && (&mem_port_req_issued);
  `FF(mem_req_all_issued_q, mem_req_all_issued, 1'b0)
  assign spatz_mem_req_sent_o = mem_req_all_issued && !mem_req_all_issued_q;

  // Do we start at the very fist element
  logic mem_is_vstart_zero;
  assign mem_is_vstart_zero = mem_spatz_req.vstart == 'd0;

  if (Runahead) begin : gen_runahead
    // Commit-FIFO occupancy: ++ push, -- pop (simultaneous nets 0). Bookkeeping for
    // D2/opq_hold/A5 only -- the in-flight CAP is the id-equality term in dual_adv.
    always_comb begin
      inflight_d = inflight_q;
      if (commit_insn_push) inflight_d = inflight_d + 1'b1;
      if (commit_insn_pop)  inflight_d = inflight_d - 1'b1;
    end
    `FF(inflight_q, inflight_d, '0)

    // Youngest candidate (op-queue head) vs oldest unretired (commit head): ids differ
    // <=> two instructions co-resident.
    assign dual_run  = mem_spatz_req_valid && commit_insn_valid &&
                       (commit_insn_q.id != mem_spatz_req.id);
    // Shapes allowed to share the buffers/commit with the elder load (all head-derivable).
    //
    // !burst_has_tail_req IS GONE. It excluded any load whose vl is not a whole multiple of
    // a full burst, and it meant something when a tail was a SECOND PHASE of the
    // instruction: switch_to_tail_phase re-based the counters and swapped the port mask
    // mid-flight, so overlapping a younger load with that was genuinely unsafe. A tail is
    // now just a shorter burst from the same allocator, on the same lanes, with the same
    // accounting -- so the condition guarded nothing and only cost runahead on every loop
    // whose trip count is not a multiple of the burst length, which is most of them.
    //
    // The one-word remainder still leaves the burst path (burst_mode_req needs
    // burst_len_eff > 1) and gives lanes 1..N-1 a dummy, but dual_adv demands
    // mem_req_all_issued, so the elder is done issuing -- remainder included -- before the
    // younger advances.
    assign dual_safe = mem_spatz_req.op_mem.is_load && use_port0_burst_req &&
                       mem_is_vstart_zero &&
                       (state_q == VLSU_RunningLoad) && commit_insn_q.is_load;
    // Block the request datapath while an UNSAFE younger instruction sits at the head
    // (store-after-load epilogue, strided/indexed, vstart!=0): its requests would
    // otherwise issue at A's addresses through the commit_insn_q.is_load gates.
    assign dual_blk  = dual_run && !dual_safe;
    // Hold a third LSU op out of the VLSU: the controller's 4-entry id pool does not
    // scale with the ROB (naive runahead strands the VFU). Deadlock-free: inflight_q
    // drops at A's retire, independent of the held instruction.
    assign opq_hold  = (inflight_q >= InflWidth'(MaxInflight));

    // THE CAP: commit_insn_q.id == mem_spatz_req.id is true IFF exactly ONE instruction
    // is in flight (FIFO strictly in-order; ids from the controller pool, released at
    // retire). dual_adv therefore fires only from "1 in flight" -> "2 in flight"; from
    // "2 in flight" the ids differ and no third advance occurs until A's pop.
    assign dual_adv  = mem_req_all_issued &&                    // level, !mem_counter_load-guarded (:773-775)
                       mem_insn_pending_q[mem_spatz_req.id] &&  // head was really admitted
                       !(|burst_alloc_q) &&                     // never mid-alloc (walk fallback window)
                       !commit_insn_push && !commit_insn_full &&
                       commit_insn_valid && (commit_insn_q.id == mem_spatz_req.id) &&
                       commit_insn_q.is_load && (state_q == VLSU_RunningLoad) &&
                       use_port0_burst_req &&
                       mem_is_vstart_zero;

    // mem_pending blanket-clear is only legal when no OLDER instruction survives the
    // cycle (FALL_THROUGH FIFO: commit_insn_valid/empty forms are dead branches,
    // fifo_v3.sv:58 -- the inflight_q form is required). Runahead=0: const 1, the
    // original behaviour (push implies nothing older).
    assign no_older  = (inflight_q == '0) ||
                       ((inflight_q == InflWidth'(1)) && commit_insn_pop);
  end else begin : gen_no_runahead
    assign inflight_q = '0;
    assign dual_adv   = 1'b0;
    assign dual_run   = 1'b0;
    assign dual_safe  = 1'b0;
    assign dual_blk   = 1'b0;
    assign opq_hold   = 1'b0;
    assign no_older   = 1'b1;
  end

  // Is the memory address unaligned
  logic mem_is_addr_unaligned;
  assign mem_is_addr_unaligned = mem_spatz_req.rs1[int'(MAXEW)-1:0] != '0;

  // Do we have to access every single element on its own
  logic mem_is_single_element_operation;
  assign mem_is_single_element_operation = mem_is_addr_unaligned || mem_is_strided || mem_is_indexed || !mem_is_vstart_zero;

  // How large is a single element (in bytes)
  logic [3:0] mem_single_element_size;
  assign mem_single_element_size = 1'b1 << mem_spatz_req.vtype.vsew;

  // How large is an index element (in bytes)
  logic [3:0] mem_idx_single_element_size;
  assign mem_idx_single_element_size = 1'b1 << mem_spatz_req.op_mem.ew;

  // Is the memory address unaligned
  logic commit_is_addr_unaligned;
  assign commit_is_addr_unaligned = commit_insn_q.rs1[int'(MAXEW)-1:0] != '0;

  // Do we have to access every single element on its own
  logic commit_is_single_element_operation;
  assign commit_is_single_element_operation = commit_is_addr_unaligned || commit_insn_q.is_strided || commit_insn_q.is_indexed || (commit_insn_q.vstart != '0);

  // Size of an element in the VRF
  logic [3:0] commit_single_element_size;
  assign commit_single_element_size = 1'b1 << commit_insn_q.vsew;

  // Number of pending load requests (must be declared before offset queue use).
  logic [NrMemPorts-1:0][idx_width(NrOutstandingLoads):0] mem_pending_d, mem_pending_q;
  logic [NrMemPorts-1:0] mem_pending;

  ////////////////////
  //  Offset Queue  //
  ////////////////////

  // Store the offsets of all loads, for realigning
  addr_offset_t [NrMemPorts-1:0] vreg_addr_offset;
  logic [NrMemPorts-1:0] offset_queue_empty;
  logic [NrMemPorts-1:0] offset_queue_full;
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_offset_queue
    fifo_v3 #(
      .DATA_WIDTH(int'(MAXEW)       ),
      .DEPTH     (NrOutstandingLoads)
    ) i_offset_queue (
      .clk_i     (clk_i                                                                ),
      .rst_ni    (rst_ni                                                               ),
      .flush_i   (1'b0                                                                 ),
      .testmode_i(1'b0                                                                 ),
      .empty_o   (offset_queue_empty[port]                                             ),
      .full_o    (offset_queue_full[port]                                              ),
      // Port0 burst mode is always aligned and uses dedicated datapath logic,
      // so offset queue tracking is only needed for non-burst (or tail) loads.
      .push_i    (exec_is_load &&
                  !mem_use_port0_burst &&
                  ((spatz_mem_req_valid[port] && spatz_mem_req_ready[port]) && !burst_use[port])),
      .data_i    (mem_req_addr_offset[port]                                            ),
      .data_o    (vreg_addr_offset[port]                                               ),
      // Pop only when a real pending load response is consumed.
      .pop_i     (rob_pop[port] && commit_insn_q.is_load &&
                  mem_pending[port] &&
                  !offset_queue_empty[port]                                             ),
      .usage_o   (/* Unused */                                                         )
    );
  end: gen_offset_queue

  ///////////////////////
  //  Output Register  //
  ///////////////////////

  typedef struct packed {
    vrf_addr_t waddr;
    vrf_data_t wdata;
    vrf_be_t wbe;

    vlsu_rsp_t rsp;
    logic rsp_valid;
  } vrf_req_t;

  vrf_req_t vrf_req_d, vrf_req_q;
  logic     vrf_req_valid_d, vrf_req_ready_d;
  logic     vrf_req_valid_q, vrf_req_ready_q;

  spill_register #(
    .T(vrf_req_t)
  ) i_vrf_req_register (
    .clk_i  (clk_i          ),
    .rst_ni (rst_ni         ),
    .data_i (vrf_req_d      ),
    .valid_i(vrf_req_valid_d),
    .ready_o(vrf_req_ready_d),
    .data_o (vrf_req_q      ),
    .valid_o(vrf_req_valid_q),
    .ready_i(vrf_req_ready_q)
  );

  assign vrf_waddr_o     = vrf_req_q.waddr;
  assign vrf_wdata_o     = vrf_req_q.wdata;
  assign vrf_wbe_o       = vrf_req_q.wbe;
  assign vrf_we_o        = vrf_req_valid_q;
  assign vrf_id_o        = {vrf_req_q.rsp.id, mem_spatz_req.id, commit_insn_q.id};
  assign vrf_req_ready_q = vrf_wvalid_i;
  // Ack when the vector store finishes, or when the vector load commits to the VRF
  assign vlsu_rsp_o       = vrf_req_q.rsp_valid && vrf_req_valid_q ? vrf_req_q.rsp   : '{id: commit_insn_q.id, default: '0};
  assign vlsu_rsp_valid_o = vrf_req_q.rsp_valid && vrf_req_valid_q ? vrf_req_ready_q : vlsu_finished_req && !commit_insn_q.is_load;

  //////////////
  // Counters //
  //////////////

  // Do we need to catch up to reach element idx parity? (Because of non-zero vstart)
  vlen_t vreg_start_0;
  assign vreg_start_0 = vlen_t'(commit_insn_q.vstart[$clog2(ELENB)-1:0]);
  assign burst_elem_idx = commit_counter_q[0] >> $clog2(ELENB);
  assign burst_lane_idx = burst_elem_idx[LaneIdxWidth-1:0];
  assign burst_word_idx = burst_elem_idx >> $clog2(N_FU);
  always_comb begin
    burst_lane_wbe = '1;
    if (commit_is_single_element_operation) begin
      automatic logic [$clog2(ELENB)-1:0] shift = commit_counter_q[0][$clog2(ELENB)-1:0];
      automatic logic [ELENB-1:0] mask          = '1;
      case (commit_insn_q.vsew)
        EW_8 : mask   = 1;
        EW_16: mask   = 3;
        EW_32: mask   = 15;
        default: mask = '1;
      endcase
      burst_lane_wbe = mask << shift;
    end
  end
  logic [N_FU-1:0] catchup;
  for (genvar i = 0; i < N_FU; i++) begin: gen_catchup
    assign catchup[i] = (commit_counter_q[i] < vreg_start_0) & (commit_counter_max[i] != commit_counter_q[i]);
  end: gen_catchup

  for (genvar fu = 0; fu < N_FU; fu++) begin: gen_vreg_counter_proc
    // The total amount of elements we have to work through
    vlen_t max_elements;

    always_comb begin
      // Default value
      max_elements = (commit_insn_q.vl >> $clog2(N_FU*ELENB)) << $clog2(ELENB);

      // Full transfer
      begin
        if (commit_insn_q.vl[$clog2(ELENB) +: $clog2(N_FU)] > fu)
          max_elements += ELENB;
        else if (commit_insn_q.vl[$clog2(N_FU*ELENB)-1:$clog2(ELENB)] == fu)
          max_elements += commit_insn_q.vl[$clog2(ELENB)-1:0];
      end

      commit_counter_load[fu] = commit_insn_pop;
      begin
        // ONE form for burst and non-burst alike. The burst arm this replaces gave lane 0 the
        // whole vstart and the others zero -- the funnel's accounting, the same remnant
        // fe49caa removed from max_elements just above and missed here. Behaviourally
        // identical today (a burst demands vstart == 0), but it is the shape that would
        // silently mis-seed every lane the moment that changed.
        commit_counter_d[fu] = (commit_insn_q.vstart >> $clog2(N_FU*ELENB)) << $clog2(ELENB);
        if (commit_insn_q.vstart[$clog2(N_FU*ELENB)-1:$clog2(ELENB)] > fu)
          commit_counter_d[fu] += ELENB;
        else if (commit_insn_q.vstart[idx_width(N_FU*ELENB)-1:$clog2(ELENB)] == fu)
          commit_counter_d[fu] += commit_insn_q.vstart[$clog2(ELENB)-1:0];
      end
      commit_operation_valid[fu] = commit_port_active[fu] &&
                                   commit_insn_valid &&
                                   (commit_counter_q[fu] != max_elements) &&
                                   (catchup[fu] || (!catchup[fu] && ~|catchup));
      commit_operation_last[fu]  = commit_operation_valid[fu] &&
                                   ((max_elements - commit_counter_q[fu]) <=
                                    (commit_is_single_element_operation ? commit_single_element_size : ELENB));
      commit_counter_delta[fu]   = !commit_operation_valid[fu] ? vlen_t'('d0) :
                                   commit_is_single_element_operation ? vlen_t'(commit_single_element_size) :
                                   commit_operation_last[fu] ? (max_elements - commit_counter_q[fu]) : vlen_t'(ELENB);
      commit_counter_en[fu]      = commit_operation_valid[fu] &&
                                   (commit_insn_q.is_load && vrf_req_valid_d && vrf_req_ready_d) ||
                                   (!commit_insn_q.is_load && vrf_rvalid_i[0] && vrf_re_o[0] && (!mem_is_indexed || vrf_rvalid_i[1]));
      commit_counter_max[fu]     = max_elements;
    end
  end

  assign vd_elem_id = (commit_counter_q[0] > vreg_start_0) ? commit_counter_q[0] >> $clog2(ELENB) : commit_counter_q[N_FU-1] >> $clog2(ELENB);

  for (genvar port = 0; port < NrMemPorts; port++) begin: gen_mem_counter_proc
    // max_elements / remaining_bytes / remaining_words are hoisted to
    // module scope (mem_*) for waveform visibility -- declared near the mem counters above.
    always_comb begin
      if (mem_use_port0_burst && (port != 0)) begin
        mem_max_elements[port]             = '0;
        mem_remaining_bytes[port]          = 0;
        mem_remaining_words[port]          = 0;
        burst_len_calc[port]     = '0;
        // Same reason as the burst_mode_req default below: pad_init is read in ANOTHER
        // always_comb (proc pad_bytes), so leaving it unassigned on this arm latches it.
        // A burst pads nothing -- the padding is the non-burst path's business, and
        // pad_fire demands !mem_use_port0_burst -- so zero is the correct value here.
        pad_init[port]           = '0;
        // Assigned on this path too, or the incomplete always_comb infers a latch on
        // burst_mode_req_reg[1] (Spyglass SYNTH_12608). Functionally a no-op: the only read
        // reachable from here is `!burst_mode_req[port] || !burst_use[port]` at the burst-issue
        // arm below, and burst_use[port] is forced to 0 on the very next line, so that
        // disjunction is 1 either way. The other read sits in the else branch, downstream of a
        // fresh assignment. Removing the latch is for the netlist (area/DFT/timing), not behaviour.
        burst_mode_req[port]     = 1'b0;
        burst_use[port]          = 1'b0;
        mem_operation_valid[port]= 1'b0;
        mem_operation_last[port] = 1'b0;
        mem_counter_load[port]   = commit_insn_push;
        mem_counter_d[port]      = '0;
        mem_counter_delta[port]  = '0;
        mem_counter_en[port]     = 1'b0;
        mem_counter_max[port]    = '0;
        mem_idx_counter_d[port]  = '0;
        mem_idx_counter_delta[port] = '0;
      end else begin
        // Default value
        if (mem_use_port0_burst)
          mem_max_elements[port] = mem_spatz_req.vl;
        else
          mem_max_elements[port] = (mem_spatz_req.vl >> $clog2(NrMemPorts*MemDataWidthB)) << $clog2(MemDataWidthB);

        if (!mem_use_port0_burst) begin
          if (NrMemPorts == 1)
            mem_max_elements[port] = mem_spatz_req.vl;
          else
            if (mem_spatz_req.vl[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
              mem_max_elements[port] += MemDataWidthB;
            else if (mem_spatz_req.vl[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
              mem_max_elements[port] += mem_spatz_req.vl[$clog2(MemDataWidthB)-1:0];
        end

        // Lane 0 always issues the most: the remainder distribution above gives the extra
        // element to the LOW ports, so every other lane pads up to lane 0's count.
        //
        // LOADS ONLY. A STORE allocates its ids from vrf_rvalid_i[0] -- one per VRF read, on
        // EVERY lane, with no per-lane work term -- so it is already uniform and needs no
        // padding at all. Loading a store's element-count imbalance here made a lane pad for
        // an imbalance that never existed: measured on vector-burst-test, lane 3 padded once
        // legitimately for the load, the store then allocated uniformly four times, and the
        // stale credit fired a second pad that put lane 3 one id AHEAD of the others. From
        // there the single base id no longer described all four buffers -- beats landed in
        // the wrong slots, valid_q[read_pointer_q] never set, and the load never committed
        // (reorder buffers diverged 16 16 16 17, cyc 9141).
        //
        // Zeroing it on the store path also clears any residue the load left, so a credit
        // can never survive into an instruction that does not need it.
        pad_init[port] = mem_spatz_req.op_mem.is_load
                       ? (idx_width(ELENB+1))'(mem_max_elements[0] - mem_max_elements[port])
                       : '0;
        mem_remaining_bytes[port] = mem_max_elements[port] - mem_counter_q[port];
        mem_remaining_words[port] = mem_remaining_bytes[port] >> $clog2(MemDataWidthB);
        // A tile no larger than MaxBurstWords already imposes the burst-length cap.
        // Select directly between remaining work and tile space, avoiding two serial clamps.
        if (TileBurstWords <= MaxBurstWords) begin
          burst_len_calc[port] = (mem_remaining_words[port] >= burst_tile_words[port])
                                ? BurstLenWidth'(burst_tile_words[port])
                                : BurstLenWidth'(mem_remaining_words[port]);
        end else begin
          burst_len_calc[port] = (mem_remaining_words[port] >= MaxBurstWords)
                                ? BurstLenWidth'(MaxBurstWords)
                                : BurstLenWidth'(mem_remaining_words[port]);
          if (burst_tile_words[port] < burst_len_calc[port])
            burst_len_calc[port] = BurstLenWidth'(burst_tile_words[port]);
        end

        // No collapse-to-what-was-reserved. Every ROB reserves its own share of THIS
        // length up front and the request only leaves once all of them hold it
        // (burst_alloc_ready), so a partial reservation can never be issued.
        burst_len_eff[port] = burst_len_calc[port];

        // A burst request is expanded downstream into CONSECUTIVE addresses
        // (tcdm_burst_expander.sv: tgt_addr = base + beat). That is only what this port wants in
        // port-0 burst mode, where address generation is linear (offset = mem_counter_q, :562).
        burst_mode_req[port] = mem_is_load && !mem_is_single_element_operation &&
                               mem_use_port0_burst &&
                               (burst_len_eff[port] > 1) &&
                               (burst_len_eff[port] <= NrOutstandingLoads);

        // Keep burst mode active once allocation started.
        // When a new burst cannot be started in this cycle, the scalar path below
        // is allowed to run so request generation never deadlocks.
        burst_use[port] = burst_mode_req[port] &&
                          (burst_alloc_q[port] ||
                           (!rob_full[port] &&
                            !offset_queue_full[port]));

        mem_operation_valid[port] = mem_spatz_req_valid && (mem_max_elements[port] != mem_counter_q[port]);
        mem_operation_last[port]  = mem_operation_valid[port] &&
                                    ((mem_max_elements[port] - mem_counter_q[port]) <=
                                    (mem_is_single_element_operation ? mem_single_element_size :
                                     (burst_use[port] ? (burst_len_issue[port] * MemDataWidthB) : MemDataWidthB)));
        // Load request-side counters when a new instruction is enqueued; this
        // avoids carrying stale per-port offsets across instruction boundaries.
        mem_counter_load[port]    = commit_insn_push;
        begin
          if (mem_use_port0_burst)
            mem_counter_d[port] = mem_spatz_req.vstart;
          else begin
            mem_counter_d[port] = (mem_spatz_req.vstart >> $clog2(NrMemPorts*MemDataWidthB)) << $clog2(MemDataWidthB);
            if (NrMemPorts == 1)
              mem_counter_d[port] = mem_spatz_req.vstart;
            else
              if (mem_spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] > port)
                mem_counter_d[port] += MemDataWidthB;
              else if (mem_spatz_req.vstart[$clog2(MemDataWidthB) +: $clog2(NrMemPorts)] == port)
                mem_counter_d[port] += mem_spatz_req.vstart[$clog2(MemDataWidthB)-1:0];
          end
        end
        if (!mem_operation_valid[port]) begin
          mem_counter_delta[port] = 'd0;
        end else if (mem_is_single_element_operation) begin
          mem_counter_delta[port] = mem_single_element_size;
        end else if (burst_use[port]) begin
          mem_counter_delta[port] =
              mem_operation_last[port] ? (mem_max_elements[port] - mem_counter_q[port]) :
              (burst_len_issue[port] * MemDataWidthB);
        end else begin
          mem_counter_delta[port] =
              mem_operation_last[port] ? (mem_max_elements[port] - mem_counter_q[port]) : MemDataWidthB;
        end
        mem_counter_en[port]    = spatz_mem_req_ready[port] && spatz_mem_req_valid[port];
        mem_counter_max[port]   = mem_max_elements[port];

        // Index counter
        mem_idx_counter_d[port]     = mem_counter_d[port];
        mem_idx_counter_delta[port] = !mem_operation_valid[port] ? 'd0 : mem_idx_single_element_size;
      end
    end
  end

  ///////////
  // State //
  ///////////

  always_comb begin: p_state
    // Maintain state
    state_d = state_q;

    unique case (state_q)
      VLSU_RunningLoad: begin
        if (commit_insn_valid && !commit_insn_q.is_load)
          // Do not switch to store mode while any load response is still in flight.
          if (&rob_empty && !(|mem_pending_q))
            state_d = VLSU_RunningStore;
      end

      VLSU_RunningStore: begin
        if (commit_insn_valid && commit_insn_q.is_load)
          if (&rob_empty)
            state_d = VLSU_RunningLoad;
      end

      default:;
    endcase
  end: p_state

  //////////////////////////
  // Memory/VRF Interface //
  //////////////////////////

  // Memory request signals
  id_t  [NrMemPorts-1:0]                   mem_req_id;
  logic [NrMemPorts-1:0][MemDataWidth-1:0] mem_req_data;
  logic [NrMemPorts-1:0]                   mem_req_svalid;
  logic [NrMemPorts-1:0][ELEN/8-1:0]       mem_req_strb;
  logic [NrMemPorts-1:0]                   mem_req_lvalid;
  logic [NrMemPorts-1:0]                   mem_req_last;

  // Number of pending requests
  `FF(mem_pending_q, mem_pending_d, '{default: '0})

  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_burst_state
    `FF(burst_alloc_q[port],      burst_alloc_d[port],      1'b0)
    `FF(burst_len_q[port],        burst_len_d[port],        '0  )
    `FF(burst_alloc_cnt_q[port],  burst_alloc_cnt_d[port],  '0  )
    `FF(burst_base_id_q[port],    burst_base_id_d[port],    '0  )
  end : gen_burst_state

  // The burst's total length: one per core, independent of the block-reservation knob.
  `FF(burst_total_q, burst_total_d, '0)

  // Non-burst alignment padding. pad_step is how many bytes one request retires, so the
  // shortfall is counted down in the same units the lanes fell behind in: a
  // single-element operation (indexed, strided, unaligned, non-zero vstart) moves one
  // element per request, everything else a whole word.
  assign pad_step = mem_is_single_element_operation
                  ? (idx_width(ELENB+1))'(mem_single_element_size)
                  : (idx_width(ELENB+1))'(ELENB);
  always_comb begin
    pad_bytes_d = pad_bytes_q;
    for (int port = 0; port < NrMemPorts; port++) begin
      // Reload at the instruction boundary, where mem_max_elements is the new op's.
      if (mem_counter_load[port])
        pad_bytes_d[port] = pad_init[port];
      else if (pad_fire[port])
        pad_bytes_d[port] = (pad_bytes_q[port] > pad_step) ? (pad_bytes_q[port] - pad_step)
                                                           : '0;
    end
  end
  `FF(pad_bytes_q, pad_bytes_d, '{default: '0})

  // The block-reservation flop: ONE per core (ROB0 only), not instantiated at all when the
  // knob is off -- the tie-off keeps the net driven for lint and const-folds away.
  if (BlockWords > 1) begin : gen_burst_reserved
    `FF(burst_reserved_q, burst_reserved_d, 1'b0)
  end else begin : gen_no_burst_reserved
    assign burst_reserved_q = 1'b0;
  end

  // Odd-expected bitmap maintenance. SET by the burst id-allocation walk (beat cnt's id is odd
  // iff cnt is odd -- the id itself may be either parity); CLEARED by any consuming push of that
  // id on either port, which keeps it coherent when a burst is served under the legacy contract
  // (group-LOCAL target or MSHR bypass: every beat funnels to port 0). Set wins over a same-cycle
  // clear (an id can be freed and re-allocated in one cycle).
  if (BurstRecvPorts > 1) begin : gen_burst_odd_expected
    always_comb begin
      burst_odd_expected_d = burst_odd_expected_q;
      if (rob_push[0])
        burst_odd_expected_d[rob_wid[0]]  = 1'b0;
      if (rob_push2[0])
        burst_odd_expected_d[rob_wid2[0]] = 1'b0;
      if (mem_use_port0_burst && burst_alloc_fire[0])
        burst_odd_expected_d[rob_id[0]]   = burst_alloc_cnt_q[0][0];
      // Block reservation: all BlockWords parities are written in ONE cycle, the same cycle the
      // ROB takes the window. Deliberately NOT gated on mem_use_port0_burst: the ROB allocates
      // on burst_block_fire alone, and the bitmap must move with the ROB, never on a separate
      // condition (that is the F2 divergence shape).
      if ((BlockWords > 1) && burst_block_fire)
        burst_odd_expected_d = (burst_odd_expected_d & ~rob_block_mask[0]) |
                               ( rob_block_mask[0] & burst_odd_alt);
    end
  end else begin : gen_no_burst_odd_expected
    assign burst_odd_expected_d = '0;
  end
  always_comb begin
    // Maintain state
    mem_pending_d = mem_pending_q;

    // The pending counters are per in-flight memory instruction.
    // Reset on instruction enqueue to avoid stale carry-over -- but ONLY when nothing
    // older survives the cycle. H1: B's push must not zero A's in-flight beats; the UNION
    // count is load-bearing (B's early beats keep mem_pending[0]!=0 after A's charge
    // drains, blocking the stale-drain arms from popping B's parked data).
    if (commit_insn_push && (!Runahead || no_older))
      mem_pending_d = '{default: '0};

    for (int port = 0; port < NrMemPorts; port++) begin
      mem_pending[port] = mem_pending_q[port] != '0;

      // New load request accepted into the request spill stage.
      // Use the local load-valid path directly to avoid depending on the
      // packed request write bit during mode transitions.
      // A burst is ONE request on port 0 that returns beats to every ROB, so it owes
      // each port its share -- not the whole length to port 0. Charging port 0 alone
      // would leave ports 1..3 reading "owes nothing" and dropping their beats.
      if (mem_use_port0_burst) begin
        // Charge each lane ITS OWN share of the length this request actually carries.
        // Keying the charge on the MODE instead of on the request was a real defect: a
        // one-word request issued while in burst mode charged a full share to all four
        // lanes -- sixteen credits for one returning beat -- and mem_pending never
        // drained again, so the instruction never retired and the scalar core wedged
        // behind acc_mem_stall. burst_port_share(1, p) is 1 for lane 0 and 0 elsewhere,
        // which is exactly where a one-word remainder lands.
        if (spatz_mem_req_valid[0] && spatz_mem_req_ready[0] && mem_req_lvalid[0])
          mem_pending_d[port] = mem_pending_d[port] +
                                burst_port_share(spatz_mem_req[0].burst_len, port);
      end else if (spatz_mem_req_valid[port] && spatz_mem_req_ready[port] &&
                   mem_req_lvalid[port]) begin
        mem_pending_d[port] = mem_pending_d[port] + spatz_mem_req[port].burst_len;
      end

      // Response used
      // Decrement only for beats that were already pending before this cycle.
      // This avoids stale ROB cleanup pops canceling freshly-issued load beats.
      if (commit_insn_q.is_load && rob_rvalid[port] && rob_pop[port] &&
          (mem_pending_q[port] != '0))
        mem_pending_d[port]--;

      // TwinROB0 dual pop consumes two beats of the port-0 charge in one cycle.
      if ((BurstRecvPorts > 1) && (port == 0) &&
          commit_insn_q.is_load && rob_pop_dual[0] &&
          (mem_pending_q[0] >= 'd2))
        mem_pending_d[0] = mem_pending_d[0] - 'd2;
    end
  end

  // (switch_to_tail_phase and proc_burst_tail_phase removed -- a tail is a shorter burst now.)

  // Burst ID pre-allocation state (per port)
  // Every port has reserved its whole share: the burst request can go out.
  logic burst_alloc_ready;
  always_comb begin : proc_burst_alloc_ready
    burst_alloc_ready = 1'b1;
    for (int port = 0; port < NrMemPorts; port++)
      // A lane whose share is ZERO is legitimately ready. A burst shorter than
      // NrMemPorts words has no beat for the high lanes, so demanding a non-zero share
      // from every port (which a full-burst-only design could) deadlocks every tail of
      // 2..NrMemPorts-1 words: burst_alloc_ready never rises and the request never
      // leaves. burst_alloc_q already separates "allocated" from "idle".
      burst_alloc_ready &= burst_alloc_q[port] &&
                           (burst_alloc_cnt_q[port] == burst_len_q[port]);
    // ...but the burst must still carry something. burst_total_q is written in the same
    // cycle as the per-lane shares, so it is non-zero whenever burst_alloc_q is set.
    burst_alloc_ready &= (burst_total_q != '0);
  end : proc_burst_alloc_ready

  always_comb begin : proc_burst_alloc
    burst_alloc_d     = burst_alloc_q;
    burst_len_d       = burst_len_q;
    burst_alloc_cnt_d = burst_alloc_cnt_q;
    burst_base_id_d   = burst_base_id_q;
    burst_reserved_d  = burst_reserved_q;
    burst_total_d     = burst_total_q;
    burst_send        = '0;
    burst_alloc_fire  = '0;
    burst_len_issue   = burst_len_q;

    for (int port = 0; port < NrMemPorts; port++) begin
      logic [BurstLenWidth-1:0] burst_len_send;
      logic force_send;

      burst_len_send = burst_len_q[port];
      if (!exec_is_load) begin
        // Burst machinery is load-only. Clear any stale state before stores.
        burst_alloc_d[port]     = 1'b0;
        burst_len_d[port]       = '0;
        burst_alloc_cnt_d[port] = '0;
        burst_base_id_d[port]   = '0;
        burst_len_issue[port]   = '0;
        burst_send[port]        = 1'b0;
        burst_alloc_fire[port]  = 1'b0;
        if ((BlockWords > 1) && (port == 0)) burst_reserved_d = 1'b0;
      end else begin
      // No partial bursts. Collapsing a burst to however many ids it managed to
      // reserve gives the ROBs UNEQUAL shares, which desynchronises their allocators
      // and invalidates the single base id every beat is derived from. Waiting costs
      // nothing here: a share is MaxBurstWords/NrMemPorts entries, four times less
      // than the old whole-burst reservation in one ROB.
      force_send = 1'b0;
      // The RESERVATION count and the REQUEST length are different quantities. Each
      // ROB reserves its share (MaxBurstWords/NrMemPorts), but the single request on
      // port 0 still covers the WHOLE burst -- it is what tells the memory side how
      // many beats to return and what advances mem_counter by the full burst. Issuing
      burst_len_issue[port] = burst_alloc_q[port] ? burst_total_q : burst_len_calc[0];

      // Start a new burst allocation when eligible. The decision is port 0's -- it is
      // the only port that issues the request -- but EVERY port allocates, because
      // every ROB receives a share of the beats.
      // The new instruction must have loaded its request counters before reserving:
      // an older instruction's offset can otherwise select a different tile segment.
      if (!burst_alloc_q[port] && mem_operation_valid[0] && burst_use[0] && !dual_blk &&
          !commit_insn_push && mem_insn_pending_q[mem_spatz_req.id]) begin
        burst_alloc_d[port]     = 1'b1;
        // This lane's own share of the burst, not a fixed MaxBurstWords/NrMemPorts.
        // A tail gives the low lanes one beat more than the high ones, and a lane
        // whose share is zero is trivially ready.
        burst_len_d[port]       = burst_rows(burst_len_calc[0]);
        burst_alloc_cnt_d[port] = '0;
        burst_total_d           = burst_len_calc[0];
      end

      // Reserve the WHOLE burst's ROB ids in one cycle (). The condition is the registered
      // rob_req_block AND the ROB's own room_block_o -- i.e. exactly the ROB's internal
      // block_fire, so both sides move together. The legacy one-id-per-cycle walk is kept as
      // the `else` of THIS BLOCK CONDITION, not of a port test (correctness-reviewer correction
      // Take the granted window in ONE cycle. This arm is the `if` and the per-cycle walk
      // its `else`, so the two allocators can never both serve one burst -- and whenever the
      // block cannot fire (knob off, no room, or a tail) the burst still makes progress the
      // old way, so nothing can latch burst_alloc_q high forever.
      if ((BlockWords > 1) && burst_block_fire) begin
        burst_reserved_d        = 1'b1;
        burst_base_id_d[port]   = rob_id[port];
        burst_alloc_cnt_d[port] = burst_len_q[port];
      end else if (burst_alloc_q[port] && (burst_alloc_cnt_q[port] < burst_len_q[port])) begin
        // Burst pre-allocation only needs one free ROB slot per beat.
        // Using rob_id_valid here can deadlock at the last beat because
        // rob_id_valid requires two available IDs.
        if (!rob_full[port] && !offset_queue_full[port]) begin
          burst_alloc_fire[port] = 1'b1;
          if (burst_alloc_cnt_q[port] == '0)
            burst_base_id_d[port] = rob_id[port];
          burst_alloc_cnt_d[port] = burst_alloc_cnt_q[port] + 1'b1;
        end
      end

      if (force_send)
        burst_len_d[port] = burst_len_send;

      // One request covers every port's share, so it may only leave once EVERY ROB
      // holds its ids -- otherwise beats would arrive for a port with nowhere to put
      // them. burst_alloc_ready is the all-ports reduction, computed below.
      burst_send[port] = burst_alloc_ready;

      // Clear burst state once the burst request handshake completes.
      // Use the local load-valid intent (not the downstream valid_o path)
      // to keep valid generation independent from ready.
      // Clear EVERY port's burst state on the one request's handshake. The handshake
      // exists only on port 0 -- ports 1..3 issue nothing -- so gating each port on its
      // own left them latched as allocated: they never reserved for the next burst and
      // port 0 ran ahead, diverging the allocators the single base id depends on.
      if (burst_send[0] && mem_req_lvalid[0] && spatz_mem_req_ready[0]) begin
        burst_alloc_d[port]     = 1'b0;
        burst_len_d[port]       = '0;
        burst_alloc_cnt_d[port] = '0;
        burst_total_d           = '0;
        // Cannot collide with the set in the block branch: in the fire cycle cnt_q is still 0
        // while burst_len_issue is MaxBurstWords, so burst_send is low there.
        if ((BlockWords > 1) && (port == 0)) burst_reserved_d = 1'b0;
      end
      end
    end
  end : proc_burst_alloc

  // verilator lint_off LATCH
  always_comb begin
    vrf_raddr_o     = {vs2_vreg_addr, vd_vreg_addr};
    vrf_re_o        = '0;
    vrf_req_d       = '0;
    vrf_req_valid_d = 1'b0;

    rob_wdata = '0;
    rob_wid   = '0;
    rob_push  = '0;
    rob_pop   = '0;
    rob_req_id = '0;
    rob_req_dummy = '0;
    rob_alloc_site = '0;
    pad_fire      = '0;
    rob_wdata2   = '0;
    rob_wid2     = '0;
    rob_push2    = '0;
    rob_pop_dual = '0;

    mem_req_id     = '0;
    mem_req_data   = '0;
    mem_req_strb   = '0;
    mem_req_svalid = '0;
    mem_req_lvalid = '0;
    mem_req_last   = '0;

    // Propagate request ID
    vrf_req_d.rsp.id    = commit_insn_q.id;
    vrf_req_d.rsp_valid = commit_insn_valid && &commit_finished_d && mem_insn_finished_d[commit_insn_q.id];

    // Request indexes
    vrf_re_o[1] = mem_is_indexed;

    // Count which vs2 element we should load (indexed loads)
    vs2_elem_id_d = vs2_elem_id_q;
    if (&(pending_index ^ ~mem_operation_valid) && mem_is_indexed)
      vs2_elem_id_d = vs2_elem_id_q + 1;
    if (mem_spatz_req_ready)
      vs2_elem_id_d = '0;

    if ((state_q == VLSU_RunningLoad) && commit_insn_valid && commit_insn_q.is_load) begin
      // If we have a valid element in the buffer, store it back to the register file
      if (state_q == VLSU_RunningLoad && |commit_operation_valid) begin
        // Enable write back to the VRF if we have a valid element in all buffers that still have to write something back.
        vrf_req_d.waddr = vd_vreg_addr;

        begin
          // Gate on the commit-side element counters, NOT on mem_pending: mem_pending
          // counts *issued* requests, so a port whose last beats have not been issued yet
          // -- because the request side is stalled behind interconnect backpressure --
          // reads 0 and was treated as "owes no data". The writeback then fired early with
          vrf_req_valid_d = &(rob_rvalid | commit_finished_q) && !(&commit_finished_q) &&
                            |mem_pending;

          for (int unsigned port = 0; port < NrMemPorts; port++) begin
            automatic logic [63:0] data = rob_rdata[port];

          // Shift data to correct position if we have an unaligned memory request
          if (MAXEW == EW_32)
            unique case ((commit_insn_q.is_strided || commit_insn_q.is_indexed) ? vreg_addr_offset[port] : commit_insn_q.rs1[1:0])
              2'b01: data   = {data[7:0], data[31:8]};
              2'b10: data   = {data[15:0], data[31:16]};
              2'b11: data   = {data[23:0], data[31:24]};
              default: data = data;
            endcase
          else
            unique case ((commit_insn_q.is_strided || commit_insn_q.is_indexed) ? vreg_addr_offset[port] : commit_insn_q.rs1[2:0])
              3'b001: data  = {data[7:0], data[63:8]};
              3'b010: data  = {data[15:0], data[63:16]};
              3'b011: data  = {data[23:0], data[63:24]};
              3'b100: data  = {data[31:0], data[63:32]};
              3'b101: data  = {data[39:0], data[63:40]};
              3'b110: data  = {data[47:0], data[63:48]};
              3'b111: data  = {data[55:0], data[63:56]};
              default: data = data;
            endcase

            // Pop stored element and free space in buffer. Guarded on !rob_empty for
            // the same reason as the drain sites below: rob_rvalid is
            // valid_q[read_pointer_q], NOT !empty_o, so the stale-entry arm
            // (!mem_pending) can otherwise pop an empty buffer and wrap status_cnt_q.
            // Unreachable while a burst funnelled everything into ROB0 -- ports 1..3
            // never reached this code -- and reachable now that every port commits.
            rob_pop[port] = !rob_empty[port] && rob_rvalid[port] &&
                            ((!mem_pending[port]) ||
                             (vrf_req_valid_d && vrf_req_ready_d && commit_counter_en[port]));

          // Shift data to correct position if we have a strided memory access
          if (commit_insn_q.is_strided || commit_insn_q.is_indexed)
            if (MAXEW == EW_32)
              unique case (commit_counter_q[port][1:0])
                2'b01: data   = {data[23:0], data[31:24]};
                2'b10: data   = {data[15:0], data[31:16]};
                2'b11: data   = {data[7:0], data[31:8]};
                default: data = data;
              endcase
            else
              unique case (commit_counter_q[port][2:0])
                3'b001: data  = {data[55:0], data[63:56]};
                3'b010: data  = {data[47:0], data[63:48]};
                3'b011: data  = {data[39:0], data[63:40]};
                3'b100: data  = {data[31:0], data[63:32]};
                3'b101: data  = {data[23:0], data[63:24]};
                3'b110: data  = {data[15:0], data[63:16]};
                3'b111: data  = {data[7:0], data[63:8]};
                default: data = data;
              endcase
            vrf_req_d.wdata[ELEN*port +: ELEN] = data;

          // Create write byte enable mask for register file
            if (commit_counter_en[port])
              if (commit_is_single_element_operation) begin
                automatic logic [$clog2(ELENB)-1:0] shift = commit_counter_q[port][$clog2(ELENB)-1:0];
                automatic logic [ELENB-1:0] mask          = '1;
                case (commit_insn_q.vsew)
                  EW_8 : mask   = 1;
                  EW_16: mask   = 3;
                  EW_32: mask   = 15;
                  default: mask = '1;
                endcase
                vrf_req_d.wbe[ELENB*port +: ELENB] = mask << shift;
              end else
                for (int unsigned k = 0; k < ELENB; k++)
                  vrf_req_d.wbe[ELENB*port+k] = k < commit_counter_delta[port];
          end
        end
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        // Write the load result to the buffer
        rob_wdata[port] = spatz_mem_rsp_i[port].data;
        `ifdef TARGET_MEMPOOL
        rob_wid[port]   = spatz_mem_rsp_i[port].id;
        // Accept load responses while in load mode, or while older load beats are
        // still pending during a mode transition. This prevents dropping late beats.
        rob_push[port]  = spatz_mem_rsp_valid_i[port] &&
                          (spatz_mem_rsp_i[port].write == '0) &&
                          ((state_q == VLSU_RunningLoad) || (mem_pending_q[port] != '0));
        // TwinROB0 receive steering: a port-1 response whose id is an expected ODD burst beat
        // (parity drain: MSHR emits beat b on resp port 1+(b&1) with core_id+(b&1)) belongs to
        // ROB0's id range -> divert it to ROB0's second write port. All burst beats are charged
        // to mem_pending[0], so acceptance follows port 0's pending, not port 1's. Native
        // port-1 traffic (strided/indexed) is untouched: op-queue serialization guarantees no
        // native ROB1 load is outstanding while any odd-expected bit is set (asserted below).
        `else
        rob_push[port]  = spatz_mem_rsp_valid_i[port] &&
                          ((state_q == VLSU_RunningLoad) || (mem_pending_q[port] != '0)) &&
                          store_count_q[port] == '0;
        `endif
        // Burst id pre-allocation is NOT gated on this port issuing anything. A burst
        // is one request on port 0, but every ROB receives a share of its beats and so
        // must reserve ids -- and ports 1..3 have mem_operation_valid/burst_use masked
        // off in burst mode. Gating this on them left their allocators idle, so
        // burst_alloc_ready never asserted and the burst never issued.
        if (burst_alloc_fire[port]) begin
          rob_req_id[port] = 1'b1;
          rob_alloc_site[port] = 3'd1;
        end
        // The final id of a lane the last row does not reach carries no beat. Allocating it
        // as a dummy keeps every lane's count at burst_rows(), which is what makes the
        // single base id valid; it is drained below without reaching the register file.
        if (burst_alloc_fire[port] &&
            (burst_alloc_cnt_q[port] == (burst_len_q[port] - BurstLenWidth'(1))) &&
            (burst_port_share(burst_len_calc[0], port) < burst_len_q[port])) begin
          rob_req_dummy[port] = 1'b1;
          rob_alloc_site[port] = 3'd2;
        end

        if (mem_operation_valid[port]) begin
          if (burst_use[port]) begin
            if (burst_send[port]) begin
              mem_req_lvalid[port] = (!mem_is_indexed || (vrf_rvalid_i[1] && !pending_index[port])) &&
                                     !commit_insn_push &&
                                     commit_insn_q.is_load;
              mem_req_id[port]     = burst_base_id_q[port];
              mem_req_last[port]   = mem_operation_last[port];
            end
          end else if (burst_alloc_q[port] && burst_send[port]) begin
            // force_send collapsed a burst to a single pre-allocated beat
            // (burst_alloc_cnt_q==1 while rob_full): burst_use is false because
            // burst_len_eff==1, but the ROB id is already reserved, so issue it
            // directly -- crucially WITHOUT the !rob_full gate of the scalar
            mem_req_lvalid[port] = (!mem_is_indexed || (vrf_rvalid_i[1] && !pending_index[port])) &&
                                   !commit_insn_push &&
                                   commit_insn_q.is_load;
            mem_req_id[port]     = burst_base_id_q[port];
            mem_req_last[port]   = mem_operation_last[port];
          // !rob_req_block: a pending block reservation owns the ROB's id-request port this
          // cycle. Without this term, a burst whose burst_use collapsed while its window was
          // still pending would drive id_req_i and id_req_block_i together; the ROB gives the
          // block priority and the single-id allocation would be silently dropped, leaving
          // this arm using an id the ROB never handed out (A4). rob_req_block is registered
          // (~4 levels) and const 0 when the knob is off, so this term folds away.
          end else if ((!burst_mode_req[port] || !burst_use[port]) && !rob_req_block[port] &&
                       !rob_full[port] && !offset_queue_full[port]) begin
            // !dual_blk: an unsafe younger op at the head (store-after-load, strided,
            // tailed, vstart!=0) must emit NOTHING until the elder retires -- every
            // lvalid arm is gated on commit_insn_q.is_load (= the ELDER, a load), so
            // without this term it would issue LOAD requests at the younger op's
            // addresses.
            mem_req_lvalid[port] = (!mem_is_indexed || (vrf_rvalid_i[1] && !pending_index[port])) &&
                                   !commit_insn_push &&
                                   rob_id_valid[port] &&
                                   commit_insn_q.is_load &&
                                   !dual_blk;
            rob_req_id[port]     = spatz_mem_req_ready[port] & mem_req_lvalid[port];
            if (rob_req_id[port]) rob_alloc_site[port] = 3'd3;
            mem_req_id[port]     = rob_id[port];
            mem_req_last[port]   = mem_operation_last[port];
          end
        end else if (!mem_pending[port]) begin
          // During port0-only burst loads, non-active ports can still carry stale
          // ROB entries from the previous store phase. Drain them so they do not
          // reappear as spurious store requests on the next store instruction.
          // Guarded on !rob_empty exactly like the structurally identical store-side
          // drain below: rob_rvalid is valid_q[read_pointer_q], which is NOT !empty_o.
          // Popping a valid head while status_cnt_q == 0 wraps the ROB's 6-bit counter
          // to 63, after which full_o/empty_o never assert again and the port wedges.
          if (!rob_empty[port])
            rob_pop[port] = rob_rvalid[port];
        end
      end
    // Store operation
    end else begin
      // Read new element from the register file and store it to the buffer
      if (state_q == VLSU_RunningStore && !(|rob_full) && |commit_operation_valid) begin
        vrf_re_o[0] = 1'b1;

        for (int unsigned port = 0; port < NrMemPorts; port++) begin
          rob_wdata[port]  = vrf_rdata_i[0][ELEN*port +: ELEN];
          rob_wid[port]    = rob_id[port];
          rob_req_id[port] = vrf_rvalid_i[0] && (!mem_is_indexed || vrf_rvalid_i[1]) && rob_id_valid[port];
          if (rob_req_id[port]) rob_alloc_site[port] = 3'd4;
          rob_push[port]   = rob_req_id[port];
        end
      end

      for (int unsigned port = 0; port < NrMemPorts; port++) begin
        // Read element from buffer and execute memory request
        if (mem_operation_valid[port]) begin
          automatic logic [63:0] data = rob_rdata[port];

          // Shift data to lsb if we have a strided or indexed memory access
          if (mem_is_strided || mem_is_indexed)
            if (MAXEW == EW_32)
              unique case (mem_counter_q[port][1:0])
                2'b01: data = {data[7:0], data[31:8]};
                2'b10: data = {data[15:0], data[31:16]};
                2'b11: data = {data[23:0], data[31:24]};
                default:; // Do nothing
              endcase
            else
              unique case (mem_counter_q[port][2:0])
                3'b001: data = {data[7:0], data[63:8]};
                3'b010: data = {data[15:0], data[63:16]};
                3'b011: data = {data[23:0], data[63:24]};
                3'b100: data = {data[31:0], data[63:32]};
                3'b101: data = {data[39:0], data[63:40]};
                3'b110: data = {data[47:0], data[63:48]};
                3'b111: data = {data[55:0], data[63:56]};
                default:; // Do nothing
              endcase

          // Shift data to correct position if we have an unaligned memory request
          if (MAXEW == EW_32)
            unique case ((mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[port] : mem_spatz_req.rs1[1:0])
              2'b01: mem_req_data[port]   = {data[23:0], data[31:24]};
              2'b10: mem_req_data[port]   = {data[15:0], data[31:16]};
              2'b11: mem_req_data[port]   = {data[7:0], data[31:8]};
              default: mem_req_data[port] = data;
            endcase
          else
            unique case ((mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[port] : mem_spatz_req.rs1[2:0])
              3'b001: mem_req_data[port]  = {data[55:0], data[63:56]};
              3'b010: mem_req_data[port]  = {data[47:0], data[63:48]};
              3'b011: mem_req_data[port]  = {data[39:0], data[63:40]};
              3'b100: mem_req_data[port]  = {data[31:0], data[63:32]};
              3'b101: mem_req_data[port]  = {data[23:0], data[63:24]};
              3'b110: mem_req_data[port]  = {data[15:0], data[63:16]};
              3'b111: mem_req_data[port]  = {data[7:0], data[63:8]};
              default: mem_req_data[port] = data;
            endcase

          // !rob_dummy: a padding entry carries no store data; sending it would emit a
          // request with whatever the buffer slot happens to hold.
          mem_req_svalid[port] = rob_rvalid[port] && !rob_dummy[port] &&
                                 (!mem_is_indexed || (vrf_rvalid_i[1] && !pending_index[port])) &&
                                 !commit_insn_q.is_load;
          mem_req_id[port]     = rob_rid[port];
          mem_req_last[port]   = mem_operation_last[port];
          rob_pop[port]        = rob_rvalid[port] && spatz_mem_req_valid[port] && spatz_mem_req_ready[port];

          // Create byte enable signal for memory request
          if (mem_is_single_element_operation) begin
            automatic logic [$clog2(ELENB)-1:0] shift = (mem_is_strided || mem_is_indexed) ? mem_req_addr_offset[port] : mem_counter_q[port][$clog2(ELENB)-1:0] + commit_insn_q.rs1[int'(MAXEW)-1:0];
            automatic logic [MemDataWidthB-1:0] mask  = '1;
            case (mem_spatz_req.vtype.vsew)
              EW_8 : mask   = 1;
              EW_16: mask   = 3;
              EW_32: mask   = 15;
              default: mask = '1;
            endcase
            mem_req_strb[port] = mask << shift;
          end else
            for (int unsigned k = 0; k < ELENB; k++)
              mem_req_strb[port][k] = k < mem_counter_delta[port];
        end else begin
          // Clear empty buffer id requests
          if (!rob_empty[port])
            rob_pop[port] = rob_rvalid[port];
        end

        // A dummy carries no beat, so no commit will ever consume it. Drain it wherever it
        // surfaces -- including after this lane's commit has finished, when none of the arms
        // above run any more -- or it blocks the lane and rob_empty never asserts.
        if (rob_dummy[port])
          rob_pop[port] = 1'b1;
      end
    end

    // Non-burst paths hand the lanes uneven element counts (indexed, strided, and any
    // unit-stride op that is not burst-eligible -- stores included, which allocate an id
    // per request just as loads do). They therefore issue different numbers of requests
    // and the buffers drift apart, which invalidates the single base id a later burst
    // derives every beat from. Once a lane has issued everything it owes, pad it with
    // dummies until it has allocated as many ids as lane 0.
    for (int port = 0; port < NrMemPorts; port++)
      if (!mem_use_port0_burst && !mem_operation_valid[port] &&
          (pad_bytes_q[port] != '0) && !rob_full[port] && rob_id_valid[port]) begin
        pad_fire[port]       = 1'b1;
        rob_req_id[port]     = 1'b1;
        rob_req_dummy[port]  = 1'b1;
        rob_alloc_site[port] = 3'd5;
      end

    // A one-word remainder stays on the word path, and that request advances ONLY lane 0.
    // The other lanes must still take an id, or the buffers drift apart and the single base
    // id every burst beat is derived from stops being valid for the next burst. Give them a
    // dummy, exactly as a short burst row does.
    if (mem_use_port0_burst && mem_req_lvalid[0] && !burst_send[0] && spatz_mem_req_ready[0])
      for (int p = 1; p < NrMemPorts; p++) begin
        rob_req_id[p]     = 1'b1;
        rob_req_dummy[p]  = 1'b1;
        rob_alloc_site[p] = 3'd6;
      end

  end
  // verilator lint_on LATCH


  // Create memory requests
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_mem_req
    spill_register #(
      .T(spatz_mem_req_t)
    ) i_spatz_mem_req_register (
      .clk_i   (clk_i                      ),
      .rst_ni  (rst_ni                     ),
      .data_i  (spatz_mem_req[port]        ),
      .valid_i (spatz_mem_req_valid[port]  ),
      .ready_o (spatz_mem_req_ready[port]  ),
      .data_o  (spatz_mem_req_o[port]      ),
      .valid_o (spatz_mem_req_valid_o[port]),
      .ready_i (spatz_mem_req_ready_i[port])
    );
`ifdef TARGET_MEMPOOL
    // ID is required in Mempool-Spatz
    assign spatz_mem_req[port].id    = mem_req_id[port];
    assign spatz_mem_req[port].addr  = mem_req_addr[port];
    assign spatz_mem_req[port].mode  = '0; // Request always uses user privilege level
    assign spatz_mem_req[port].size  = mem_spatz_req.vtype.vsew[1:0];
    assign spatz_mem_req[port].write = mem_req_svalid[port];
    assign spatz_mem_req[port].burst_len = (mem_req_lvalid[port] && burst_send[port]) ? burst_len_issue[port] : BurstLenWidth'(1);
    assign spatz_mem_req[port].strb  = mem_req_strb[port];
    assign spatz_mem_req[port].data  = mem_req_data[port];
    assign spatz_mem_req[port].last  = mem_req_last[port];
    assign spatz_mem_req[port].spec  = 1'b0; // Request is never speculative
    assign spatz_mem_req_valid[port] = mem_req_svalid[port] || mem_req_lvalid[port];
`else
    assign spatz_mem_req[port].addr  = mem_req_addr[port];
    assign spatz_mem_req[port].write = mem_req_svalid[port];
    assign spatz_mem_req[port].amo   = reqrsp_pkg::AMONone;
    assign spatz_mem_req[port].data  = mem_req_data[port];
    assign spatz_mem_req[port].strb  = mem_req_strb[port];
    assign spatz_mem_req[port].burst_len = (mem_req_lvalid[port] && burst_send[port]) ? burst_len_issue[port] : BurstLenWidth'(1);
    assign spatz_mem_req[port].user  = '0;
    assign spatz_mem_req_valid[port] = mem_req_svalid[port] || mem_req_lvalid[port];
`endif
  end

  ////////////////
  // Assertions //
  ////////////////

  if (MemDataWidth != ELEN)
    $error("[spatz_vlsu] The memory data width needs to be equal to %d.", ELEN);

  if (NrMemPorts != N_FU)
    $error("[spatz_vlsu] The number of memory ports needs to be equal to the number of FUs.");

  if (NrMemPorts != 2**$clog2(NrMemPorts))
    $error("[spatz_vlsu] The NrMemPorts parameter needs to be a power of two");

  // TwinROB0 (2-wide burst receive) misconfig guards.
  if (NumRespPorts > NrMemPorts)
    $error("[spatz_vlsu] NumRespPorts (%0d) must be <= NrMemPorts (%0d).", NumRespPorts, NrMemPorts);
  // The block reservation grants a FIXED window of MaxBurstWords/NrMemPorts ids per lane, and
  // a full-length burst must consume it exactly. If MaxBurstWords is not a whole multiple of
  // NrMemPorts then burst_rows() rounds up and the grant is one id short of what the low lanes
  // need, so the walk and the block would both have to serve one burst.
  if ((BlockWords > 1) && ((MaxBurstWords % NrMemPorts) != 0))
    $error("[spatz_vlsu] SPATZ_VLSU_BLOCK_ALLOC requires MaxBurstWords (%0d) to be a whole multiple of NrMemPorts (%0d).",
           MaxBurstWords, NrMemPorts);

`ifndef SYNTHESIS
  for (genvar port = 1; port < NrMemPorts; port++) begin : gen_port0_burst_assert
    // In aligned unit-stride burst mode, only port 0 is allowed to issue requests.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        mem_use_port0_burst |-> !spatz_mem_req_valid[port])
      else $fatal(1, "[spatz_vlsu] Port %0d issued request during port0-only burst mode.", port);
  end

`ifdef TARGET_MEMPOOL
  // The converse, which the assertion above does NOT cover: a multi-beat request may only be
  // emitted while address generation is LINEAR. The burst expander turns burst_len into
  // consecutive addresses, but the multi-port path is word-interleaved (:564), so a burst issued
  // there silently fetches the wrong words. Tripwire for the burst_mode_req gate above.
  // (Scoped to TARGET_MEMPOOL: burst_len is only driven in that branch of gen_mem_req.)
  for (genvar port = 0; port < NrMemPorts; port++) begin : gen_burst_only_in_port0_mode
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        (spatz_mem_req_valid[port] && (spatz_mem_req[port].burst_len > BurstLenWidth'(1)))
          |-> mem_use_port0_burst)
      else $fatal(1, "[spatz_vlsu] Port %0d issued burst_len=%0d outside port0-burst mode (word-interleaved addressing -> wrong data).",
                  port, spatz_mem_req[port].burst_len);
  end
`endif

  // (gen_burst_ew_vl_ceiling removed. It warned that a load exceeding the burst ceiling
  // silently took the word path, but tested the OLD single-ROB ceiling
  // (NrOutstandingLoads*MemDataWidthB) rather than the whole-ROB-set one admission now
  // uses, so it fired for loads that were in fact bursting -- every cycle the request
  // was valid, with no edge detection. The BURSTWHY probe below reports the same thing
  // per conjunct, and is gated behind spatz_burst_debug.)

  // NON-BURST CAPACITY. Replaces gen_robn_nonburst_capacity, which tested the removed
  // SPATZ_VLSU_ROBN_DEPTH. The depths are uniform now, but the bound still exists and it still
  // has no gate above it: an op that is not burst-eligible (indexed, strided, unaligned, or the
  // whole traffic when SPATZ_VLSU_BURST=0) streams down the word-interleaved path, where each
  // lane needs vl / (MemDataWidthB * NrMemPorts) ids -- and NOTHING checks that against the ROB
  // depth. A STORE cannot escape onto the burst path at all (use_port0_burst_req demands
  // is_load), which is what made this audible in the first place: at ROB0=128/ROBN=16 a 512 B
  // store wedged the machine with every core LSU-stalled, inflight 0, and no message.
  //
  // At the shipped ROB32 the bound is 32 * 4 * 4 = 512 B == MAXVL, so nothing can exceed it and
  // this const-folds away. It is here for the next person who lowers spatz_vlsu_rob_depth.
  //
  // $warning, not $fatal: the exact wedge threshold between "throttles" and "deadlocks" has
  // never been measured, and firing fatally on a threshold I have not measured would be worse
  // than either. Rate-limited for the same reason as BurstWhyMax below -- the condition holds
  // for the whole life of an offending instruction, on every core, every cycle.
  if ((MAXVL / (MemDataWidthB * NrMemPorts)) > NrOutstandingLoads) begin : gen_nonburst_capacity
    // verilog_lint: waive-start
    // pragma translate_off
    localparam int unsigned CapWarnMax = 8;
    logic [$clog2(CapWarnMax+1)-1:0] cap_warn_n;
    always_ff @(posedge clk_i) begin
      if (!rst_ni) cap_warn_n <= '0;
      else if (mem_spatz_req_valid && !use_port0_burst_req &&
               (cap_warn_n < CapWarnMax) &&
               ((mem_spatz_req.vl / (MemDataWidthB * NrMemPorts)) > NrOutstandingLoads)) begin
        $warning("[spatz_vlsu] NON-BURST OVER CAPACITY: %0s vl=%0d B needs %0d ids per lane on the word-interleaved path but the reorder buffers are %0d deep. Raise SPATZ_VLSU_ROB_DEPTH; a STORE has no burst path to fall back on.",
                 mem_spatz_req.op_mem.is_load ? "load" : "store",
                 mem_spatz_req.vl,
                 mem_spatz_req.vl / (MemDataWidthB * NrMemPorts), NrOutstandingLoads);
        cap_warn_n <= cap_warn_n + 1;
      end
    end
    // pragma translate_on
    // verilog_lint: waive-stop
  end

  // BURSTWHY -- root-cause probe for "knob is on but every request is bl=1".
  // use_port0_burst_req is a 5-way AND; when it is 0 the load silently falls back to the
  // multi-port WORD-INTERLEAVED path, which turns one vector load into vl/4 single-word
  // requests. At e16,m2 that is 32 requests, so two loads fill a 64-entry ROB and the port
  // verilog_lint: waive-start
  // pragma translate_off
`ifndef TARGET_SYNTHESIS
  // Read the VALUE, not just `ifdef: the build defines SPATZ_BURST_DEBUG=0 to mean
  // off, and a bare `ifdef would be true for that. Same idiom as BurstEn above.
  localparam bit BurstWhyEn = `ifdef SPATZ_BURST_DEBUG `SPATZ_BURST_DEBUG `else 0 `endif;
  if (BurstWhyEn) begin : gen_burst_why
    localparam int unsigned BurstWhyMax = 24;
    // logic, not int; and NO declaration initialiser -- the reset branch below is the only
    // place these get their value. A declaration assignment is equivalent to an initial block
    // (IEEE 1800 10.5), which gives a variable already written by always_ff a SECOND procedural
    // driver -- illegal per 9.2.2.4 and rejected by VCS as Error-[ICPD_INIT]. It was also
    // redundant: the reset branch already writes the same values.
    logic [$clog2(BurstWhyMax+1)-1:0]   burst_why_n;
    logic [$bits(mem_spatz_req.id)-1:0] burst_why_last_id;
    logic                               burst_why_seen;
    always_ff @(posedge clk_i) begin
      if (!rst_ni) begin
        burst_why_n <= 0; burst_why_seen <= 1'b0;
      end else if (mem_spatz_req_valid && mem_spatz_req.op_mem.is_load &&
                   (!burst_why_seen || (mem_spatz_req.id != burst_why_last_id)) &&
                   (burst_why_n < BurstWhyMax)) begin
        burst_why_last_id <= mem_spatz_req.id;
        burst_why_seen    <= 1'b1;
        burst_why_n       <= burst_why_n + 1;
        $display("[BURSTWHY] t=%0t %m id=%0d vsew=%0d vl=%0dB rs1=0x%08x | strided=%0b indexed=%0b ew_ok=%0b vl_ge_%0d=%0b vl_le_%0d=%0b align%0d=%0b => burst=%0b",
                 $time, mem_spatz_req.id,
                 mem_spatz_req.vtype.vsew, mem_spatz_req.vl, mem_spatz_req.rs1,
                 mem_is_strided, mem_is_indexed,
                 1'b1,  // every element width is burst-eligible
                 (2*MemDataWidthB),               (mem_spatz_req.vl >= (2*MemDataWidthB)),
                 (NrOutstandingLoads*MemDataWidthB*NrMemPorts),
                 (mem_spatz_req.vl <= (NrOutstandingLoads*MemDataWidthB*NrMemPorts)),
                 $clog2(MemDataWidthB), (mem_spatz_req.rs1[$clog2(MemDataWidthB)-1:0] == '0),
                 use_port0_burst_req);
      end
    end
  end
`endif
  // pragma translate_on
  // verilog_lint: waive-stop

  if (BlockWords > 1) begin : gen_block_alloc_asserts
    // A5 (the F2 class): a block reservation may only be requested while port 0 is actually
    // running a burst. If the ROB reserved MaxBurstWords ids for a port that then never issues
    // the burst, the ROB believes them allocated while the VLSU never fills them: it never
    // advances, and the next MaxBurstWords responses push into slots it believes free.
    // Port 0 remains the authority on whether a burst is running -- it is the only port
    // that issues the request, and ports 1..N-1 have mem_operation_valid/burst_use masked
    // off in burst mode -- but the reservation is now requested on EVERY port, so the
    // antecedent has to cover all of them.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        (|rob_blk_req) |-> (mem_operation_valid[0] && burst_use[0]))
      else $fatal(1, "[spatz_vlsu] Block ROB reservation requested outside an active port-0 burst.");
    // A4, VLSU side (the ROB asserts the same on its own inputs): a block GRANT and a single id
    // request are mutually exclusive -- the ROB serves the block and drops the single silently,
    // and its requester then uses an id the ROB never handed out.
    //
    // Qualified by burst_block_fire (grant), NOT rob_req_block (request). 2026-08-29: the
    // unqualified form fired on a back-to-back 512 B burst load. rob_room_block goes low once
    // ROB0 passes NumWords-BlockWords (112 of 128), which a 512 B load -- 128 words in a
    // 128-entry ROB -- guarantees; the block is then REFUSED and the per-beat walk takes over,
    // which :2117 below calls the fallback by name. In that case block_fire is 0, so
    // reorder_buffer.sv:317 falls through `if (block_fire) ... else if (id_req_i && !full_o)`
    // and serves the single correctly. Nothing is dropped and no phantom id is used, so the
    // hazard this guards simply is not present. vector-burst-test never hit it because it issues
    // one m8 load then a store, letting the ROB drain between bursts.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        !(burst_block_fire && (|rob_req_id)))
      else $fatal(1, "[spatz_vlsu] Block GRANT and single ROB id request asserted together.");
    // The reservation and the fallback walk must never both advance one burst.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        !(burst_block_fire && (|burst_alloc_fire)))
      else $fatal(1, "[spatz_vlsu] Block reservation and the legacy id walk fired in the same cycle.");
  end

  // A-TRUNC: REMOVED 2026-08-28, superseded by the generation tag in reorder_buffer.
  //
  // It asserted `rob_wid[port] < RobNDepth` on ports 1-3, on the reasoning that a narrow ROB
  // derives a narrow id from NumWords, so an id at or above RobNDepth could only be a truncated
  // one. That reasoning died with IdWidthExt: an id now carries GenBits of GENERATION above the
  // entry index, so id 16 at RobNDepth 16 means generation 1 / entry 0 and is entirely legal.
  // The assertion fired on the first legal wrap and killed vector-burst-test at cycle 13,798 with
  //   "[spatz_vlsu] port 1 write id 0 >= RobNDepth 16: truncated."
  // -- a message whose own numbers contradict it, because the compare used the full id while the
  // report printed the entry part.
  //
  // Worth keeping the lesson rather than the code: A-TRUNC guarded the wrong half of the hazard
  // from the start. The harm was never ids being TRUNCATED (they round-tripped through the ROB's
  // own id_o); it was ids being REUSED while a duplicate response was still in flight, which this
  // assertion could not see. The entry index is now id[EntryAw-1:0] and is in range by
  // construction, and a stale generation is detected and counted inside reorder_buffer, which
  // reports it at end of simulation.

  if ((BlockWords > 1) && (BurstRecvPorts > 1)) begin : gen_block_odd_asserts
    // A6 (blk_odd_clean): the window a reservation is about to take must carry no LIVE
    // odd-expected bits. Otherwise a port-1 response for an OLD id is diverted into the new
    // window -- wrong data in vd -- while ROB1's real entry never arrives and stalls it.
    // room_block_o guarantees the window is unallocated; this checks the odd bitmap agrees.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        burst_block_fire |-> ((burst_odd_expected_q & rob_block_mask[0]) == '0))
      else $fatal(1, "[spatz_vlsu] Block reservation window overlaps live odd-expected beats.");
  end

`ifdef TARGET_MEMPOOL
`ifndef VERILATOR
  // [VPERF] kernel-window VLSU performance counters (permanent sim-only instrumentation; the
  // matmul bottleneck study's anchor -- keep as a regression guard for receive-path changes).
  // Gated by the benchmark trace CSR window (same gate as the TB profilers); one summary line
  // per core at window close. Cycle-accounting identity (verified exactly): win = pair_commit +
  // wait_beats + no_insn + store/residual + vrf_bp. wait_beats is VLSU OCCUPANCY, not
  // critical-path exposure (it overlaps VFU compute) -- see docs/matmul_bottleneck_report.md §5.
  if (1) begin : gen_vperf
    logic        vperf_win_q;
    logic [31:0] c_win, c_insn, c_noinsn, c_pairok, c_single, c_waitbeat, c_vrfbp, c_reqstall, c_ret;
    logic [31:0] c_dual, c_blkstall;
    // Request B (TeraNoC_gvsoc/ TIME AVERAGE of inflight_q, which the
    // existing counters cannot give. TWO denominators are emitted deliberately, because the model
    // and the RTL could otherwise average over different cycle sets -- the failure mode that
    // request's own section 4 warns about:
    logic [47:0] c_arr_words;
    logic [31:0] c_arr_h1, c_arr_h2p, c_arr_cyc;
    // LOAD-ONLY arrival width. The unfiltered counters above count $countones(spatz_mem_rsp_valid_i),
    // and STORE ACKS assert that same valid (see the store_count_d ack test, which itself qualifies
    // on spatz_mem_rsp_i[port].write). The ROB is write-filtered (rob_push excludes .write), so the
    // unfiltered arrival total counts loads AND store acks while commit counts only loads -- which is
    logic [NrMemPorts-1:0] rsp_load_valid;
    logic [47:0] c_ld_words;
    logic [31:0] c_ld_h1, c_ld_h2p, c_ld_cyc;
    always_comb begin
      for (int unsigned pp = 0; pp < NrMemPorts; pp++)
        rsp_load_valid[pp] = spatz_mem_rsp_valid_i[pp] && !spatz_mem_rsp_i[pp].write;
    end
    logic [47:0] c_inflsum;
    logic [31:0] c_actcyc;
    wire vperf_win = mempool_tb.csr_trace_any_global;
    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) begin
        vperf_win_q <= 1'b0;
        {c_win, c_insn, c_noinsn, c_pairok, c_single, c_waitbeat, c_vrfbp, c_reqstall, c_ret} <= '0;
        {c_dual, c_blkstall} <= '0;
        {c_inflsum, c_actcyc} <= '0;
        {c_arr_words, c_arr_h1, c_arr_h2p, c_arr_cyc} <= '0;
        {c_ld_words, c_ld_h1, c_ld_h2p, c_ld_cyc} <= '0;
      end else begin
        vperf_win_q <= vperf_win;
        if (vperf_win && !vperf_win_q) begin
          {c_win, c_insn, c_noinsn, c_pairok, c_single, c_waitbeat, c_vrfbp, c_reqstall, c_ret} <= '0;
        {c_dual, c_blkstall} <= '0;
        {c_inflsum, c_actcyc} <= '0;
        {c_arr_words, c_arr_h1, c_arr_h2p, c_arr_cyc} <= '0;
        {c_ld_words, c_ld_h1, c_ld_h2p, c_ld_cyc} <= '0;
        end else if (vperf_win) begin
          c_win <= c_win + 1;
          // arrival width this cycle, counted at the core's response ports
          c_arr_words <= c_arr_words + 48'($countones(spatz_mem_rsp_valid_i));
          if ($countones(spatz_mem_rsp_valid_i) != 0) c_arr_cyc <= c_arr_cyc + 1;
          if ($countones(spatz_mem_rsp_valid_i) == 1) c_arr_h1  <= c_arr_h1  + 1;
          if ($countones(spatz_mem_rsp_valid_i) >  1) c_arr_h2p <= c_arr_h2p + 1;
          // same histogram, loads only
          c_ld_words <= c_ld_words + 48'($countones(rsp_load_valid));
          if ($countones(rsp_load_valid) != 0) c_ld_cyc <= c_ld_cyc + 1;
          if ($countones(rsp_load_valid) == 1) c_ld_h1  <= c_ld_h1  + 1;
          if ($countones(rsp_load_valid) >  1) c_ld_h2p <= c_ld_h2p + 1;
          if (commit_insn_valid && commit_insn_q.is_load) c_insn <= c_insn + 1;
          if (!commit_insn_valid)                         c_noinsn <= c_noinsn + 1;
          if (commit_pair_active && vrf_req_valid_d && vrf_req_ready_d) c_pairok <= c_pairok + 1;
          if (commit_use_port0_burst && !commit_pair_active &&
              vrf_req_valid_d && vrf_req_ready_d)         c_single <= c_single + 1;
          if (commit_pair_active && mem_pending[0] &&
              !(rob_rvalid[0] && rob_rvalid2[0]))         c_waitbeat <= c_waitbeat + 1;
          if (vrf_req_valid_d && !vrf_req_ready_d)        c_vrfbp <= c_vrfbp + 1;
          if (mem_req_lvalid[0] && !spatz_mem_req_ready[0]) c_reqstall <= c_reqstall + 1;
          if (commit_insn_pop)                            c_ret <= c_ret + 1;
          if (dual_adv)                                   c_dual <= c_dual + 1;
          if (burst_alloc_q[0] && !burst_block_fire &&
              mem_operation_valid[0])                     c_blkstall <= c_blkstall + 1;
          // inflight_q exists only when Runahead (MaxInflight>1); fall back to the ongoing-insn
          // indicator so the line stays meaningful at dual_load=1.
          c_inflsum <= c_inflsum + (Runahead ? 48'(inflight_q) : 48'(commit_insn_valid ? 1 : 0));
          if (Runahead ? (inflight_q != '0) : commit_insn_valid) c_actcyc <= c_actcyc + 1;
        end
        // begin/end is REQUIRED here: with two $display statements a bare `if` guards only the
        // first, and the second fires every cycle. That produced 3.8M lines / 1.0 GB before the
        // window even closed, and indentation made it look guarded.
        if (!vperf_win && vperf_win_q && (c_ret != 0)) begin
          $display("[VPERF] %m win=%0d insn_act=%0d no_insn=%0d pair_commit=%0d single_commit=%0d wait_beats=%0d vrf_bp=%0d req_stall=%0d insn_ret=%0d dual_adv=%0d blk_stall=%0d infl_sum=%0d act_cyc=%0d",
                   c_win, c_insn, c_noinsn, c_pairok, c_single, c_waitbeat, c_vrfbp, c_reqstall, c_ret, c_dual, c_blkstall, c_inflsum, c_actcyc);
          $display("[VARRIVE] %m win=%0d arr_words=%0d arr_cyc=%0d arr_h1=%0d arr_h2p=%0d ports=%0d",
                   c_win, c_arr_words, c_arr_cyc, c_arr_h1, c_arr_h2p, NrMemPorts);
          $display("[VARRIVE-LD] %m win=%0d ld_words=%0d ld_cyc=%0d ld_h1=%0d ld_h2p=%0d ports=%0d",
                   c_win, c_ld_words, c_ld_cyc, c_ld_h1, c_ld_h2p, NrMemPorts);
        end
      end
    end
  end
`endif
`endif

  if (BurstRecvPorts > 1) begin : gen_twinrob0_asserts
    // The odd-expected classifier is sound because the mem op-queue serializes instructions
    // until full retire: no native port-1 (strided/indexed) ROB1 allocation may be outstanding
    // while odd burst beats are expected. A future Spatz change pipelining a second mem
    // instruction into the FSM re-opens id aliasing -- this assertion is the tripwire.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        (|burst_odd_expected_q) |-> !rob_req_id[1])
      else $fatal(1, "[spatz_vlsu] Native ROB1 allocation while odd burst beats are expected.");
    if (Runahead) begin : gen_runahead_asserts
    // A1: never advance the op queue mid-allocation (the walk-fallback window).
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        (dual_adv && mem_spatz_req_ready) |-> !(|burst_alloc_q))
      else $fatal(1, "[spatz_vlsu] H1 advance during burst allocation.");
    // A3: while two instructions are co-resident, no native multi-port ROB traffic
    // (only ROB0 may allocate; a native ROB1 alloc aliases the parity steering).
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        dual_run |-> !(|rob_req_id[NrMemPorts-1:1]))
      else $fatal(1, "[spatz_vlsu] Native multi-port ROB allocation during dual residency.");
    // A5: the in-flight cap and the bookkeeping counter agree with the commit FIFO.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        (inflight_q <= InflWidth'(MaxInflight)) &&
        // usage_o is the LOW idx_width(CommitQDepth) bits of an (idx_width+1)-bit count, so a
        // FULL queue reads back as 0. Comparing against it directly was wrong at that boundary
        // as well as width-mismatched; check the full case explicitly instead.
        (commit_insn_full ? (inflight_q == InflWidth'(CommitQDepth))
                          : (idx_width(CommitQDepth)'(inflight_q) == commit_usage)))
      else $fatal(1, "[spatz_vlsu] H1 inflight bookkeeping diverges from the commit FIFO.");
    // A6: the union pending count is bounded by the ROB allocation.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        mem_pending_q[0] <= NrOutstandingLoads)
      else $fatal(1, "[spatz_vlsu] mem_pending[0] exceeds the ROB id space.");
    // A7: a 2-wide commit pop always has both beats charged.
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        (commit_insn_q.is_load && rob_pop_dual[0]) |-> (mem_pending_q[0] >= 2))
      else $fatal(1, "[spatz_vlsu] Dual ROB pop with fewer than two pending beats.");

    // A9 (fence underflow -- arm FIRST: a lost pulse hangs snitch acc_mem_req_cnt and
    // wedges the next gbar_sync). Exactly one spatz_mem_req_sent_o pulse per
    // push->pop interval of every instruction id.
    logic [NrParallelInstructions-1:0] sent_seen_q, sent_seen_d;
    always_comb begin
      sent_seen_d = sent_seen_q;
      if (commit_insn_push)      sent_seen_d[mem_spatz_req.id] = 1'b0;
      if (spatz_mem_req_sent_o)  sent_seen_d[mem_spatz_req.id] = 1'b1;
    end
    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) sent_seen_q <= '0;
      else         sent_seen_q <= sent_seen_d;
    end
    a9_no_double_pulse: assert property (@(posedge clk_i) disable iff (!rst_ni)
        spatz_mem_req_sent_o |-> !sent_seen_q[mem_spatz_req.id])
      else $fatal(1, "[spatz_vlsu] H1 fence: two req_sent pulses for one instruction.");
    a9_pulse_per_insn: assert property (@(posedge clk_i) disable iff (!rst_ni)
        (commit_insn_pop && commit_insn_q.is_load) |-> sent_seen_q[commit_insn_q.id])
      else $fatal(1, "[spatz_vlsu] H1 fence: instruction retired without a req_sent pulse.");

    // A10 (positional commit attribution -- the no-id-tagging invariant): the ROB0 pops
    // attributed to the commit head must equal its word count at retire. Count COMMIT pops
    // ONLY: the stale-drain arm (rob_pop[0] with !mem_pending[0]) also pops ROB0 to discard
    // leftover entries from an earlier phase (e.g. warmup leftovers at the start of a load
    // phase) -- those are not commits and must not inflate the count. In the TwinROB0 pair
    // branch rob_pop_dual is always a commit pop; in the legacy branch a commit pop has
    // mem_pending[0] set.
    logic [15:0] head_pops_q, head_pops_d;
    always_comb begin
      head_pops_d = head_pops_q + (rob_pop_dual[0] ? 16'd2 :
                                   ((rob_pop[0] && mem_pending[0]) ? 16'd1 : 16'd0));
      if (commit_insn_pop) head_pops_d = '0;
    end
    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (!rst_ni) head_pops_q <= '0;
      else         head_pops_q <= head_pops_d;
    end
    a10_positional: assert property (@(posedge clk_i) disable iff (!rst_ni)
        (commit_insn_pop && commit_insn_q.is_load && commit_insn_q.use_port0_burst)
        |-> (head_pops_q + (rob_pop_dual[0] ? 16'd2 :
                            ((rob_pop[0] && mem_pending[0]) ? 16'd1 : 16'd0)))
            == (16'(commit_insn_q.vl) >> 2))
      else $fatal(1, "[spatz_vlsu] H1 positional attribution broken: pops=%0d+%0d != vl_words=%0d at retire.",
                  head_pops_q,
                  (rob_pop_dual[0] ? 16'd2 : ((rob_pop[0] && mem_pending[0]) ? 16'd1 : 16'd0)),
                  (16'(commit_insn_q.vl) >> 2));
  end

  if (!Runahead) begin : gen_retire_odd_assert
      // Every expected odd beat must have been consumed by the time its instruction retires.
      assert property (@(posedge clk_i) disable iff (!rst_ni)
          (commit_insn_pop && commit_insn_q.use_port0_burst) |-> !(|burst_odd_expected_d))
        else $fatal(1, "[spatz_vlsu] Burst instruction retiring with odd-expected bits still set.");
    end else begin : gen_walk_odd_assert
      // Under dual residency the form above FALSE-POSITIVES (A retires while B's odd bits
      // legitimately live). Id-scoped sound form: an allocation may never target a live
      // odd-expected id (blk_odd_clean covers the block path; this is the walk mirror).
      // Disjoint id windows make aliasing structurally impossible; the native-ROB1
      // tripwire above stays armed as the F5 backstop.
      assert property (@(posedge clk_i) disable iff (!rst_ni)
          burst_alloc_fire[0] |-> !burst_odd_expected_q[rob_id[0]])
        else $fatal(1, "[spatz_vlsu] Walk allocation into a live odd-expected id.");
    end
  end
`endif


`ifndef SYNTHESIS
  // The single base id every burst beat is derived from is only valid while all the reorder
  // buffers agree on their next free id. Equal allocation is what maintains that: a burst
  // takes ceil(len/NrMemPorts) ids in EVERY lane, the lanes a short row does not reach
  // taking a dummy, and a one-word word-path remainder gives the other lanes a dummy too.
  // The non-burst paths (indexed, strided, stores) do NOT yet pad, so this is the tripwire
  // for that: it fails loudly instead of steering beats into the wrong buffer.
`ifndef TARGET_SYNTHESIS
`ifdef SPATZ_ROB_ALLOC_TRACE
  // pragma translate_off
  always_ff @(posedge clk_i) begin
    if (rst_ni && (|rob_req_id))
      $display("[ROBALLOC] %m t=%0t site=%0d,%0d,%0d,%0d req=%04b dmy=%04b id=%0d,%0d,%0d,%0d full=%04b empty=%04b idvld=%04b burst=%0b",
               $time, rob_alloc_site[0], rob_alloc_site[1], rob_alloc_site[2], rob_alloc_site[3],
               rob_req_id, rob_req_dummy,
               rob_id[0], rob_id[1], rob_id[2], rob_id[3],
               rob_full, rob_empty, rob_id_valid, mem_use_port0_burst);
  end
  // pragma translate_on
`endif
`endif

`ifndef TARGET_SYNTHESIS
  assert property (@(posedge clk_i) disable iff (!rst_ni)
      (mem_use_port0_burst && spatz_mem_req_valid[0] && spatz_mem_req_ready[0]) |->
      (!commit_insn_push &&
       (spatz_mem_req[0].burst_len <= mem_remaining_words[0])))
    else $fatal(1, "[spatz_vlsu] Burst exceeds the initialized instruction extent.");
`endif

  // ANTECEDENT COVERS BOTH ALLOCATORS. It used to be (|burst_alloc_fire) alone -- the
  // per-cycle walk -- so with SPATZ_VLSU_BLOCK_ALLOC=1, which is the shipped default, the
  // whole guard was silent: the block path allocates through burst_block_fire and never
  // raises burst_alloc_fire. The invariant it protects is what makes the single base id on
  // the request valid for all four buffers, so it has to watch the path we actually use.
  assert property (@(posedge clk_i) disable iff (!rst_ni)
      (|burst_alloc_fire || burst_block_fire) |-> ((rob_id[0] == rob_id[1]) &&
                               (rob_id[1] == rob_id[2]) &&
                               (rob_id[2] == rob_id[3])) ||
                              (burst_alloc_cnt_q[0] != '0))
    else $fatal(1, "[spatz_vlsu] reorder buffers diverged (%0d %0d %0d %0d): the single burst base id is invalid.",
                rob_id[0], rob_id[1], rob_id[2], rob_id[3]);

  // NOTE (superseded): the buffers were briefly allowed to diverge, each lane carrying its
  // own base. Every lane
  // publishes its own base id to the burst adapter (mem_req_id[1..N-1] above), so beats
  // are derived per lane. That is what lets a tail reserve ragged shares, and it also
  // means an earlier op that distributed an uneven element count across the ports --
  // indexed and strided both do -- can no longer misplace a later burst's beats.
`endif


  // TEMPORARY wedge watchdog (SPATZ_VLSU_WEDGE): dump VLSU completion state when an
  // in-flight instruction makes no progress for a long time.
`ifndef SYNTHESIS
`ifdef SPATZ_VLSU_WEDGE
  int unsigned wedge_idle_cnt;
  logic        wedge_fired;
  // Where do the beats go? Count what the port SEES vs what the ROB accepts.
  int unsigned c_rsp   [NrMemPorts];
  int unsigned c_push  [NrMemPorts];
  int unsigned c_wrack [NrMemPorts];
  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      for (int p = 0; p < NrMemPorts; p++) begin
        c_rsp[p] <= 0; c_push[p] <= 0; c_wrack[p] <= 0;
      end
    end else begin
      for (int p = 0; p < NrMemPorts; p++) begin
        if (spatz_mem_rsp_valid_i[p]) c_rsp[p]  <= c_rsp[p] + 1;
        if (rob_push[p])              c_push[p] <= c_push[p] + 1;
        if (spatz_mem_rsp_valid_i[p] && spatz_mem_rsp_i[p].write)
                                      c_wrack[p] <= c_wrack[p] + 1;
      end
    end
  end
  logic        wedge_activity;
  always_comb begin
    wedge_activity = 1'b0;
    for (int p = 0; p < NrMemPorts; p++)
      if ((spatz_mem_req_valid[p] && spatz_mem_req_ready[p]) || spatz_mem_rsp_valid_i[p])
        wedge_activity = 1'b1;
  end
  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      wedge_idle_cnt <= 0;
      wedge_fired    <= 1'b0;
    end else begin
      if (wedge_activity || !commit_insn_valid) wedge_idle_cnt <= 0;
      else                                      wedge_idle_cnt <= wedge_idle_cnt + 1;
      if ((wedge_idle_cnt > 20000) && !wedge_fired) begin
        wedge_fired <= 1'b1;
        $display("[VLSU-WEDGE] %m t=%0t state=%0d is_load=%0b p0burst=%0b vl=%0dB total=%0d IdW=%0d",
                 $time, state_q, commit_insn_q.is_load, mem_use_port0_burst,
                 mem_spatz_req.vl, burst_total_q, IdWidth);
        for (int p = 0; p < NrMemPorts; p++)
          $display("[VLSU-WEDGE]   port%0d rsp=%0d push=%0d wrack=%0d | pend=%0d robE=%0b robF=%0b cnt=%0d base=%0d nextid=%0d rvalid=%0b",
                   p, c_rsp[p], c_push[p], c_wrack[p],
                   mem_pending_q[p], rob_empty[p], rob_full[p], mem_counter_q[p],
                   burst_base_id_q[p], rob_id[p], rob_rvalid[p]);
        for (int p = 0; p < NrMemPorts; p++)
          $display("[VLSU-WEDGE]   port%0d head_id=%0d", p, rob_rid[p]);
        for (int f = 0; f < N_FU; f++)
          $display("[VLSU-WEDGE]   fu%0d commit_cnt=%0d", f, commit_counter_q[f]);
      end
    end
  end
`endif
`endif

endmodule : spatz_vlsu
