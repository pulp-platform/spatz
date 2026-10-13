// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Florian Zaruba <zarubaf@iis.ee.ethz.ch>
// Author: Wolfgang Roenninger <wroennin@ethz.ch>

`include "mem_interface/typedef.svh"

/// Lightweight wrapper for a fixed response latency interconnect, i.e.,
/// something that can be used to interconnect memories.
module spatz_tcdm_fc_interconnect #(
  /// Number of inputs into the interconnect (`> 0`).
  parameter int unsigned NumInp                = 32'd0,
  /// Number of outputs from the interconnect (`> 0`).
  parameter int unsigned NumOut                = 32'd0,
  /// Radix of the individual switch points of the network.
  /// Currently supported are `32'd2` and `32'd4`.
  parameter int unsigned Radix                 = 32'd2,
  /// Payload type of the data request ports.
  parameter type         tcdm_req_t            = logic,
  /// Payload type of the data response ports.
  parameter type         tcdm_rsp_t            = logic,
  /// Payload type of the data request ports.
  parameter type         mem_req_t             = logic,
  /// Payload type of the data response ports.
  parameter type         mem_rsp_t             = logic,
  /// Address width on the memory side. Must be smaller than the incoming
  /// address width. Used for all routing/misalignment arithmetic (bank_select,
  /// addr_misaligned, etc.) -- do NOT set to ProtMemAddrWidth.
  parameter int unsigned MemAddrWidth          = 32,
  /// Actual address field width carried in in_req/out_req/mem_req_o
  /// Defaults to MemAddrWidth (no ECC, backward compatible).
  parameter int unsigned ProtMemAddrWidth      = MemAddrWidth,
  /// Data size of the interconnect. Only the data portion counts. The offsets
  /// into the address are derived from this. Must equal the unprotected word
  /// width (e.g. 32) -- used for byte-offset/strobe sizing only. Do NOT set
  /// to ProtDataWidth.
  parameter int unsigned DataWidth             = 32,
  /// Actual data field width carried in each request/response (may include
  /// ECC bits). Defaults to DataWidth for backward compatibility.
  parameter int unsigned ProtDataWidth         = DataWidth,
  /// Additional user payload to route.
  parameter type         user_t                = logic,
  /// Latency of memory response (in cycles)
  parameter int unsigned MemoryResponseLatency = 1,
  parameter snitch_pkg::topo_e Topology        = snitch_pkg::LogarithmicInterconnect,
  // Misalign rows of the TCDM
  parameter logic              AddrMisalign    = 1'b0,
  /// Plaintext width of the {user, write, amo, strb} bundle protected together by out_wctrl_ecc_o
  /// Derived. *Do not override*
  localparam int unsigned WCtrlWidth           = 1 + $bits(reqrsp_pkg::amo_op_e) + DataWidth/8
                                                + $bits(user_t),
  /// Codeword width of out_wctrl_ecc_o (WCtrlWidth plus Hsiao parity bits).
  /// Derived. *Do not override*
  localparam int unsigned WCtrlEccWidth        = WCtrlWidth + hsiao_ecc_pkg::min_ecc(WCtrlWidth)
) (
  /// Clock, positive edge triggered.
  input  logic                             clk_i,
  /// Reset, active low.
  input  logic                             rst_ni,
  /// Request port.
  input  tcdm_req_t           [NumInp-1:0] req_i,
  /// Resposne port.
  output tcdm_rsp_t           [NumInp-1:0] rsp_o,
  /// Memory Side
  /// Request.
  output mem_req_t            [NumOut-1:0] mem_req_o,
  /// Response.
  input  mem_rsp_t            [NumOut-1:0] mem_rsp_i,
  /// OR of every per-input response-valid, response-bank-select, AND
  /// request-grant (q_ready) vote's TMR fault.
  output logic                             rsp_valid_tmr_fault_o,
  /// Set when the triplicated switch-network idx_o vote's three replicas disagree
  output logic                             xbar_handshake_tmr_fault_o,
  /// Set when the vote's own OUTPUT wirediverges from a second, 
  /// independent vote on the same three replicas 
  output logic                             xbar_idx_voter_output_fault_o,
  /// Three independently-computed replicas of "is req_i[k] valid"
  input  logic [2:0]                       req_valid_rep_i [NumInp],
  /// Three replicas of "is req_i[k] granted"
  output logic [2:0]                       req_ready_rep_o [NumInp],
  /// Three replicas of "is mem_req_o[k] valid"
  output logic [2:0]                       mem_valid_rep_o [NumOut],
  /// Three replicas of "is mem_req_o[k]'s downstream ready"
  input  logic [2:0]                       mem_ready_rep_i [NumOut],
  /// ECC codeword protecting mem_req_o[k].q's {write, amo, strb} fields
  output logic [WCtrlEccWidth-1:0]         out_wctrl_ecc_o [NumOut]
);

  localparam int unsigned ByteOffset = $clog2(DataWidth/8);
  localparam int unsigned StrbWidth = DataWidth/8;
  // Plain MemAddrWidth-bit type -- used for internal routing/misalignment
  // arithmetic only (row/addr_misaligned below). NOT the type carried by
  // mem_req_chan_t -- see addr_prot_t.
  typedef logic [MemAddrWidth-1:0] addr_t;
  typedef logic [ProtDataWidth-1:0] data_t;
  typedef logic [StrbWidth-1:0] strb_t;
  // ECC-protected width used for mem_req_chan_t's addr field
  typedef logic [ProtMemAddrWidth-1:0] addr_prot_t;
  `MEM_TYPEDEF_REQ_CHAN_T(mem_req_chan_t, addr_prot_t, data_t, strb_t, user_t);

  // Width of the bank select signal.
  localparam int unsigned SelWidth = cf_math_pkg::idx_width(NumOut);
  typedef logic [SelWidth-1:0] select_t;
  select_t [NumInp-1:0] bank_select;

  // Generate the `bank_select` signal based on the address.
  // This generates a bank interleaved addressing scheme, where consecutive
  // addresses are routed to individual banks.

  // Misalignment logic for TCDM to achieve no conflicts within 2x Spatz VLSU interfaces within a core
  // Misalignment pattern :
  //                               Superbank-0   Superbank-1
  //                        row1 : Block-0       Block-1
  //                        row2 : Block-3       Block-2
  //                        row3 : Block-5       Block-4
  //                        row4 : Block-6       Block-7

  localparam int unsigned ROWSIZE = $clog2(DataWidth * NumOut / 8);
  localparam int unsigned ADDRWIDTH = $bits(req_i[0].q.addr);
  addr_t [NumInp-1:0] row;
  logic [NumInp-1:0] [ROWSIZE-1:0] addr_shift;
  logic [NumInp-1:0] [ADDRWIDTH-1:0] addr_misaligned;

  always_comb begin : gen_addr_misalign
    row = '0;
    addr_shift = '0;
    addr_misaligned = '0;
    for (int i = 0; i < NumInp; i++) begin
      row[i] = req_i[i].q.addr[ADDRWIDTH-1 : ROWSIZE];
      addr_shift[i] = (row[i][1:0] == 2'b00) || (row[i][1:0] == 2'b11) ? 0 : (DataWidth * NumOut / 8) / 2;
      addr_misaligned[i] = req_i[i].q.addr + addr_shift[i];
      addr_misaligned[i][ADDRWIDTH-1 : ROWSIZE] = req_i[i].q.addr[ADDRWIDTH-1 : ROWSIZE];
    end
  end

  for (genvar i = 0; i < NumInp; i++) begin : gen_bank_select
    assign bank_select[i] = AddrMisalign ? NumOut == 1 ? 1'b0 : addr_misaligned[i][ByteOffset+:SelWidth]
                                         : NumOut == 1 ? 1'b0 : req_i[i].q.addr[ByteOffset+:SelWidth];
  end

  mem_req_chan_t [NumInp-1:0] in_req;
  mem_req_chan_t [NumOut-1:0] out_req;

  // The usual struct packing unpacking.
  //
  // Address ECC: encode the payload address right here, before it enters
  // in_req/i_stream_xbar's data path.
  logic [MemAddrWidth-1:0] in_req_addr_plain [NumInp-1:0];
  // {write, amo, strb} bundle, ECC-protected across the crossbar's internal mux
  logic [WCtrlEccWidth-1:0] in_req_wctrl_ecc [NumInp-1:0];

  for (genvar i = 0; i < NumInp; i++) begin : gen_flat_inp
    assign in_req_addr_plain[i] = AddrMisalign
      ? addr_misaligned[i][ByteOffset+$clog2(NumOut)+:MemAddrWidth]
      : req_i[i].q.addr[ByteOffset+$clog2(NumOut)+:MemAddrWidth];

    if (ProtMemAddrWidth > MemAddrWidth) begin : gen_addr_enc
      hsiao_ecc_enc #(
        .DataWidth (MemAddrWidth),
        .ProtWidth (ProtMemAddrWidth - MemAddrWidth)
      ) i_in_addr_enc (
        .in  (in_req_addr_plain[i]),
        .out (in_req[i].addr)
      );
    end else begin : gen_addr_no_ecc
      assign in_req[i].addr = in_req_addr_plain[i];
    end

    hsiao_ecc_enc #(
      .DataWidth (WCtrlWidth)
    ) i_in_wctrl_enc (
      .in  ({req_i[i].q.user, req_i[i].q.strb, req_i[i].q.amo, req_i[i].q.write}),
      .out (in_req_wctrl_ecc[i]                                                 )
    );

  // .data is the real, consumed payload, muxed through as-is by
  // gen_xbar_data_mux/gen_omega_data_mux below.
  // .write/.amo/.strb/.user are simple placeholders here -- the real values
  // are carried ECC-protected in in_req_wctrl_ecc/out_wctrl_ecc_o and decoded
  // back out at the far end (spatz_tcdm_interconnect.sv's i_ic_wctrl_dec).
    assign in_req[i].write = req_i[i].q.write;
    assign in_req[i].amo   = req_i[i].q.amo;
    assign in_req[i].data  = req_i[i].q.data;
    assign in_req[i].strb  = req_i[i].q.strb;
    assign in_req[i].user  = '0;
  end

  for (genvar i = 0; i < NumOut; i++) begin : gen_flat_oup
    // Representative placeholder signals.
    assign mem_req_o[i].q_valid = mem_valid_rep_o[i][0];
    assign mem_req_o[i].q = out_req[i];
  end

  // ------------
  // Request Side
  // ------------
  // We need to arbitrate the requests coming from the input side and resolve
  // potential bank conflicts. Therefore a full arbitration tree is needed.
  if (Topology == snitch_pkg::LogarithmicInterconnect) begin : gen_xbar
    // End-to-end handshake TMR: each of the three stream_xbar replicas is
    // driven by its OWN incoming rail (req_valid_rep_i[*][t]/mem_ready_rep_i[*][t])
    //
    // idx_o is voted locally: it selects the single un-replicated
    // payload mux below (gen_xbar_data_mux).
    localparam int unsigned XbarIdxWidth = (NumInp > 32'd1) ? unsigned'($clog2(NumInp)) : 32'd1;

    logic [NumInp-1:0]                   xbar_valid_i_rep [3];
    logic [NumOut-1:0]                   xbar_ready_i_rep [3];
    logic [NumInp-1:0]                   xbar_ready_rep   [3];
    logic [NumOut-1:0]                   xbar_valid_rep   [3];
    logic [NumOut-1:0][XbarIdxWidth-1:0] xbar_idx_rep     [3];

    for (genvar t = 0; t < 3; t++) begin : gen_xbar_rep
      for (genvar k = 0; k < NumInp; k++) begin : gen_xbar_valid_i
        assign xbar_valid_i_rep[t][k] = req_valid_rep_i[k][t];
      end : gen_xbar_valid_i
      for (genvar k = 0; k < NumOut; k++) begin : gen_xbar_ready_i
        assign xbar_ready_i_rep[t][k] = mem_ready_rep_i[k][t];
      end : gen_xbar_ready_i

      stream_xbar #(
        .NumInp      ( NumInp    ),
        .NumOut      ( NumOut    ),
        .payload_t   ( logic     ),
        .OutSpillReg ( 1'b0      ),
        .ExtPrio     ( 1'b0      ),
        .AxiVldRdy   ( 1'b1      ),
        .LockIn      ( 1'b1      )
      ) i_stream_xbar (
        .clk_i,
        .rst_ni,
        .flush_i ( 1'b0 ),
        .rr_i    ( '0 ),
        .data_i  ( '0 ),
        .sel_i   ( bank_select ),
        .valid_i ( xbar_valid_i_rep[t] ),
        .ready_o ( xbar_ready_rep[t] ),
        .data_o  ( ),
        .idx_o   ( xbar_idx_rep[t] ),
        .valid_o ( xbar_valid_rep[t] ),
        .ready_i ( xbar_ready_i_rep[t] )
      );

      for (genvar k = 0; k < NumInp; k++) begin : gen_req_ready_rep
        assign req_ready_rep_o[k][t] = xbar_ready_rep[t][k];
      end : gen_req_ready_rep
      for (genvar k = 0; k < NumOut; k++) begin : gen_mem_valid_rep
        assign mem_valid_rep_o[k][t] = xbar_valid_rep[t][k];
      end : gen_mem_valid_rep
    end : gen_xbar_rep

    localparam int unsigned XbarIdxVoteWidth = NumOut*XbarIdxWidth;
    logic [XbarIdxVoteWidth-1:0] xbar_idx_flat_rep [3];
    logic [XbarIdxVoteWidth-1:0] xbar_idx_flat_voted;

    for (genvar t = 0; t < 3; t++) begin : gen_xbar_idx_bundle
      assign xbar_idx_flat_rep[t] = xbar_idx_rep[t];
    end : gen_xbar_idx_bundle

    bitwise_TMR_voter_fail #(
      .DataWidth (XbarIdxVoteWidth),
      .VoterType (1)
    ) i_xbar_idx_voter (
      .a_i              (xbar_idx_flat_rep[0]      ),
      .b_i              (xbar_idx_flat_rep[1]      ),
      .c_i              (xbar_idx_flat_rep[2]      ),
      .majority_o       (xbar_idx_flat_voted        ),
      .fault_detected_o (xbar_handshake_tmr_fault_o )
    );

    // a secondary checker for faults on the output of the voter itself
    logic [XbarIdxVoteWidth-1:0] xbar_idx_flat_voted_chk;

    bitwise_TMR_voter_fail #(
      .DataWidth (XbarIdxVoteWidth),
      .VoterType (1)
    ) i_xbar_idx_voter_chk (
      .a_i              (xbar_idx_flat_rep[0]      ),
      .b_i              (xbar_idx_flat_rep[1]      ),
      .c_i              (xbar_idx_flat_rep[2]      ),
      .majority_o       (xbar_idx_flat_voted_chk    ),
      .fault_detected_o (/* redundant with i_xbar_idx_voter's own fault_detected_o */)
    );

    assign xbar_idx_voter_output_fault_o =
      (xbar_idx_flat_voted != xbar_idx_flat_voted_chk);

    logic [NumOut-1:0][XbarIdxWidth-1:0] xbar_idx_voted;
    assign xbar_idx_voted = xbar_idx_flat_voted;

    // Single, un-replicated payload mux, selected by the voted winning
    // input index -- same rationale as gen_tcdm_signals's un-replicated
    // tcdm_req[j].q mux in spatz_cluster.sv. in_req_wctrl_ecc is muxed by
    // the same xbar_idx_voted as in_req itself, so the {write, amo, strb}
    // codeword stays consistent with whichever input actually won.
    for (genvar j = 0; j < NumOut; j++) begin : gen_xbar_data_mux
      assign out_req[j]         = in_req[xbar_idx_voted[j]];
      assign out_wctrl_ecc_o[j] = in_req_wctrl_ecc[xbar_idx_voted[j]];
    end : gen_xbar_data_mux
  end else if (Topology == snitch_pkg::OmegaNet) begin : gen_omega_net
    // End-to-end handshake TMR, mirroring gen_xbar above
    localparam int unsigned XbarIdxWidth = (NumInp > 32'd1) ? unsigned'($clog2(NumInp)) : 32'd1;

    logic [NumInp-1:0]                   omega_valid_i_rep [3];
    logic [NumOut-1:0]                   omega_ready_i_rep [3];
    logic [NumInp-1:0]                   omega_ready_rep   [3];
    logic [NumOut-1:0]                   omega_valid_rep   [3];
    logic [NumOut-1:0][XbarIdxWidth-1:0] omega_idx_rep     [3];

    for (genvar t = 0; t < 3; t++) begin : gen_omega_rep
      for (genvar k = 0; k < NumInp; k++) begin : gen_omega_valid_i
        assign omega_valid_i_rep[t][k] = req_valid_rep_i[k][t];
      end : gen_omega_valid_i
      for (genvar k = 0; k < NumOut; k++) begin : gen_omega_ready_i
        assign omega_ready_i_rep[t][k] = mem_ready_rep_i[k][t];
      end : gen_omega_ready_i

      stream_omega_net #(
        .NumInp      ( NumInp    ),
        .NumOut      ( NumOut    ),
        .Radix       ( Radix     ),
        .payload_t   ( logic     ),
        .SpillReg    ( 1'b0      ),
        .ExtPrio     ( 1'b0      ),
        .AxiVldRdy   ( 1'b1      ),
        .LockIn      ( 1'b1      )
      ) i_stream_omega_net (
        .clk_i,
        .rst_ni,
        .flush_i ( 1'b0 ),
        .rr_i    ( '0 ),
        .data_i  ( '0 ),
        .sel_i   ( bank_select ),
        .valid_i ( omega_valid_i_rep[t] ),
        .ready_o ( omega_ready_rep[t] ),
        .data_o  ( ),
        .idx_o   ( omega_idx_rep[t] ),
        .valid_o ( omega_valid_rep[t] ),
        .ready_i ( omega_ready_i_rep[t] )
      );

      for (genvar k = 0; k < NumInp; k++) begin : gen_req_ready_rep
        assign req_ready_rep_o[k][t] = omega_ready_rep[t][k];
      end : gen_req_ready_rep
      for (genvar k = 0; k < NumOut; k++) begin : gen_mem_valid_rep
        assign mem_valid_rep_o[k][t] = omega_valid_rep[t][k];
      end : gen_mem_valid_rep
    end : gen_omega_rep

    localparam int unsigned XbarIdxVoteWidth = NumOut*XbarIdxWidth;
    logic [XbarIdxVoteWidth-1:0] omega_idx_flat_rep [3];
    logic [XbarIdxVoteWidth-1:0] omega_idx_flat_voted;

    for (genvar t = 0; t < 3; t++) begin : gen_omega_idx_bundle
      assign omega_idx_flat_rep[t] = omega_idx_rep[t];
    end : gen_omega_idx_bundle

    bitwise_TMR_voter_fail #(
      .DataWidth (XbarIdxVoteWidth),
      .VoterType (1)
    ) i_omega_idx_voter (
      .a_i              (omega_idx_flat_rep[0]     ),
      .b_i              (omega_idx_flat_rep[1]     ),
      .c_i              (omega_idx_flat_rep[2]     ),
      .majority_o       (omega_idx_flat_voted       ),
      .fault_detected_o (xbar_handshake_tmr_fault_o )
    );

    // a secondary checker for faults on the output of the voter itself
    logic [XbarIdxVoteWidth-1:0] omega_idx_flat_voted_chk;

    bitwise_TMR_voter_fail #(
      .DataWidth (XbarIdxVoteWidth),
      .VoterType (1)
    ) i_omega_idx_voter_chk (
      .a_i              (omega_idx_flat_rep[0]      ),
      .b_i              (omega_idx_flat_rep[1]      ),
      .c_i              (omega_idx_flat_rep[2]      ),
      .majority_o       (omega_idx_flat_voted_chk   ),
      .fault_detected_o (/* redundant with i_omega_idx_voter's own fault_detected_o */)
    );

    assign xbar_idx_voter_output_fault_o =
      (omega_idx_flat_voted != omega_idx_flat_voted_chk);

    logic [NumOut-1:0][XbarIdxWidth-1:0] omega_idx_voted;
    assign omega_idx_voted = omega_idx_flat_voted;

    // Single, un-replicated payload mux, selected by the voted winning
    // input index
    for (genvar j = 0; j < NumOut; j++) begin : gen_omega_data_mux
      assign out_req[j]         = in_req[omega_idx_voted[j]];
      assign out_wctrl_ecc_o[j] = in_req_wctrl_ecc[omega_idx_voted[j]];
    end : gen_omega_data_mux
  end

  // -------------
  // Response Side
  // -------------
  // A simple multiplexer is sufficient here.
  logic [NumInp-1:0] rsp_valid_tmr_fault_rep;

  for (genvar i = 0; i < NumInp; i++) begin : gen_rsp_mux
    logic    in_rsp_valid;
    select_t in_rsp_bank_select;

    logic req_ready_voted;
    logic req_ready_tmr_fault;
    bitwise_TMR_voter_fail #(
      .DataWidth (1),
      .VoterType (1)
    ) i_req_ready_voter (
      .a_i              (req_ready_rep_o[i][0]),
      .b_i              (req_ready_rep_o[i][1]),
      .c_i              (req_ready_rep_o[i][2]),
      .majority_o       (req_ready_voted      ),
      .fault_detected_o (req_ready_tmr_fault  )
    );
    assign rsp_o[i].q_ready = req_ready_voted;

    assign in_rsp_valid       = req_i[i].q_valid & rsp_o[i].q_ready;
    assign in_rsp_bank_select = bank_select[i];

    // A this is a fixed latency interconnect a simple shift register is
    // sufficient to track the arbitration decisions.
    //
    // TMR-protected: whether a response for this input is due this cycle
    // -- a pure handshake flag.
    logic out_rsp_valid_rep [3];
    for (genvar t = 0; t < 3; t++) begin : gen_rsp_valid_rep
      shift_reg #(
        .dtype ( logic ),
        .Depth ( MemoryResponseLatency )
      ) i_shift_reg_valid (
        .clk_i,
        .rst_ni,
        .d_i ( in_rsp_valid ),
        .d_o ( out_rsp_valid_rep[t] )
      );
    end : gen_rsp_valid_rep

    logic out_rsp_valid;
    logic rsp_valid_tmr_fault;
    bitwise_TMR_voter_fail #(
      .DataWidth (1),
      .VoterType (1)
    ) i_rsp_valid_voter (
      .a_i              (out_rsp_valid_rep[0]),
      .b_i              (out_rsp_valid_rep[1]),
      .c_i              (out_rsp_valid_rep[2]),
      .majority_o       (out_rsp_valid       ),
      .fault_detected_o (rsp_valid_tmr_fault )
    );

    // TMR-protected: which bank this input's response is due from.
    select_t out_rsp_bank_select_rep [3];
    for (genvar t = 0; t < 3; t++) begin : gen_rsp_bank_select_rep
      shift_reg #(
        .dtype ( select_t ),
        .Depth ( MemoryResponseLatency )
      ) i_shift_reg_bank_select (
        .clk_i,
        .rst_ni,
        .d_i ( in_rsp_bank_select ),
        .d_o ( out_rsp_bank_select_rep[t] )
      );
    end : gen_rsp_bank_select_rep

    select_t out_rsp_bank_select;
    logic    rsp_bank_select_tmr_fault;
    bitwise_TMR_voter_fail #(
      .DataWidth (SelWidth),
      .VoterType (1)
    ) i_rsp_bank_select_voter (
      .a_i              (out_rsp_bank_select_rep[0]),
      .b_i              (out_rsp_bank_select_rep[1]),
      .c_i              (out_rsp_bank_select_rep[2]),
      .majority_o       (out_rsp_bank_select       ),
      .fault_detected_o (rsp_bank_select_tmr_fault )
    );

    assign rsp_o[i].p.data = mem_rsp_i[out_rsp_bank_select].p.data;
    assign rsp_o[i].p_valid = out_rsp_valid;
    assign rsp_valid_tmr_fault_rep[i] =
      rsp_valid_tmr_fault | rsp_bank_select_tmr_fault | req_ready_tmr_fault;
  end

  assign rsp_valid_tmr_fault_o = |rsp_valid_tmr_fault_rep;

endmodule
