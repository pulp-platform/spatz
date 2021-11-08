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

	timeunit			1ns;
	timeprecision	1ps;

	localparam 		ClockPeriod = 1ns;

	logic clk;
	logic rst_n;

	always #(ClockPeriod/2) clk = !clk;

	initial begin
		clk 	= 1'b1;
		rst_n = 1'b0;

		repeat (5)
			#(ClockPeriod);

		#(ClockPeriod/4);
		rst_n = 1'b1;

		@(posedge clk);
		repeat (20)
			#(ClockPeriod);

		$finish;
	end

	///////////
	// Spatz //
	///////////

	riscv_pkg::instr_t instr;
	logic instr_valid;
	spatz_pkg::elen_t rs1;
	spatz_pkg::elen_t rs2;
	spatz_pkg::elen_t rd;

	spatz dut (
		.clk_i					(clk),
		.rst_ni					(rst_n),
		.instr_i        (instr),
		.instr_illegal_o(),
		.instr_valid_i  (instr_valid),
		.rs1_i          (rs1),
		.rs2_i 					(rs2),
		.rd_o           (rd)
	);

	initial begin
		instr = '0;
		instr_valid = '0;
		rs1 = '0;
		rs2 = '0;

		wait (rst_n == 1'b1);
		@(posedge clk);

		instr_valid = '1;

		// vl_exp = 64 (e8, m4)
		instr = 32'h0c257557;
		rs1 = 32'd128;

		@(posedge clk);

		// check vl for 64 length
		instr = 32'hC2012173;
		rs1 = 32'd0;
		@(negedge clk);
		assert (rd == 32'd64);

		@(posedge clk);

		// vl_exp stays the same, but ratio changes (e16, m2) -> illegal
		instr = 32'h00907057;
		rs1 = 32'd128;

		@(posedge clk);

		// check vl for zero length
		instr = 32'hC2012173;
		rs1 = 32'd0;
		@(negedge clk);
		assert (rd == 0);

		@(posedge clk);

		// vl_exp = 128 (e8, m4)
		instr = 32'h08207557;
		rs1 = 32'd128;

		@(posedge clk);

		// vl_exp stays the same, ratio stays the same (e16, m8)
		instr = 32'h00B07057;
		rs1 = 32'd128;

		@(posedge clk);

		// check vtype for (vm0, vt0, e16, m8) configuration
		instr = 32'hC2112173;
		rs1 = 32'd0;
		@(negedge clk);
		assert (rd == 32'(8'b00001011));

		@(posedge clk);

		// check vlenb
		instr = 32'hC2212173;
		rs1 = 32'd0;
		@(negedge clk);
		assert (rd == spatz_pkg::VLENB);

		@(posedge clk);

		// set vstart to 6 (over immediate) and check vstart for 0
		instr = 32'h00836173;
		rs1 = 32'd1;
		@(negedge clk);
		assert (rd == 0);

		@(posedge clk);

		// set vstart to 4 and check vstart for 6
		instr = 32'h00811173;
		rs1 = 32'd4;
		@(negedge clk);
		assert (rd == 6);

		@(posedge clk);

		// check vstart for 4
		instr = 32'h00812173;
		rs1 = 32'd0;
		@(negedge clk);
		assert (rd == 4);
	end

endmodule : spatz_tb
