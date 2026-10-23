// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Thomas Benz <tbenz@iis.ee.ethz.ch>
// Author: Florian Zaruba <zarubaf@iis.ee.ethz.ch>

`include "common_cells/assertions.svh"

/// This module multiplexes many narrow ports, one wide port, and one external port
/// onto many narrow ports. Arbitration is done statically: the wide port has the
/// highest priority, followed by the external port, and finally the narrow ports.
///
/// ## Constraint
///
/// The data and strb width of the wide port must be evenly divisible by the
/// small port data and strb width.
///
/// ## Caution
///
/// As of now this module's request always need an immediate grant in case
/// `in_wide_req_i.q_valid` is high. Any delayed grant will break the module's behavior.
module mem_wide_narrow_mux #(
  /// Width of narrow data.
  parameter int unsigned NarrowDataWidth = 0,
  /// Width of wide data.
  parameter int unsigned WideDataWidth   = 0,
  /// Latency of upstream memory port.
  parameter int unsigned MemoryLatency   = 1,
  /// Request type of narrow inputs.
  parameter type mem_narrow_req_t        = logic,
  /// Response type of narrow inputs.
  parameter type mem_narrow_rsp_t        = logic,
  /// Request type of wide inputs.
  parameter type mem_wide_req_t          = logic,
  /// Response type of wide inputs.
  parameter type mem_wide_rsp_t          = logic,
  /// Derived. *Do not override*
  /// Number of narrow inputs.
  parameter int unsigned NrPorts = WideDataWidth / NarrowDataWidth
) (
  input  logic                          clk_i,
  input  logic                          rst_ni,
  // Inputs
  /// Narrow side.
  input  mem_narrow_req_t [NrPorts-1:0] in_narrow_req_i,
  output mem_narrow_rsp_t [NrPorts-1:0] in_narrow_rsp_o,
  /// Wide side.
  input  mem_wide_req_t                 in_wide_req_i,
  output mem_wide_rsp_t                 in_wide_rsp_o,
  /// External side.
  input  mem_wide_req_t                 in_ext_req_i,
  output mem_wide_rsp_t                 in_ext_rsp_o,
  // Multiplexed output.
  output mem_narrow_req_t [NrPorts-1:0] out_req_o,
  input  mem_narrow_rsp_t [NrPorts-1:0] out_rsp_i
);

  localparam int unsigned NarrowStrbWidth = NarrowDataWidth/8;

  always_comb begin
    // ----------------
    // Backward Channel
    // ----------------

    // Map the response from each narrow output port to the respective narrow
    // input port.
    in_narrow_rsp_o = out_rsp_i;

    // Map the response from each narrow output port to the respective narrow
    // word within the wide input port.
    for (int i = 0; i < NrPorts; i++) begin
      in_wide_rsp_o.p[i*NarrowDataWidth+:NarrowDataWidth] = out_rsp_i[i].p.data;
      in_ext_rsp_o.p[i*NarrowDataWidth+:NarrowDataWidth]  = out_rsp_i[i].p.data;
    end

    // ---------------
    // Forward Channel
    // ---------------

    // Select narrow requests by default.
    out_req_o = in_narrow_req_i;

    // Tie-off wide and ext by default.
    in_wide_rsp_o.q_ready = 1'b0;
    in_ext_rsp_o.q_ready  = 1'b0;

    // The wide port has the highest priority
    if (in_wide_req_i.q_valid) begin
      for (int i = 0; i < NrPorts; i++) begin
        out_req_o[i].q_valid = in_wide_req_i.q_valid;
        // Block access from narrow ports.
        in_narrow_rsp_o[i].q_ready = 1'b0;
        out_req_o[i].q = '{
          addr: in_wide_req_i.q.addr,
          write: in_wide_req_i.q.write,
          amo: reqrsp_pkg::AMONone,
          data: in_wide_req_i.q.data[i*NarrowDataWidth+:NarrowDataWidth],
          strb: in_wide_req_i.q.strb[i*NarrowStrbWidth+:NarrowStrbWidth],
          user: in_wide_req_i.q.user
        };
        // The protocol requires that the response is always granted
        // immediately (at least when `in_wide_req_i.q_valid` is high).
        in_wide_rsp_o.q_ready = 1'b1;
      end
    end else if (in_ext_req_i.q_valid) begin // The ext port has the second highest priority
      for (int i = 0; i < NrPorts; i++) begin
        out_req_o[i].q_valid = in_ext_req_i.q_valid;
        // Block access from narrow ports.
        in_narrow_rsp_o[i].q_ready = 1'b0;
        out_req_o[i].q = '{
          addr: in_ext_req_i.q.addr,
          write: in_ext_req_i.q.write,
          amo: reqrsp_pkg::AMONone,
          data: in_ext_req_i.q.data[i*NarrowDataWidth+:NarrowDataWidth],
          strb: in_ext_req_i.q.strb[i*NarrowStrbWidth+:NarrowStrbWidth],
          user: in_ext_req_i.q.user
        };

        in_ext_rsp_o.q_ready = 1'b1;
      end
    end
  end

  // ----------
  // Assertions
  // ----------
  `ASSERT_INIT(DataDivisible, WideDataWidth % NarrowDataWidth == 0)

  // Currently the module has a couple of `quirks` and interface requirements
  // which are checked here.
  logic [NrPorts-1:0] q_valid_flat;
  logic [NrPorts-1:0][NarrowDataWidth-1:0] q_data;
  logic [NrPorts-1:0][NarrowStrbWidth-1:0] q_strb;
  // verilog_lint: waive-start line-length
  `ASSERT(ImmediateGrantWide, in_wide_req_i.q_valid |-> in_wide_rsp_o.q_ready)
  for (genvar i = 0; i < NrPorts; i++) begin : gen_per_port
    assign q_valid_flat[i] = out_req_o[i].q_valid;
    assign q_data[i] = out_req_o[i].q.data;
    assign q_strb[i] = out_req_o[i].q.strb;
    `ASSERT(ImmediateGrantOut, in_wide_req_i.q_valid & out_req_o[i].q_valid |-> out_rsp_i[i].q_ready)
    `ASSERT(SilentNarrow, in_wide_req_i.q_valid |-> !in_narrow_rsp_o[i].q_ready)
    `ASSERT(NarrowPassThrough, !in_wide_req_i.q_valid & in_narrow_req_i[i].q_valid |-> out_req_o[i].q_valid)
  end
  `ASSERT(DmaSelected, in_wide_req_i.q_valid & in_wide_req_i.q_valid |-> &q_valid_flat)
  `ASSERT(DmaSelectedReadyWhenValid,
    in_wide_req_i.q_valid & in_wide_req_i.q_valid |-> in_wide_rsp_o.q_ready)
  `ASSERT(DmaWriteDataCorrect,
    in_wide_req_i.q_valid & in_wide_rsp_o.q_ready |->
    (in_wide_req_i.q.data === q_data) && (in_wide_req_i.q.strb === q_strb))
  // verilog_lint: waive-stop line-length

endmodule
