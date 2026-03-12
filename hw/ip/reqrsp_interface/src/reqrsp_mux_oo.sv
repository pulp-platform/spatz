// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Florian Zaruba <zarubaf@iis.ee.ethz.ch>

`include "reqrsp_interface/typedef.svh"

/// Multiplex multiple `reqrsp` ports onto one, based on arbitration.
module reqrsp_mux_oo #(
    /// Number of input ports.
    parameter int unsigned               NrPorts      = 2,
    /// Address width of the interface.
    parameter int unsigned               AddrWidth    = 0,
    /// Data width of the interface.
    parameter int unsigned               DataWidth    = 0,
    /// Request type.
    parameter type                       req_t        = logic,
    /// Response type.
    parameter type                       rsp_t        = logic,
    /// Amount of outstanding responses. Determines the FIFO size.
    parameter int unsigned               RespDepth    = 8,
    /// Cut timing paths on the request path. Incurs a cycle additional latency.
    /// Registers are inserted at the slave side.
    parameter bit          [NrPorts-1:0] RegisterReq  = '0,
    /// Dependent parameter, do **not** overwrite.
    /// Width of the arbitrated index.
    parameter int unsigned IdxWidth   = (NrPorts > 32'd1) ? unsigned'($clog2(NrPorts)) : 32'd1
) (
    input  logic                clk_i,
    input  logic                rst_ni,
    input  req_t [NrPorts-1:0]  slv_req_i,
    output rsp_t [NrPorts-1:0]  slv_rsp_o,
    output req_t                mst_req_o,
    input  rsp_t                mst_rsp_i,
    // Response wiring signal
    input  logic [IdxWidth-1:0] idx_i
);

  typedef logic [AddrWidth-1:0] addr_t;
  typedef logic [DataWidth-1:0] data_t;
  typedef logic [DataWidth/8-1:0] strb_t;

  `REQRSP_TYPEDEF_REQ_CHAN_T(req_chan_t, addr_t, data_t, strb_t)

  localparam int unsigned LogNrPorts = cf_math_pkg::idx_width(NrPorts);

  logic [NrPorts-1:0] req_valid_masked, req_ready_masked;

  req_chan_t [NrPorts-1:0] req_payload_q;
  logic [NrPorts-1:0] req_valid_q, req_ready_q;

  // Unforunately we need this signal otherwise the simulator complains about
  // multiple driven signals, because some other signals are driven from an
  // `always_comb` block.
  logic [NrPorts-1:0] slv_rsp_q_ready;

  // Optionially cut the incoming path
  for (genvar i = 0; i < NrPorts; i++) begin : gen_cuts
    spill_register #(
      .T (req_chan_t),
      .Bypass (!RegisterReq[i])
    ) i_spill_register_req (
      .clk_i,
      .rst_ni,
      .valid_i  (slv_req_i[i].q_valid ),
      .ready_o  (slv_rsp_q_ready[i]   ),
      .data_i   (slv_req_i[i].q       ),
      .valid_o  (req_valid_q[i]       ),
      .ready_i  (req_ready_masked[i]  ),
      .data_o   (req_payload_q[i]     )
    );
  end

  // We need to silence the handshake in case the fifo is full and we can't
  // accept more transactions.
  for (genvar i = 0; i < NrPorts; i++) begin : gen_req_valid_masked
    assign req_valid_masked[i] = req_valid_q[i];
    assign req_ready_masked[i] = req_ready_q[i];
  end

  /// Arbitrate on instruction request port
  rr_arb_tree #(
    .NumIn (NrPorts),
    .DataType (req_chan_t),
    .AxiVldRdy (1'b1),
    .LockIn (1'b1)
  ) i_q_mux (
    .clk_i,
    .rst_ni,
    .flush_i  (1'b0             ),
    .rr_i     ('0               ),
    .req_i    (req_valid_masked ),
    .gnt_o    (req_ready_q      ),
    .data_i   (req_payload_q    ),
    .gnt_i    (mst_rsp_i.q_ready),
    .req_o    (mst_req_o.q_valid),
    .data_o   (mst_req_o.q      ),
    .idx_o    (idx_o            )
  );

  // Output Mux
  always_comb begin
    for (int i = 0; i < NrPorts; i++) begin
      slv_rsp_o[i].p_valid = '0;
      slv_rsp_o[i].q_ready = slv_rsp_q_ready[i];
      slv_rsp_o[i].p = mst_rsp_i.p;
    end
    slv_rsp_o[idx_i].p_valid = mst_rsp_i.p_valid;
  end

  assign mst_req_o.p_ready = slv_req_i[idx_i].p_ready;

endmodule
