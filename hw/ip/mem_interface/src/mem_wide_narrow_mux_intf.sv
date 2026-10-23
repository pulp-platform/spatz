// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Danilo Cammarata <dcammarata@iis.ee.ethz.ch>

`include "mem_interface/typedef.svh"
`include "mem_interface/assign.svh"

/// Interface wrapper.
module mem_wide_narrow_mux_intf #(
  /// Address width of wide and narrow channel.
  parameter int unsigned AddrWidth        = 0,
  /// Width of narrow data.
  parameter int unsigned NarrowDataWidth  = 0,
  /// Address width of wide channel.
  parameter int unsigned WideDataWidth    = 0,
  /// User type of `mem` channel.
  parameter type         user_t           = logic,
  /// Latency of upstream memory port.
  parameter int unsigned MemoryLatency    = 1,
  /// Derived. *Do not override*
  /// Number of narrow inputs.
  parameter int unsigned NrPorts = WideDataWidth / NarrowDataWidth
) (
  input  logic clk_i,
  input  logic rst_ni,
  // Inputs
  /// Narrow side.
  MEM_BUS      in_narrow [NrPorts],
  MEM_BUS      in_wide,
  MEM_BUS      in_ext,
  // Output
  MEM_BUS      out [NrPorts]
);

  typedef logic [AddrWidth-1:0] addr_t;
  typedef logic [NarrowDataWidth-1:0] narrow_data_t;
  typedef logic [NarrowDataWidth/8-1:0] narrow_strb_t;
  typedef logic [WideDataWidth-1:0] wide_data_t;
  typedef logic [WideDataWidth/8-1:0] wide_strb_t;

  `MEM_TYPEDEF_ALL(mem_narrow, addr_t, narrow_data_t, narrow_strb_t, user_t)
  `MEM_TYPEDEF_ALL(mem_wide, addr_t, wide_data_t, wide_strb_t, user_t)

  mem_narrow_req_t [NrPorts-1:0] in_narrow_req;
  mem_narrow_rsp_t [NrPorts-1:0] in_narrow_rsp;
  mem_wide_req_t in_wide_req;
  mem_wide_rsp_t in_wide_rsp;
  mem_wide_req_t in_ext_req;
  mem_wide_rsp_t in_ext_rsp;

  mem_narrow_req_t [NrPorts-1:0] out_req;
  mem_narrow_rsp_t [NrPorts-1:0] out_rsp;

  mem_wide_narrow_mux #(
    .NarrowDataWidth (NarrowDataWidth),
    .WideDataWidth (WideDataWidth),
    .NrPorts (NrPorts),
    .MemoryLatency (MemoryLatency),
    .mem_narrow_req_t (mem_narrow_req_t),
    .mem_narrow_rsp_t (mem_narrow_rsp_t),
    .mem_wide_req_t (mem_wide_req_t),
    .mem_wide_rsp_t (mem_wide_rsp_t)
  ) i_mem_wide_narrow_mux (
    .clk_i (clk_i),
    .rst_ni (rst_ni),
    .in_narrow_req_i (in_narrow_req),
    .in_narrow_rsp_o (in_narrow_rsp),
    .in_wide_req_i (in_wide_req),
    .in_wide_rsp_o (in_wide_rsp),
    .in_ext_req_i (in_ext_req),
    .in_ext_rsp_o (in_ext_rsp),
    .out_req_o (out_req),
    .out_rsp_i (out_rsp)
  );

  for (genvar i = 0; i < NrPorts; i++) begin : gen_interface_assign
    `MEM_ASSIGN_TO_REQ(in_narrow_req[i], in_narrow[i])
    `MEM_ASSIGN_FROM_RESP(in_narrow[i], in_narrow_rsp[i])
    `MEM_ASSIGN_FROM_REQ(out[i], out_req[i])
    `MEM_ASSIGN_TO_RESP(out_rsp[i], out[i])
  end

  `MEM_ASSIGN_TO_REQ(in_wide_req, in_wide)
  `MEM_ASSIGN_FROM_RESP(in_wide, in_wide_rsp)

  `MEM_ASSIGN_TO_REQ(in_ext_req, in_ext)
  `MEM_ASSIGN_FROM_RESP(in_ext, in_ext_rsp)

endmodule
