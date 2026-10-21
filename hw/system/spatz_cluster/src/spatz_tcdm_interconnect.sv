// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Florian Zaruba <zarubaf@iis.ee.ethz.ch>
// Author: Wolfgang Roenninger <wroennin@ethz.ch>

`include "mem_interface/typedef.svh"
// `include "tcdm_interface/typedef.svh"
`include "common_cells/registers.svh"
// `include "common_cells/assertions.svh"

/// Lightweight wrapper for a fixed response latency interconnect, i.e.,
/// something that can be used to interconnect memories.
module spatz_tcdm_interconnect #(
  /// Number of inputs into the interconnect (`> 0`).
  parameter int unsigned NumInp                = 32'd0,
  /// Number of outputs from the interconnect (`> 0`).
  parameter int unsigned NumOut                = 32'd0,
  /// Radix of the individual switch points of the network.
  /// Currently supported are `32'd2` and `32'd4`.
  parameter int unsigned Radix                 = 32'd2,
  /// Number of Hyperbanks to simplify FC Xbar
  parameter int unsigned NumHyperBanks         = 32'd2,
  /// Payload type of the data request ports.
  parameter type         tcdm_req_t            = logic,
  /// Payload type of the data response ports.
  parameter type         tcdm_rsp_t            = logic,
  /// Payload type of the data request ports.
  parameter type         mem_req_t             = logic,
  /// Payload type of the data response ports.
  parameter type         mem_rsp_t             = logic,
  /// Address width on the memory side. Must be smaller than the incoming
  /// address width.
  parameter int unsigned MemAddrWidth          = 32,
  /// Plain request address width on the request port (TCDMAddrWidth for the TCDM
  /// crossbar). The request address on req_i is a Hsiao codeword of this width.
  parameter int unsigned ReqAddrWidth         = MemAddrWidth,
  /// Logical data width for address routing (byte-offset and bank-select bits).
  /// Must equal the unprotected word width (e.g. 32). Do NOT set to ProtDataWidth.
  parameter int unsigned DataWidth             = 32,
  /// Actual data field width carried in each request/response (may include ECC bits).
  /// Defaults to DataWidth for backward compatibility.
  parameter int unsigned ProtDataWidth         = DataWidth,
  /// Additional user payload to route.
  parameter type         user_t                = logic,
  /// Latency of memory response (in cycles)
  parameter int unsigned MemoryResponseLatency = 1,
  parameter snitch_pkg::topo_e Topology        = snitch_pkg::LogarithmicInterconnect,
  // Misalign rows of the TCDM
  parameter logic              AddrMisalign    = 1'b0
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
  /// Passthrough of i_fc_interconnect's rsp_valid_tmr_fault_o.
  output logic                             rsp_valid_tmr_fault_o,
  /// Passthrough of i_fc_interconnect's xbar_handshake_tmr_fault_o.
  output logic                             xbar_handshake_tmr_fault_o,
  /// Passthrough of i_fc_interconnect's xbar_idx_voter_output_fault_o.
  output logic                             xbar_idx_voter_output_fault_o,
  /// Set when this module's own valid-replica vote (gating sel_d/sel_q in
  /// gen_hyperbank_demux) disagrees, OR'd across all hyperbank-demux banks.
  output logic                             ic_mem_valid_rep_tmr_fault_o,
  /// Set when this module's own ready-replica vote (gating sel_d/sel_q in
  /// gen_hyperbank_demux) disagrees, OR'd across all hyperbank-demux banks.
  output logic                             ic_mem_ready_rep_tmr_fault_o,
  /// Passthrough of i_fc_interconnect's req_valid_rep_i
  input  logic [2:0]                       req_valid_rep_i [NumInp],
  /// Passthrough of i_fc_interconnect's req_ready_rep_o.
  output logic [2:0]                       req_ready_rep_o [NumInp],
  /// Three replicas of "is mem_req_o[k] valid"
  output logic [2:0]                       mem_valid_rep_o [NumOut],
  /// Three replicas of "is mem_req_o[k]'s downstream ready", muxed the
  /// same way ic_mem_rsp[i].q_ready itself is muxed below, before being fed
  /// back into i_fc_interconnect's mem_ready_rep_i.
  input  logic [2:0]                       mem_ready_rep_i [NumOut],
  /// ECC codeword protecting mem_req_o[k].q's addr field ecc decoding errors
  output logic [1:0]                       tcdm_req_addr_ecc_o
);

  localparam int unsigned BanksPerHyperBank = NumOut / NumHyperBanks;
  localparam int unsigned VirtualMemAddrWidth = MemAddrWidth + $clog2(NumHyperBanks);
  // Address ECC (Hsiao) applied to the address as it crosses
  // i_fc_interconnect's internal crossbar mux
  localparam int unsigned VirtualMemAddrEccWidth =
    VirtualMemAddrWidth + hsiao_ecc_pkg::min_ecc(VirtualMemAddrWidth);

  localparam int unsigned ByteOffset = $clog2(DataWidth/8);
  localparam int unsigned StrbWidth = DataWidth/8;
  // Matches spatz_tcdm_fc_interconnect.sv's own WCtrlWidth/WCtrlEccWidth
  localparam int unsigned WCtrlWidth = 1 + $bits(reqrsp_pkg::amo_op_e) + DataWidth/8
                                      + $bits(user_t);
  localparam int unsigned WCtrlEccWidth = WCtrlWidth + hsiao_ecc_pkg::min_ecc(WCtrlWidth);
  typedef logic [VirtualMemAddrEccWidth-1:0] virtual_mem_addr_t;
  typedef logic [MemAddrWidth-1:0] addr_t;
  typedef logic [ProtDataWidth-1:0] data_t;
  typedef logic [StrbWidth-1:0] strb_t;

  // Define a new datatype to support larger addresses from TCDM interconnect
  // We basically pretend to the IC that our banks are combined as larger hyperbanks (Add memoryspace from two banks into one), such that we can demux the address later
  // Banks within the same hyperbank cannot be accessed simultaiousely
  `MEM_TYPEDEF_REQ_CHAN_T(mem_req_chan_t, virtual_mem_addr_t, data_t, strb_t, user_t);
  `MEM_TYPEDEF_REQ_T(virtual_mem_req_t, mem_req_chan_t);
  virtual_mem_req_t [BanksPerHyperBank-1:0] ic_mem_req;
  mem_rsp_t         [BanksPerHyperBank-1:0] ic_mem_rsp;
  logic [2:0]                                ic_mem_valid_rep [BanksPerHyperBank];
  logic [2:0]                                ic_mem_ready_rep [BanksPerHyperBank];
  // i_fc_interconnect's {write, amo, strb} ECC codeword
  logic [WCtrlEccWidth-1:0]                  ic_wctrl_ecc [BanksPerHyperBank];
  // Per-bank voter-disagreement flags for sel_d's own valid/ready vote
  logic [BanksPerHyperBank-1:0]              ic_mem_valid_rep_tmr_fault;
  logic [BanksPerHyperBank-1:0]              ic_mem_ready_rep_tmr_fault;

  // Instantiate one demux per bank in the hyperbank
  for (genvar i = 0; i < BanksPerHyperBank; i++) begin : gen_bank_connection

    // Decode the address codeword right here, before any routing decision
    logic [VirtualMemAddrWidth-1:0] ic_mem_req_addr_dec;
    logic [1:0]                     ic_mem_req_addr_err; // [0]: correctable, [1]: uncorrectable

    hsiao_ecc_dec #(
      .DataWidth (VirtualMemAddrWidth)
    ) i_ic_addr_dec (
      .in         (ic_mem_req[i].q.addr),
      .out        (ic_mem_req_addr_dec ),
      .syndrome_o (                    ),
      .err_o      (ic_mem_req_addr_err )
    );

    // Decode i_fc_interconnect's {user, write, amo, strb} codeword the same
    // way and at the same point as the address above
    logic [WCtrlWidth-1:0] ic_wctrl_dec;
    logic [1:0]            ic_wctrl_err; // [0]: correctable, [1]: uncorrectable
    logic                  ic_write_dec;
    reqrsp_pkg::amo_op_e   ic_amo_dec;
    strb_t                 ic_strb_dec;
    user_t                 ic_user_dec;

    hsiao_ecc_dec #(
      .DataWidth (WCtrlWidth)
    ) i_ic_wctrl_dec (
      .in         (ic_wctrl_ecc[i]),
      .out        (ic_wctrl_dec   ),
      .syndrome_o (               ),
      .err_o      (ic_wctrl_err   )
    );
    assign {ic_user_dec, ic_strb_dec, ic_amo_dec, ic_write_dec} = ic_wctrl_dec;

    if (NumHyperBanks > 1) begin : gen_hyperbank_demux

      logic select, sel_q, sel_d;

      `FF(sel_q, sel_d, '0, clk_i, rst_ni)

      // Demux select signal is determined by the MSB of the request address
      assign select = ic_mem_req_addr_dec[VirtualMemAddrWidth-1];
      logic ic_mem_valid_voted, ic_mem_ready_voted;

      bitwise_TMR_voter_fail #(
        .DataWidth (1),
        .VoterType (1)
      ) i_ic_mem_valid_voter (
        .a_i              (ic_mem_valid_rep[i][0]       ),
        .b_i              (ic_mem_valid_rep[i][1]       ),
        .c_i              (ic_mem_valid_rep[i][2]       ),
        .majority_o       (ic_mem_valid_voted           ),
        .fault_detected_o (ic_mem_valid_rep_tmr_fault[i])
      );
      bitwise_TMR_voter_fail #(
        .DataWidth (1),
        .VoterType (1)
      ) i_ic_mem_ready_voter (
        .a_i              (ic_mem_ready_rep[i][0]       ),
        .b_i              (ic_mem_ready_rep[i][1]       ),
        .c_i              (ic_mem_ready_rep[i][2]       ),
        .majority_o       (ic_mem_ready_voted           ),
        .fault_detected_o (ic_mem_ready_rep_tmr_fault[i])
      );
      assign sel_d = ic_mem_valid_voted && ic_mem_ready_voted ? select : sel_q;

      // Request demux (data/address -- single-instance, content)
      // decoded control and address
      assign mem_req_o[i].q.addr  = ic_mem_req_addr_dec[MemAddrWidth-1:0];
      assign mem_req_o[i].q.write = ic_write_dec;
      assign mem_req_o[i].q.data  = ic_mem_req[i].q.data;
      assign mem_req_o[i].q.strb  = ic_strb_dec;
      assign mem_req_o[i].q.user  = ic_user_dec;
      assign mem_req_o[i].q.amo   = ic_amo_dec;
      assign mem_req_o[i+BanksPerHyperBank].q.addr  = ic_mem_req_addr_dec[MemAddrWidth-1:0];
      assign mem_req_o[i+BanksPerHyperBank].q.write = ic_write_dec;
      assign mem_req_o[i+BanksPerHyperBank].q.data  = ic_mem_req[i].q.data;
      assign mem_req_o[i+BanksPerHyperBank].q.strb  = ic_strb_dec;
      assign mem_req_o[i+BanksPerHyperBank].q.user  = ic_user_dec;
      assign mem_req_o[i+BanksPerHyperBank].q.amo   = ic_amo_dec;

      // Request demux (handshake -- all three replicas demuxed identically
      // by the same single `select`, keeping each replica independent).
      for (genvar t = 0; t < 3; t++) begin : gen_valid_demux_rep
        assign mem_valid_rep_o[i][t]                  = !select && ic_mem_valid_rep[i][t];
        assign mem_valid_rep_o[i+BanksPerHyperBank][t] = select && ic_mem_valid_rep[i][t];
      end : gen_valid_demux_rep
      // A placeholder for mem_req_o[*].q_valid -- replica 0. Only read by assertions.
      assign mem_req_o[i].q_valid                  = mem_valid_rep_o[i][0];
      assign mem_req_o[i+BanksPerHyperBank].q_valid = mem_valid_rep_o[i+BanksPerHyperBank][0];

      // Response mux (currently assumes response arrives exactly one cycle after request)
      for (genvar t = 0; t < 3; t++) begin : gen_ready_mux_rep
        assign ic_mem_ready_rep[i][t] = select ? mem_ready_rep_i[i+BanksPerHyperBank][t]
                                                : mem_ready_rep_i[i][t];
      end : gen_ready_mux_rep
      assign ic_mem_rsp[i].q_ready = ic_mem_ready_rep[i][0];
      assign ic_mem_rsp[i].p = sel_q ? mem_rsp_i[i+BanksPerHyperBank].p : mem_rsp_i[i].p;

    end else begin : gen_no_demux

      // Demux and mux degenerate to direct one-to-one connections
      assign mem_req_o[i].q.addr   = ic_mem_req_addr_dec[MemAddrWidth-1:0];
      assign mem_req_o[i].q.write  = ic_write_dec;
      assign mem_req_o[i].q.data   = ic_mem_req[i].q.data;
      assign mem_req_o[i].q.strb   = ic_strb_dec;
      assign mem_req_o[i].q.user   = ic_user_dec;
      assign mem_req_o[i].q.amo    = ic_amo_dec;

      assign mem_valid_rep_o[i] = ic_mem_valid_rep[i];
      assign mem_req_o[i].q_valid = mem_valid_rep_o[i][0];

      assign ic_mem_ready_rep[i] = mem_ready_rep_i[i];
      assign ic_mem_rsp[i].q_ready = ic_mem_ready_rep[i][0];
      assign ic_mem_rsp[i].p       = mem_rsp_i[i].p;

      // No sel_d/sel_q vote exists in this branch (NumHyperBanks == 1, so
      // there's nothing to demux), so nothing to flag.
      assign ic_mem_valid_rep_tmr_fault[i] = 1'b0;
      assign ic_mem_ready_rep_tmr_fault[i] = 1'b0;

    end
  end

  assign ic_mem_valid_rep_tmr_fault_o = |ic_mem_valid_rep_tmr_fault;
  assign ic_mem_ready_rep_tmr_fault_o = |ic_mem_ready_rep_tmr_fault;

  spatz_tcdm_fc_interconnect #(
    .NumInp                (NumInp               ),
    .NumOut                (BanksPerHyperBank    ),
    .tcdm_req_t            (tcdm_req_t           ),
    .tcdm_rsp_t            (tcdm_rsp_t           ),
    .mem_req_t             (virtual_mem_req_t    ),
    .mem_rsp_t             (mem_rsp_t            ),
    .user_t                (user_t               ),
    .ReqAddrWidth          (ReqAddrWidth         ),
    .MemAddrWidth          (VirtualMemAddrWidth  ),
    .ProtMemAddrWidth      (VirtualMemAddrEccWidth),
    .DataWidth             (DataWidth            ),
    .ProtDataWidth         (ProtDataWidth        ),
    .MemoryResponseLatency (MemoryResponseLatency),
    .AddrMisalign          (AddrMisalign         )
  ) i_fc_interconnect (
    .clk_i                 ,
    .rst_ni                ,
    .req_i     (req_i     ),
    .rsp_o     (rsp_o     ),
    .mem_req_o (ic_mem_req),
    .mem_rsp_i (ic_mem_rsp),
    .rsp_valid_tmr_fault_o      (rsp_valid_tmr_fault_o     ),
    .xbar_handshake_tmr_fault_o (xbar_handshake_tmr_fault_o),
    .xbar_idx_voter_output_fault_o (xbar_idx_voter_output_fault_o),
    .req_valid_rep_i (req_valid_rep_i ),
    .req_ready_rep_o (req_ready_rep_o ),
    .mem_valid_rep_o (ic_mem_valid_rep),
    .mem_ready_rep_i (ic_mem_ready_rep),
    .out_wctrl_ecc_o (ic_wctrl_ecc    ),
    .tcdm_req_addr_ecc_o (tcdm_req_addr_ecc_o)
  );

endmodule
