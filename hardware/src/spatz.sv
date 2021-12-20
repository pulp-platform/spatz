// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// This is the toplevel module of Spatz. It contains all other SPatz modules.
// This includes the Controller, which interfaces with the main core and handles
// instruction decoding, operation issuing to the other units, and result write
// back to the core. The Vector Function Unit (VFU) is the high high throughput
// unit that executes all arithmetic and logical operations. The Load/Store Unit
// (LSU) is used to load vectors from memory to the register file and store them
// back again. Finally, the Vector Register File (VRF) is the main register file
// that stores all of the currently used vectors close to the execution units.

module spatz
  import spatz_pkg::*;
  import rvv_pkg::*;
#(
  parameter int unsigned NR_MEM_PORTS = 1,
  parameter type x_issue_req_t        = logic,
  parameter type x_issue_resp_t       = logic,
  parameter type x_result_t           = logic,
  parameter type x_mem_req_t          = logic,
  parameter type x_mem_resp_t         = logic,
  parameter type x_mem_result_t       = logic
) (
  input  logic clk_i,
  input  logic rst_ni,
  // X-Interface Issue
  input  logic          x_issue_valid_i,
  output logic          x_issue_ready_o,
  input  x_issue_req_t  x_issue_req_i,
  output x_issue_resp_t x_issue_resp_o,
  // X-Interface Result
  output logic      x_result_valid_o,
  input  logic      x_result_ready_i,
  output x_result_t x_result_o,
  // X-Interface Memory Request
  output logic          [NR_MEM_PORTS-1:0] x_mem_valid_o,
  input  logic          [NR_MEM_PORTS-1:0] x_mem_ready_i,
  output x_mem_req_t    [NR_MEM_PORTS-1:0] x_mem_req_o,
  input  x_mem_resp_t   [NR_MEM_PORTS-1:0] x_mem_resp_i,
  //X-Interface Memory Result
  input  logic          [NR_MEM_PORTS-1:0] x_mem_result_valid_i,
  input  x_mem_result_t [NR_MEM_PORTS-1:0] x_mem_result_i,
  // X-Interface Memory Finished
  output logic x_mem_finished_o
);

  ////////////////
  // Parameters //
  ////////////////

  // Number of ports of the vector register file
  localparam NrWritePorts = 3;
  localparam NrReadPorts  = 5;

  /////////////
  // Signals //
  /////////////

  // Spatz req
  spatz_req_t spatz_req;
  logic       spatz_req_valid;

  logic       vfu_req_ready;
  logic       vfu_rsp_valid;
  vfu_rsp_t   vfu_rsp;

  logic       vlsu_req_ready;
  logic       vlsu_rsp_valid;
  vlsu_rsp_t  vlsu_rsp;

  /////////
  // VRF //
  /////////

  // Write ports
  vreg_addr_t [NrWritePorts-1:0] vrf_waddr;
  vreg_data_t [NrWritePorts-1:0] vrf_wdata;
  logic       [NrWritePorts-1:0] vrf_we;
  vreg_be_t   [NrWritePorts-1:0] vrf_wbe;
  logic       [NrWritePorts-1:0] vrf_wvalid;
  // Read ports
  vreg_addr_t [NrReadPorts-1:0] vrf_raddr;
  logic       [NrReadPorts-1:0] vrf_re;
  vreg_data_t [NrReadPorts-1:0] vrf_rdata;
  logic       [NrReadPorts-1:0] vrf_rvalid;

  spatz_vrf #(
    .NR_READ_PORTS (NrReadPorts),
    .NR_WRITE_PORTS(NrWritePorts)
  ) i_vrf (
    .clk_i   (clk_i),
    .rst_ni  (rst_ni),
    // Write Ports
    .waddr_i (vrf_waddr),
    .wdata_i (vrf_wdata),
    .we_i    (vrf_we),
    .wbe_i   (vrf_wbe),
    .wvalid_o(vrf_wvalid),
    // Read Ports
    .raddr_i (vrf_raddr),
    .re_i    (vrf_re),
    .rdata_o (vrf_rdata),
    .rvalid_o(vrf_rvalid)
  );

  ////////////////
  // Controller //
  ////////////////

  // Scoreboard read enable and write enable input signals
  logic [NrReadPorts-1:0]  sb_re;
  logic [NrWritePorts-1:0] sb_we;

  spatz_controller #(
    .NrVregfilePorts (NrReadPorts+NrWritePorts),
    .x_issue_req_t   (x_issue_req_t),
    .x_issue_resp_t  (x_issue_resp_t),
    .x_result_t      (x_result_t)
  ) i_controller (
    .clk_i            (clk_i),
    .rst_ni           (rst_ni),
    // X-intf
    .x_issue_valid_i  (x_issue_valid_i),
    .x_issue_ready_o  (x_issue_ready_o),
    .x_issue_req_i    (x_issue_req_i),
    .x_issue_resp_o   (x_issue_resp_o),
    .x_result_valid_o (x_result_valid_o),
    .x_result_ready_i (x_result_ready_i),
    .x_result_o       (x_result_o),
    // Spatz req
    .spatz_req_valid_o(spatz_req_valid),
    .spatz_req_o      (spatz_req),
    // VFU
    .vfu_req_ready_i  (vfu_req_ready),
    .vfu_rsp_valid_i  (vfu_rsp_valid),
    .vfu_rsp_i        (vfu_rsp),
    // VFU
    .vlsu_req_ready_i (vlsu_req_ready),
    .vlsu_rsp_valid_i (vlsu_rsp_valid),
    .vlsu_rsp_i       (vlsu_rsp),
    // Scoreboard check
    .sb_addr_i        ({vrf_waddr, vrf_raddr}),
    .sb_enable_i      ({sb_we, sb_re}),
    .sb_enable_o      ({vrf_we, vrf_re})
  );

  /////////
  // VFU //
  /////////

  spatz_vfu i_vfu (
    .clk_i            (clk_i),
    .rst_ni           (rst_ni),
    // Request
    .spatz_req_i      (spatz_req),
    .spatz_req_valid_i(spatz_req_valid),
    .spatz_req_ready_o(vfu_req_ready),
    // Response
    .vfu_rsp_valid_o  (vfu_rsp_valid),
    .vfu_rsp_o        (vfu_rsp),
    // VRF
    .vrf_waddr_o      (vrf_waddr[0]),
    .vrf_wdata_o      (vrf_wdata[0]),
    .vrf_we_o         (sb_we[0]),
    .vrf_wbe_o        (vrf_wbe[0]),
    .vrf_wvalid_i     (vrf_wvalid[0]),
    .vrf_raddr_o      (vrf_raddr[2:0]),
    .vrf_re_o         (sb_re[2:0]),
    .vrf_rdata_i      (vrf_rdata[2:0]),
    .vrf_rvalid_i     (vrf_rvalid[2:0])
  );

  //////////
  // VLSU //
  //////////

  spatz_vlsu #(
    .NR_MEM_PORTS  (NR_MEM_PORTS),
    .x_mem_req_t   (x_mem_req_t),
    .x_mem_resp_t  (x_mem_resp_t),
    .x_mem_result_t(x_mem_result_t)
  ) i_vlsu (
    .clk_i            (clk_i),
    .rst_ni           (rst_ni),
    // Request
    .spatz_req_i      (spatz_req),
    .spatz_req_valid_i(spatz_req_valid),
    .spatz_req_ready_o(vlsu_req_ready),
    // Response
    .vlsu_rsp_valid_o (vlsu_rsp_valid),
    .vlsu_rsp_o       (vlsu_rsp),
    // VRF
    .vrf_waddr_o      (vrf_waddr[1]),
    .vrf_wdata_o      (vrf_wdata[1]),
    .vrf_we_o         (sb_we[1]),
    .vrf_wbe_o        (vrf_wbe[1]),
    .vrf_wvalid_i     (vrf_wvalid[1]),
    .vrf_raddr_o      (vrf_raddr[3]),
    .vrf_re_o         (sb_re[3]),
    .vrf_rdata_i      (vrf_rdata[3]),
    .vrf_rvalid_i     (vrf_rvalid[3]),
    // X-Interface Memory
    .x_mem_valid_o       (x_mem_valid_o),
    .x_mem_ready_i       (x_mem_ready_i),
    .x_mem_req_o         (x_mem_req_o),
    .x_mem_resp_i        (x_mem_resp_i),
    .x_mem_result_valid_i(x_mem_result_valid_i),
    .x_mem_result_i      (x_mem_result_i),
    .x_mem_finished_o    (x_mem_finished_o)
  );

  //////////
  // VSLD //
  //////////

  assign vrf_waddr[2] = '0;
  assign vrf_wdata[2] = '0;
  assign sb_we[2]    = '0;
  assign vrf_wbe[2]   = '0;
  assign vrf_raddr[4] = '0;
  assign sb_re[4]    = '0;

  ////////////////
  // Assertions //
  ////////////////

  if (spatz_pkg::N_IPU <= 1)
    $error("[spatz] The number of IPUs needs to be at least two.");

  if (spatz_pkg::N_IPU != 2**$clog2(spatz_pkg::N_IPU))
    $error("[spatz] The number of IPUs needs to be a power of two");

  if (spatz_pkg::ELEN > spatz_pkg::VLEN)
    $error("[spatz] The size of one element can not exceede the length of one vector register.");

  if (NR_MEM_PORTS == 0)
    $error("[spatz] Spatz requires at least one memory port.");

endmodule : spatz
