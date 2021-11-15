// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

/*import "DPI-C" function void read_elf (input string filename);
import "DPI-C" function byte get_section (output longint address, output longint len);
import "DPI-C" context function byte read_section(input longint address, inout byte buffer[]);*/

module spatz_tb;

  ////////////////
  // Parameters //
  ////////////////

  timeunit      1ns;
  timeprecision 1ps;

  localparam ClockPeriod = 1ns;

  logic clk;
  logic rst_n;

  always #(ClockPeriod/2) clk = !clk;

  initial begin
    clk   = 1'b1;
    rst_n = 1'b0;

    repeat (5)
      #(ClockPeriod);

    #(ClockPeriod/4);
    rst_n = 1'b1;

    @(posedge clk);
    repeat (200)
      #(ClockPeriod);

    $finish;
  end

  ///////////
  // Spatz //
  ///////////

  // X-Intf signals
  logic x_issue_valid;
  logic x_issue_ready;
  core_v_xif_pkg::x_issue_req_t  x_issue_req;
  core_v_xif_pkg::x_issue_resp_t x_issue_resp;
  logic x_result_valid;
  logic x_result_ready;
  core_v_xif_pkg::x_result_t x_result;

  spatz dut (
    .clk_i           (clk),
    .rst_ni          (rst_n),
    // X-Interface
    .x_issue_valid_i (x_issue_valid),
    .x_issue_ready_o (x_issue_ready),
    .x_issue_req_i   (x_issue_req),
    .x_issue_resp_o  (x_issue_resp),
    .x_result_valid_o(x_result_valid),
    .x_result_ready_i(x_result_ready),
    .x_result_o      (x_result)
  );

  ///////////////////////
  // Instruction Issue //
  ///////////////////////

  initial begin
    x_issue_req = '0;
    x_issue_valid = '0;
    x_result_ready = '0;

    wait (rst_n == 1'b1);
    @(posedge clk);

    x_issue_valid = '1;
    x_result_ready = '1;

    // vl_exp = 128 (e8, m4)
    x_issue_req.instr = 32'h0c257557;
    x_issue_req.rs[0] = 32'd256;
    @(negedge clk);
    assert (x_result.data == 32'd128);

    @(posedge clk);

    // check vl for 128 length
    x_issue_req.instr = 32'hC2012173;
    x_issue_req.rs[0] = 32'd0;
    @(negedge clk);
    assert (x_result.data == 32'd128);

    @(posedge clk);

    // vl_exp stays the same, but ratio changes (e16, m2) -> illegal
    x_issue_req.instr = 32'h00907057;
    x_issue_req.rs[0] = 32'd128;

    @(posedge clk);

    // check vl for zero length
    x_issue_req.instr = 32'hC2012173;
    x_issue_req.rs[0] = 32'd0;
    @(negedge clk);
    assert (x_result.data == 0);

    @(posedge clk);

    // vl_exp = 256 (e8, m4)
    x_issue_req.instr = 32'h08207557;
    x_issue_req.rs[0] = 32'd256;
    @(negedge clk);
    assert (x_result.data == 32'd256);

    @(posedge clk);

    // vl_exp stays the same, ratio stays the same (e16, m8)
    x_issue_req.instr = 32'h00B07057;
    x_issue_req.rs[0] = 32'd256;
    @(negedge clk);
    assert (x_result.data == 32'd256);

    @(posedge clk);

    // check vtype for (vm0, vt0, e16, m8) configuration
    x_issue_req.instr = 32'hC2112173;
    x_issue_req.rs[0] = 32'd0;
    @(negedge clk);
    assert (x_result.data == 32'(8'b00001011));

    @(posedge clk);

    // check vlenb
    x_issue_req.instr = 32'hC2212173;
    x_issue_req.rs[0] = 32'd0;
    @(negedge clk);
    assert (x_result.data == spatz_pkg::VLENB);

    @(posedge clk);

    // set vstart to 6 (over immediate) and check vstart for 0
    x_issue_req.instr = 32'h00836173;
    x_issue_req.rs[0] = 32'd1;
    @(negedge clk);
    assert (x_result.data == 0);

    @(posedge clk);

    // set vstart to 4 and check vstart for 6
    x_issue_req.instr = 32'h00811173;
    x_issue_req.rs[0] = 32'd4;
    @(negedge clk);
    assert (x_result.data == 6);

    @(posedge clk);

    // check vstart for 4
    x_issue_req.instr = 32'h00812173;
    x_issue_req.rs[0] = 32'd0;
    @(negedge clk);
    assert (x_result.data == 4);

    @(posedge clk);

    // vl_exp = 128 (e8, m4)
    x_issue_req.instr = 32'h0c257557;
    x_issue_req.rs[0] = 32'd127;
    @(negedge clk);
    assert (x_result.data == 32'd127);

    @(posedge clk);

    // Execute Add instruction (vx)
    x_issue_req.instr = 32'h03F0C0D7;
    x_issue_req.rs[0] = 32'd15;

    @(posedge clk);

    // check illegal instruction
    x_issue_req.instr = 32'h00812174;
    x_issue_req.rs[0] = 32'd0;
    @(negedge clk);
    assert (x_issue_resp.exc == 1);

    @(posedge clk);

    x_issue_valid = 1'b0;
  end

endmodule : spatz_tb
