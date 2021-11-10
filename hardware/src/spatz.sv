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

  // Spatz req
  spatz_req_t spatz_req;
  logic       spatz_req_valid;

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
    // Spatz req
    .spatz_req_valid_o  (spatz_req_valid),
    .spatz_req_o        (spatz_req)
  );

endmodule : spatz
