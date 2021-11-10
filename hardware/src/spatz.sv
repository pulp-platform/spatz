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

  /////////////
  // Signals //
  /////////////

  // CSR registers
  vlen_t  vstart;
  vlen_t  vl;
  vtype_t vtype;

  // Decoder req
  decoder_req_t decoder_req;
  logic         decoder_req_valid;
  // Decoder rsp
  decoder_rsp_t decoder_rsp;
  logic         decoder_rsp_valid;

  // Spatz req
  spatz_req_t spatz_req;
  logic       spatz_req_valid;

  //////////
  // CSRs //
  //////////

  spatz_vcsr i_vcsr (
    .clk_i            (clk_i),
    .rst_ni           (rst_ni),
    // VCSR req
    .vcsr_req_valid_i (spatz_req_valid),
    .vcsr_req_i       (spatz_req),
    // CSR register signals
    .vtype_o          (vtype),
    .vl_o             (vl),
    .vstart_o         (vstart)
  );

  ////////////////
  // Controller //
  ////////////////

  spatz_controller i_controller (
    .clk_i              (clk_i),
    .rst_ni             (rst_ni),
    // X-intf
    .x_issue_valid_i    (x_issue_valid_i),
    .x_issue_ready_o    (x_issue_ready_o),
    .x_issue_req_i      (x_issue_req_i),
    .x_issue_resp_o     (x_issue_resp_o),
    .x_result_valid_o   (x_result_valid_o),
    .x_result_ready_i   (x_result_ready_i),
    .x_result_o         (x_result_o),
    // Decoder
    .decoder_req_valid_o(decoder_req_valid),
    .decoder_req_o      (decoder_req),
    .decoder_rsp_valid_i(decoder_rsp_valid),
    .decoder_rsp_i      (decoder_rsp),
    // CSRs
    .vstart_i           (vstart),
    .vl_i               (vl),
    .vtype_i            (vtype),
    // Spatz req
    .spatz_req_valid_o  (spatz_req_valid),
    .spatz_req_o        (spatz_req)
  );

  /////////////
  // Decoder //
  /////////////

  spatz_decoder i_decoder (
    .clk_i (clk_i),
    .rst_ni(rst_ni),
    // Request
    .decoder_req_i      (decoder_req),
    .decoder_req_valid_i(decoder_req_valid),
    // Response
    .decoder_rsp_o      (decoder_rsp),
    .decoder_rsp_valid_o(decoder_rsp_valid)
  );

endmodule : spatz
