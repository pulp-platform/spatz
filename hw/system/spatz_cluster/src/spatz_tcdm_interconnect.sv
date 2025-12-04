// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Florian Zaruba <zarubaf@iis.ee.ethz.ch>
// Author: Wolfgang Roenninger <wroennin@ethz.ch>

`include "mem_interface/typedef.svh"

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
  /// Data size of the interconnect. Only the data portion counts. The offsets
  /// into the address are derived from this.
  parameter int unsigned DataWidth             = 32,
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
  input  mem_rsp_t            [NumOut-1:0] mem_rsp_i
);

  localparam int unsigned ByteOffset = $clog2(DataWidth/8);
  localparam int unsigned StrbWidth = DataWidth/8;
  typedef logic [MemAddrWidth-1:0] addr_t;
  typedef logic [DataWidth-1:0] data_t;
  typedef logic [StrbWidth-1:0] strb_t;
  `MEM_TYPEDEF_REQ_CHAN_T(mem_req_chan_t, addr_t, data_t, strb_t, user_t);

  // Width of the bank select signal.
  localparam int unsigned SelWidth = cf_math_pkg::idx_width(NumOut);
  typedef logic [SelWidth-1:0] select_t;
  select_t [NumInp-1:0] bank_select;

  typedef struct packed {
    // Which bank was selected.
    select_t bank_select;
    // The response is valid.
    logic valid;
  } rsp_t;

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
    assign bank_select[i] = AddrMisalign ? addr_misaligned[i][ByteOffset+:SelWidth] : req_i[i].q.addr[ByteOffset+:SelWidth];
  end

  mem_req_chan_t [NumInp-1:0] in_req;
  mem_req_chan_t [NumOut-1:0] out_req;

  logic [NumInp-1:0] req_q_valid_flat, rsp_q_ready_flat;
  logic [NumOut-1:0] mem_q_valid_flat, mem_q_ready_flat;

  // The usual struct packing unpacking.
  for (genvar i = 0; i < NumInp; i++) begin : gen_flat_inp
    assign req_q_valid_flat[i] = req_i[i].q_valid;
    assign rsp_o[i].q_ready = rsp_q_ready_flat[i];
    assign in_req[i] = '{
      addr: AddrMisalign ? addr_misaligned[i][ByteOffset+SelWidth+:MemAddrWidth] : req_i[i].q.addr[ByteOffset+SelWidth+:MemAddrWidth],
      write: req_i[i].q.write,
      amo: req_i[i].q.amo,
      data: req_i[i].q.data,
      strb: req_i[i].q.strb,
      user: req_i[i].q.user
    };
  end

  for (genvar i = 0; i < NumOut; i++) begin : gen_flat_oup
    assign mem_req_o[i].q_valid = mem_q_valid_flat[i];
    assign mem_q_ready_flat[i] = mem_rsp_i[i].q_ready;
    assign mem_req_o[i].q = out_req[i];
  end

  // ------------
  // Request Side
  // ------------
  // We need to arbitrate the requests coming from the input side and resolve
  // potential bank conflicts. Therefore a full arbitration tree is needed.
  if (Topology == snitch_pkg::LogarithmicInterconnect) begin : gen_xbar
    stream_xbar #(
      .NumInp      ( NumInp    ),
      .NumOut      ( NumOut    ),
      .payload_t   ( mem_req_chan_t ),
      .OutSpillReg ( 1'b0      ),
      .ExtPrio     ( 1'b0      ),
      .AxiVldRdy   ( 1'b1      ),
      .LockIn      ( 1'b1      )
    ) i_stream_xbar (
      .clk_i,
      .rst_ni,
      .flush_i ( 1'b0 ),
      .rr_i    ( '0 ),
      .data_i  ( in_req ),
      .sel_i   ( bank_select ),
      .valid_i ( req_q_valid_flat ),
      .ready_o ( rsp_q_ready_flat ),
      .data_o  ( out_req ),
      .idx_o   ( ),
      .valid_o ( mem_q_valid_flat ),
      .ready_i ( mem_q_ready_flat )
    );
  end else if (Topology == snitch_pkg::OmegaNet) begin : gen_omega_net
    stream_omega_net #(
      .NumInp      ( NumInp        ),
      .NumOut      ( NumOut        ),
      .payload_t   ( mem_req_chan_t ),
      .SpillReg    ( 1'b0          ),
      .ExtPrio     ( 1'b0          ),
      .AxiVldRdy   ( 1'b1          ),
      .LockIn      ( 1'b1          ),
      .Radix       ( Radix         )
    ) i_stream_omega_net (
      .clk_i,
      .rst_ni,
      .flush_i ( 1'b0 ),
      .rr_i    ( '0 ),
      .data_i  ( in_req ),
      .sel_i   ( bank_select ),
      .valid_i ( req_q_valid_flat ),
      .ready_o ( rsp_q_ready_flat ),
      .data_o  ( out_req ),
      .idx_o   ( ),
      .valid_o ( mem_q_valid_flat ),
      .ready_i ( mem_q_ready_flat )
    );
  end

  // -------------
  // Response Side
  // -------------
  // A simple multiplexer is sufficient here.
  for (genvar i = 0; i < NumInp; i++) begin : gen_rsp_mux
    rsp_t out_rsp_mux, in_rsp_mux;
    assign in_rsp_mux = '{
      bank_select: bank_select[i],
      valid: req_i[i].q_valid & rsp_o[i].q_ready
    };
    // A this is a fixed latency interconnect a simple shift register is
    // sufficient to track the arbitration decisions.
    shift_reg #(
      .dtype ( rsp_t ),
      .Depth ( MemoryResponseLatency )
    ) i_shift_reg (
      .clk_i,
      .rst_ni,
      .d_i ( in_rsp_mux ),
      .d_o ( out_rsp_mux )
    );
    assign rsp_o[i].p.data = mem_rsp_i[out_rsp_mux.bank_select].p.data;
    assign rsp_o[i].p_valid = out_rsp_mux.valid;
  end


endmodule
