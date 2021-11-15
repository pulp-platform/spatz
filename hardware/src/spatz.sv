// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz
  import spatz_pkg::*;
  import rvv_pkg::*;
(
  input  logic clk_i,
  input  logic rst_ni,
  // X-Interface Issue
  input  logic x_issue_valid_i,
  output logic x_issue_ready_o,
  input  core_v_xif_pkg::x_issue_req_t  x_issue_req_i,
  output core_v_xif_pkg::x_issue_resp_t x_issue_resp_o,
  // X-Interface Result
  output logic x_result_valid_o,
  input  logic x_result_ready_i,
  output core_v_xif_pkg::x_result_t x_result_o
);

  // Include FF
  `include "common_cells/registers.svh"

  ////////////////
  // Parameters //
  ////////////////

  localparam NrWritePorts = 1;
  localparam NrReadPorts  = 3;

  /////////////
  // Signals //
  /////////////

  // Spatz req
  spatz_req_t spatz_req;
  logic       spatz_req_valid;

  logic       vfu_req_ready;
  logic       vfu_rsp_valid;
  vfu_rsp_t   vfu_rsp;

  ////////////////
  // Controller //
  ////////////////

  spatz_controller i_controller (
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
    .vfu_rsp_i        (vfu_rsp)
  );

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
    .vfu_rsp_valid_o  (vfr_rsp_valid),
    .vfu_rsp_o        (vfu_rsp),
    // VRF
    .vrf_waddr_o      (vrf_waddr[0]),
    .vrf_wdata_o      (vrf_wdata[0]),
    .vrf_we_o         (vrf_we[0]),
    .vrf_wbe_o        (vrf_wbe[0]),
    .vrf_wvalid_i     (vrf_wvalid[0]),
    .vrf_raddr_o      (vrf_raddr[2:0]),
    .vrf_re_o         (vrf_re[2:0]),
    .vrf_rdata_i      (vrf_rdata[2:0]),
    .vrf_rvalid_i     (vrf_rvalid[2:0])
  );

endmodule : spatz
