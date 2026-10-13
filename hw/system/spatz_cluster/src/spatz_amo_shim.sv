// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Florian Zaruba <zarubaf@iis.ee.ethz.ch>
`include "common_cells/registers.svh"
`include "common_cells/assertions.svh"

/// Shim in front of SRAMs which translates atomic (and normal)
/// memory operations to RMW sequences. The requests are atomic except
/// for the DMA which can request priority. The current model is
/// that the DMA will never write to the same memory location.
/// We provide `amo_conflict_o` to detect such event and
/// indicate a fault to the programmer.

/// LR/SC reservations are happening on `DataWidth` granularity.
module spatz_amo_shim
  import snitch_pkg::*;
  import reqrsp_pkg::*;
#(
  /// Address width.
  parameter int unsigned AddrMemWidth = 32,
  /// Word width (AMO ALU data width; 32 or 64 bit).
  parameter int unsigned DataWidth    = 64,
  /// Codeword width for ECC-protected TCDM (≥ DataWidth).
  /// When ProtDataWidth > DataWidth the shim encodes AMO/SC results internally
  /// and passes non-AMO codewords through unchanged. Set equal to DataWidth to
  /// disable ECC mode (original behaviour).
  parameter int unsigned ProtDataWidth = DataWidth,
  /// Core ID type.
  parameter int unsigned CoreIDWidth  = 1,
  /// Do not override. Derived parameter.
  parameter int unsigned StrbWidth    = DataWidth/8,
  /// Do not override. Derived parameter. Codeword width for mem_be_ecc_o
  localparam int unsigned StrbEccWidth = StrbWidth + hsiao_ecc_pkg::min_ecc(StrbWidth)
) (
  input   logic                     clk_i,
  input   logic                     rst_ni,
  // Request Side
  /// End-to-end handshake TMR: three independently-computed replicas of
  /// "is this request valid", carried all the way from spatz_cluster.sv's
  /// gen_tcdm_signals arbiter through i_tcdm_interconnect and
  /// mem_wide_narrow_mux without being voted down at any point in between
  input   logic [2:0]               valid_rep_i,
  /// Request is ready.
  output  logic                     ready_o,
  /// Request is DMA transfer.
  /// Bypass AMO, stall current AMO operation
  input   logic                     dma_access_i,
  /// Request address. Word aligned (not byte aligned!)
  input   logic [AddrMemWidth-1:0]  addr_i,
  /// Request AMO type. Must AMONone if `dma_access_i` is set.
  input   amo_op_e      amo_i,
  /// Request is a write. Must be `0` for AMOs.
  input   logic                     write_i,
  /// Data to write, second operand for AMOs.
  /// Must be a valid ProtDataWidth-wide ECC codeword when ProtDataWidth > DataWidth.
  input   logic [ProtDataWidth-1:0] wdata_i,
  /// Write byte mask for AMOs. For AMOs the byte mask must
  /// be all `1` for the 32 bits which are read by the AMO.
  input   logic [StrbWidth-1:0]     wstrb_i,
  /// Read data, first operand for AMOs.
  output  logic [ProtDataWidth-1:0] rdata_o,
  /// Core making the request. Only valid if not a DMA transfer.
  /// This is needed for determining if the reservation should be
  /// killed or not.
  input   logic [CoreIDWidth-1:0]   core_id_i,
  /// The request is made by a core. Only valid if not a DMA transfer.
  /// Another source of transfers can be coming from the AXI bus.
  input   logic                     is_core_i,
  // SRAM interface. Data always comes a cycle later.
  /// Bank request.
  output  logic                     mem_req_o,
  /// Address.
  output  logic [AddrMemWidth-1:0]  mem_add_o,
  /// 1: Store, 0: Load
  output  logic                     mem_wen_o,
  /// Write data (ProtDataWidth-wide codeword to SRAM).
  output  logic [ProtDataWidth-1:0] mem_wdata_o,
  /// Byte enable.
  output  logic [StrbWidth-1:0]     mem_be_o,
  /// mem_be_o, ECC-protected
  output  logic [StrbEccWidth-1:0]  mem_be_ecc_o,
  /// Read data (ProtDataWidth-wide codeword from SRAM).
  input   logic [ProtDataWidth-1:0] mem_rdata_i,
  /// Status signal, AMO clashed with DMA transfer.
  output  logic                     amo_conflict_o,
  // CMY: input grant signal from ecc_sram
  input   logic                     ecc_sram_gnt_i,
  /// Set when the AMO access FSM lands on an illegal (non-one-hot) code --
  /// detection only, single-bit-flip fault model.
  output  logic                     fsm_illegal_state_o,
  /// Set when the triplicated FSM/priority-arbiter handshake decision disagrees across its three replicas
  output  logic                     handshake_tmr_fault_o
);

  logic idx_q, idx_d;
  logic [31:0] operand_a, operand_b_q, amo_result, amo_result_q;
  logic [AddrMemWidth-1:0] addr_q;
  amo_op_e amo_op_q;
  logic load_amo;
  logic sc_successful, sc_successful_q;
  logic sc_q;

  // Declared here (before first use in assign rdata_o); driven by gen_prot_enc below.
  localparam int unsigned ProtWidth = ProtDataWidth - DataWidth;
  logic [ProtDataWidth-1:0] amo_result_cw;
  logic [ProtDataWidth-1:0] sc_result_cw;
  // DMA writes arrive without valid ECC parity (plain 32-bit data in wdata_i[31:0],
  // garbage in wdata_i[38:32]).  Encode here so SRAM always receives a valid codeword.
  logic [ProtDataWidth-1:0] dma_wdata_enc;

  // One-hot encoded: under a single-bit-flip fault model, any SEU on state_q
  // flips its parity (odd weight-1 -> even), so `^state_q` alone catches it.
  typedef enum logic [2:0] {
    Idle        = 3'b001,
    DoAMO       = 3'b010,
    WriteBackAMO = 3'b100
  } state_e;
  // TMR-protected: this is the only flop that gates the multi-cycle
  // ready_o/mem_req_o/mem_wen_o/core_arb_ready handshake decision (Idle/
  // DoAMO/WriteBackAMO)
  state_e state_d;
  state_e state_q_rep [3];
  for (genvar t = 0; t < 3; t++) begin : gen_state_rep
    `FF(state_q_rep[t], state_d, Idle)
  end
  state_e state_q;
  // Vote the triplicated state register down to the single `state_q` every
  // combinational consumer below (including the FSM's own next-state logic
  // in gen_ctrl_rep) reads -- without this, state_q is never driven and the
  // FSM can never reach DoAMO/WriteBackAMO.
  logic   state_q_tmr_fault;
  bitwise_TMR_voter_fail #(
    .DataWidth ($bits(state_e)),
    .VoterType (1)
  ) i_state_q_voter (
    .a_i              (state_q_rep[0]   ),
    .b_i              (state_q_rep[1]   ),
    .c_i              (state_q_rep[2]   ),
    .majority_o       (state_q          ),
    .fault_detected_o (state_q_tmr_fault)
  );
  logic state_illegal;
  assign state_illegal = ~(^state_q);

  typedef struct packed {
    /// Is the reservation valid.
    logic valid;
    /// On which address is the reservation placed.
    /// This address is aligned to the memory size
    /// implying that the reservation happen on a set size
    /// equal to the word width of the memory (32 or 64 bit).
    logic [AddrMemWidth-1:0] addr;
    /// Which core made this reservation. Important to
    /// track the reservations from different cores and
    /// to prevent any live-locking.
    logic [CoreIDWidth-1:0]  core;
  } reservation_t;
  reservation_t reservation_d, reservation_q;

  logic [AddrMemWidth-1:0]  core_add;
  logic [ProtDataWidth-1:0] core_wdata;
  logic [StrbWidth-1:0]     core_be;

  // ----------------
  // Priority arbiter (data path only)
  // ----------------
  //  DMA has priority. The handshake decision (mem_req_o/ready_o/
  //  mem_wen_o/core_arb_ready) is computed redundantly in gen_ctrl_rep
  //  below and voted -- see its comment for why -- so only the data
  //  fields remain here, single-instance.
  always_comb begin
    mem_add_o = core_add;
    mem_wdata_o = core_wdata;
    mem_be_o = core_be;

    if (dma_access_i) begin
      mem_add_o = addr_i;
      mem_wdata_o = dma_wdata_enc; // encode: DMA provides plain 32-bit data, not a codeword
      mem_be_o = wstrb_i;
    end
  end

  hsiao_ecc_enc #(
    .DataWidth (StrbWidth)
  ) i_mem_be_enc (
    .in  (mem_be_o    ),
    .out (mem_be_ecc_o)
  );

  // In case of a SC we must forward SC result from the cycle earlier.
  assign rdata_o = sc_q ? sc_result_cw : mem_rdata_i;
  assign amo_conflict_o = dma_access_i & (state_q != Idle) & (addr_q == addr_i);

  // -----
  // LR/SC
  // -----
  `FF(sc_successful_q, sc_successful, 1'b0)
  // Datapath (single-instance, not part of the deferred handshake chain --
  // see valid_rep_i's port comment): gates on mem_req_o (the voted "is this
  // a new valid transaction" decision)
  `FF(sc_q, mem_req_o & ready_o & (amo_i == AMOSC), 1'b0)
  `FF(reservation_q, reservation_d, '0)

  always_comb begin
    reservation_d = reservation_q;
    sc_successful = 1'b0;
    // new valid transaction
    if (mem_req_o && ready_o) begin

      // An SC can only pair with the most recent LR in program order.
      // Place a reservation on the address if there isn't already a valid reservation.
      // We prevent a live-lock by don't throwing away the reservation of a hart unless
      // it makes a new reservation in program order or issues any SC.
      if (amo_i == AMOLR && (!reservation_q.valid || reservation_q.core == core_id_i)) begin
        reservation_d.valid = 1'b1;
        reservation_d.addr = addr_i;
        reservation_d.core = core_id_i;
      end

      // An SC may succeed only if no store from another hart (or other device) to
      // the reservation set can be observed to have occurred between
      // the LR and the SC, and if there is no other SC between the
      // LR and itself in program order.

      // check whether another core has made a write attempt
      if ((!is_core_i || dma_access_i || core_id_i != reservation_q.core) &&
          (addr_i == reservation_q.addr) &&
          (!(amo_i inside {AMONone, AMOLR, AMOSC}) || write_i)) begin
        reservation_d.valid = 1'b0;
      end

      // An SC from the same hart clears any pending reservation.
      if (reservation_q.valid && amo_i == AMOSC && reservation_q.core == core_id_i) begin
        reservation_d.valid = 1'b0;
        sc_successful = reservation_q.addr == addr_i;
      end
    end
  end

  // -------
  // Atomics
  // -------
  logic [63:0] wdata;
  assign wdata = $unsigned(wdata_i);

  `FFL(amo_op_q, amo_i, load_amo, AMOAdd)
  `FFL(addr_q, addr_i, load_amo, '0)
  // Which word to pick.
  `FFL(idx_q, idx_d, load_amo, '0)
  `FFL(operand_b_q, (wstrb_i[0] ? wdata[31:0]  : wdata[63:32]), load_amo, '0)
  `FFL(amo_result_q, amo_result, (state_q == DoAMO), '0)

  assign idx_d = ((DataWidth == 64) ? wstrb_i[DataWidth/8/2] : 0);
  // Datapath (single-instance): gates the DATA register loads above, so
  // uses mem_req_o (the voted "new valid transaction" decision)
  assign load_amo = mem_req_o & ready_o &
          ~(amo_i inside {AMONone, AMOLR, AMOSC});
  assign operand_a = mem_rdata_i[32*idx_q+:32];

  // Data path only (single-instance, un-replicated): which address/data/
  // byte-enable to present to the SRAM. The control decision that gates
  // *when* mem_req_o/mem_wen_o assert is computed separately, redundantly,
  // in gen_ctrl_rep below.
  always_comb begin
    // pass-through by default (if it's not AMO access)
    core_add = addr_i;
    core_wdata = wdata_i;
    core_be = wstrb_i;

    unique case (state_q)
      // Third cycle: Try to write-back result.
      WriteBackAMO: begin
        core_add = addr_q;
        core_be = 'b1111 << (idx_q*4);
        core_wdata = amo_result_cw;
      end
      default:;
    endcase
  end

  // TMR-protected -- and the final vote of the whole end-to-end deferred
  // handshake chain: the FSM/priority-arbiter decision logic is replicated
  // three times, each replica driven by its own incoming valid_rep_i[t] rail
  state_e state_d_rep        [3];
  logic   mem_req_rep        [3];
  logic   ready_rep          [3];
  logic   mem_wen_rep        [3];
  logic   core_arb_ready_rep [3];
  logic   load_amo_rep       [3];

  for (genvar t = 0; t < 3; t++) begin : gen_ctrl_rep
    logic core_valid_rep, core_ready_rep, core_wen_rep;

    always_comb begin
      // pass-through by default (if it's not AMO access)
      core_valid_rep = valid_rep_i[t];
      core_ready_rep = 1'b1;
      core_wen_rep   = write_i | (sc_successful & (amo_i == AMOSC));

      state_d_rep[t] = state_q;

      unique case (state_q)
        // First cycle: Read operand a.
        Idle: if (load_amo_rep[t]) state_d_rep[t] = DoAMO;
        // Second cycle: Do atomic.
        DoAMO: begin
          core_valid_rep = 1'b0;
          core_ready_rep = 1'b0;
          state_d_rep[t] = WriteBackAMO;
        end
        // Third cycle: Try to write-back result.
        WriteBackAMO: begin
          core_valid_rep = 1'b1;
          core_ready_rep = 1'b0;
          core_wen_rep   = 1'b1;
          if (core_arb_ready_rep[t]) state_d_rep[t] = Idle;
        end
        default:;
      endcase

      // Priority arbiter decision. DMA has priority.
      mem_req_rep[t]        = core_valid_rep;
      ready_rep[t]          = core_ready_rep & ecc_sram_gnt_i;
      mem_wen_rep[t]        = core_wen_rep;
      core_arb_ready_rep[t] = 1'b1 & ecc_sram_gnt_i;

      if (dma_access_i) begin
        mem_req_rep[t]        = valid_rep_i[t];
        ready_rep[t]          = 1'b1 & ecc_sram_gnt_i;
        mem_wen_rep[t]        = write_i;
        core_arb_ready_rep[t] = 1'b0 & ecc_sram_gnt_i;
      end
    end

    assign load_amo_rep[t] = valid_rep_i[t] & ready_rep[t] &
            ~(amo_i inside {AMONone, AMOLR, AMOSC});
  end : gen_ctrl_rep

  localparam int unsigned CtrlWidth = $bits(state_e) + 4;
  logic [CtrlWidth-1:0] ctrl_rep [3];
  logic [CtrlWidth-1:0] ctrl_voted;
  logic                 core_arb_ready;

  for (genvar t = 0; t < 3; t++) begin : gen_ctrl_bundle
    assign ctrl_rep[t] = {state_d_rep[t], mem_req_rep[t], ready_rep[t],
                           mem_wen_rep[t], core_arb_ready_rep[t]};
  end : gen_ctrl_bundle

  logic ctrl_tmr_fault;
  bitwise_TMR_voter_fail #(
    .DataWidth (CtrlWidth),
    .VoterType (1)
  ) i_ctrl_voter (
    .a_i              (ctrl_rep[0]   ),
    .b_i              (ctrl_rep[1]   ),
    .c_i              (ctrl_rep[2]   ),
    .majority_o       (ctrl_voted    ),
    .fault_detected_o (ctrl_tmr_fault)
  );

  assign handshake_tmr_fault_o = ctrl_tmr_fault | state_q_tmr_fault;

  assign {state_d, mem_req_o, ready_o, mem_wen_o, core_arb_ready} = ctrl_voted;

   // ----------------
   // AMO ALU
   // ----------------

  logic [33:0] adder_sum;
  logic [32:0] adder_operand_a, adder_operand_b;

   assign adder_sum = adder_operand_a + adder_operand_b;
   /* verilator lint_off WIDTH */
   always_comb begin : amo_alu
     adder_operand_a = $signed(operand_a);
     adder_operand_b = $signed(operand_b_q);

     amo_result = operand_b_q;

     unique case (amo_op_q)
       // the default is to output operand_b
       AMOSwap:;
       AMOAdd: amo_result = adder_sum[31:0];
       AMOAnd: amo_result = operand_a & operand_b_q;
       AMOOr:  amo_result = operand_a | operand_b_q;
       AMOXor: amo_result = operand_a ^ operand_b_q;
       AMOMax: begin
         adder_operand_b = -$signed(operand_b_q);
         amo_result = adder_sum[32] ? operand_b_q : operand_a;
       end
       AMOMin: begin
         adder_operand_b = -$signed(operand_b_q);
         amo_result = adder_sum[32] ? operand_a : operand_b_q;
       end
       AMOMaxu: begin
         adder_operand_a = $unsigned(operand_a);
         adder_operand_b = -$unsigned(operand_b_q);
         amo_result = adder_sum[32] ? operand_b_q : operand_a;
       end
       AMOMinu: begin
         adder_operand_a = $unsigned(operand_a);
         adder_operand_b = -$unsigned(operand_b_q);
         amo_result = adder_sum[32] ? operand_a : operand_b_q;
       end
       default: amo_result = '0;
     endcase
   end

  // -----------------------------------------------
  // ECC encode for AMO write-back and SC result
  // When ProtDataWidth > DataWidth (ECC mode), AMO results and SC status values must
  // be re-encoded before writing to SRAM. Non-AMO codewords pass through unchanged.
  // When ProtDataWidth == DataWidth (no ECC), fall through to identity assignments.
  // -----------------------------------------------
  if (ProtDataWidth > DataWidth) begin : gen_prot_enc
    hsiao_ecc_enc #(
      .DataWidth (DataWidth),
      .ProtWidth (ProtWidth)
    ) i_amo_result_enc (
      .in  (amo_result_q),
      .out (amo_result_cw)
    );

    hsiao_ecc_enc #(
      .DataWidth (DataWidth),
      .ProtWidth (ProtWidth)
    ) i_sc_result_enc (
      .in  ({{(DataWidth-1){1'b0}}, ~sc_successful_q}),
      .out (sc_result_cw)
    );

    // DMA transfers plain 32-bit data; wdata_i[38:32] contains garbage bits from
    // the wide-to-narrow bus extraction in mem_wide_narrow_mux (not valid ECC parity).
    // Re-encode wdata_i[DataWidth-1:0] so SRAM always receives a valid codeword.
    hsiao_ecc_enc #(
      .DataWidth (DataWidth),
      .ProtWidth (ProtWidth)
    ) i_dma_wdata_enc (
      .in  (wdata_i[DataWidth-1:0]),
      .out (dma_wdata_enc)
    );
  end else begin : gen_no_prot
    assign amo_result_cw = amo_result_q << (idx_q*32);
    assign sc_result_cw  = {DataWidth/32{31'h0, ~sc_successful_q}};
    assign dma_wdata_enc = wdata_i; // no ECC: pass through unchanged
  end

  // ----------
  // Assertions
  // ----------
  // Check that data width is at least 32 bit and at most 64 bit.
  `ASSERT_INIT(DataWidthCheck, DataWidth >= 32 && DataWidth <= 64)
  // ECC mode (ProtDataWidth > DataWidth) is only supported for DataWidth=32.
  `ASSERT_INIT(ProtDataWidthCheck, ProtDataWidth >= DataWidth && (ProtDataWidth == DataWidth || DataWidth == 32))
  // Make sure that write is never set for AMOs. Checked against
  // valid_rep_i[0] as a representative single value (the protocol property
  // being checked doesn't depend on which replica).
  `ASSERT(AMOWriteEnable, valid_rep_i[0] && !amo_i inside {AMONone} |-> !write_i)
  // Make sure DMA transfers are not AMOs.
  `ASSERT(DMANoAMO, valid_rep_i[0] && dma_access_i |-> amo_i == AMONone)
  // Byte enable mask is correct
  `ASSERT(ByteMaskCorrect, valid_rep_i[0] && !amo_i inside {AMONone} |-> wstrb_i[4*idx_d+:4] == '1)

  assign fsm_illegal_state_o = state_illegal;

endmodule
